#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "pagesim.h"
#include "builtin.h"
#include "parser.h"

/* ------------------------------------------------------------
   Helper: parse a line of page numbers into an array.
   Supports two formats:
     - Space-separated:  "7 0 1 2 0 3"
     - No-space digits:  "7012030"  (each digit = one page)
   Returns the number of pages parsed.
   ------------------------------------------------------------ */
static int parse_refs(const char *line, int **out_refs)
{
    int *refs = NULL;
    int count = 0;

    /* Check if there are any spaces in the non-newline part */
    int has_space = 0;
    for (const char *c = line; *c && *c != '\n'; c++) {
        if (isspace((unsigned char)*c)) { has_space = 1; break; }
    }

    if (has_space) {
        /* Space-separated parsing */
        const char *p = line;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            char *end;
            long val = strtol(p, &end, 10);
            if (p == end) break;
            refs = realloc(refs, (count + 1) * sizeof(int));
            refs[count++] = (int)val;
            p = end;
        }
    } else {
        /* No spaces: treat each digit as a separate page number */
        for (const char *p = line; *p; p++) {
            if (isdigit((unsigned char)*p)) {
                refs = realloc(refs, (count + 1) * sizeof(int));
                refs[count++] = *p - '0';
            }
        }
    }

    *out_refs = refs;
    return count;
}

/* ------------------------------------------------------------
   Result struct: holds both hits and faults
   ------------------------------------------------------------ */
typedef struct {
    int hits;
    int faults;
} sim_result_t;

/* ------------------------------------------------------------
   FIFO page-replacement algorithm.
   ------------------------------------------------------------ */
static sim_result_t fifo(int *refs, int n, int frames)
{
    int *frame  = calloc(frames, sizeof(int));
    int pos     = 0;
    int filled  = 0;
    sim_result_t res = {0, 0};

    for (int i = 0; i < n; ++i) {
        int page  = refs[i];
        int found = 0;
        for (int j = 0; j < filled; ++j) {
            if (frame[j] == page) { found = 1; break; }
        }
        if (found) {
            res.hits++;
        } else {
            res.faults++;
            frame[pos] = page;
            pos = (pos + 1) % frames;
            if (filled < frames) filled++;
        }
    }
    free(frame);
    return res;
}

/* ------------------------------------------------------------
   LRU page-replacement algorithm.
   ------------------------------------------------------------ */
static sim_result_t lru(int *refs, int n, int frames)
{
    int *frame  = calloc(frames, sizeof(int));
    int *age    = calloc(frames, sizeof(int)); /* larger = less recently used */
    int filled  = 0;
    sim_result_t res = {0, 0};

    for (int i = 0; i < n; ++i) {
        int page = refs[i];
        int idx  = -1;
        for (int j = 0; j < filled; ++j) {
            if (frame[j] == page) { idx = j; break; }
        }
        if (idx != -1) {
            res.hits++;
            age[idx] = 0;
        } else {
            res.faults++;
            if (filled < frames) {
                idx = filled;
                frame[idx] = page;
                filled++;
            } else {
                /* Find LRU victim (max age) */
                int max_age = -1, victim = 0;
                for (int j = 0; j < frames; ++j) {
                    if (age[j] > max_age) { max_age = age[j]; victim = j; }
                }
                idx = victim;
                frame[idx] = page;
            }
            age[idx] = 0;
        }
        /* Increment age of all other frames */
        for (int j = 0; j < filled; ++j) {
            if (j != idx) age[j]++;
        }
    }
    free(frame);
    free(age);
    return res;
}

/* ------------------------------------------------------------
   Optimal page-replacement algorithm.
   ------------------------------------------------------------ */
static sim_result_t optimal(int *refs, int n, int frames)
{
    int *frame = calloc(frames, sizeof(int));
    int filled = 0;
    sim_result_t res = {0, 0};

    for (int i = 0; i < n; ++i) {
        int page    = refs[i];
        int present = 0;
        for (int j = 0; j < filled; ++j) {
            if (frame[j] == page) { present = 1; break; }
        }
        if (present) {
            res.hits++;
            continue;
        }
        res.faults++;
        if (filled < frames) {
            frame[filled++] = page;
        } else {
            /* Victim: page whose next use is farthest (or never) */
            int farthest = -1, victim = 0;
            for (int j = 0; j < frames; ++j) {
                int next = n; /* never used again */
                for (int k = i + 1; k < n; ++k) {
                    if (refs[k] == frame[j]) { next = k; break; }
                }
                if (next > farthest) { farthest = next; victim = j; }
            }
            frame[victim] = page;
        }
    }
    free(frame);
    return res;
}

/* ------------------------------------------------------------
   LFU page-replacement algorithm.
   Least Frequently Used: replace the page used least often.
   Tie-break: replace the one that was loaded earliest (FIFO).
   ------------------------------------------------------------ */
static sim_result_t lfu(int *refs, int n, int frames)
{
    int *frame  = calloc(frames, sizeof(int));
    int *freq   = calloc(frames, sizeof(int)); /* usage frequency */
    int *load_t = calloc(frames, sizeof(int)); /* time of load (for tie-break) */
    int filled  = 0;
    sim_result_t res = {0, 0};

    for (int i = 0; i < n; ++i) {
        int page = refs[i];
        int idx  = -1;

        /* Check if page is already in a frame */
        for (int j = 0; j < filled; ++j) {
            if (frame[j] == page) { idx = j; break; }
        }

        if (idx != -1) {
            /* Page hit */
            res.hits++;
            freq[idx]++;
        } else {
            /* Page fault */
            res.faults++;
            if (filled < frames) {
                /* Empty frame available */
                idx = filled;
                frame[idx]  = page;
                freq[idx]   = 1;
                load_t[idx] = i;
                filled++;
            } else {
                /* Find victim: lowest frequency, tie-break by earliest load */
                int min_freq = freq[0], victim = 0;
                for (int j = 1; j < frames; ++j) {
                    if (freq[j] < min_freq ||
                       (freq[j] == min_freq && load_t[j] < load_t[victim])) {
                        min_freq = freq[j];
                        victim   = j;
                    }
                }
                frame[victim]  = page;
                freq[victim]   = 1;
                load_t[victim] = i;
            }
        }
    }
    free(frame);
    free(freq);
    free(load_t);
    return res;
}

/* ------------------------------------------------------------
   Print result with hits and faults
   ------------------------------------------------------------ */
static void print_result(const char *algo, sim_result_t r, int n)
{
    printf("\n========== %s Result ==========\n", algo);
    printf("  Total references : %d\n", n);
    printf("  Page Hits        : %d\n", r.hits);
    printf("  Page Faults      : %d\n", r.faults);
    printf("  Hit  Ratio       : %.2f%%\n", n > 0 ? (r.hits  * 100.0 / n) : 0);
    printf("  Fault Ratio      : %.2f%%\n", n > 0 ? (r.faults * 100.0 / n) : 0);
    printf("=====================================\n");
}

/* ------------------------------------------------------------
   Built-in command entry point.
   ------------------------------------------------------------ */
int builtin_pagesim(command_t *cmd)
{
    (void)cmd; /* interactive mode */
    int frames = 0;

    printf("Enter number of frames: ");
    fflush(stdout);
    if (scanf("%d", &frames) != 1 || frames <= 0) {
        fprintf(stderr, "pagesim: invalid frame count.\n");
        int c; while ((c = getchar()) != '\n' && c != EOF);
        return -1;
    }
    int c; while ((c = getchar()) != '\n' && c != EOF); /* consume newline */

    printf("Enter reference string (space-separated or no-space digits):\n");
    fflush(stdout);
    char line[1024];
    if (!fgets(line, sizeof(line), stdin)) {
        fprintf(stderr, "pagesim: failed to read reference string.\n");
        return -1;
    }

    int *refs = NULL;
    int n = parse_refs(line, &refs);
    if (n == 0) {
        fprintf(stderr, "pagesim: no page numbers provided.\n");
        free(refs);
        return -1;
    }

    /* -------------------------------------------------------
       Loop: keep asking for algorithm until user says no
       ------------------------------------------------------- */
    while (1)
    {
        printf("\nSelect algorithm:\n");
        printf("  1. FIFO    (First-In First-Out)\n");
        printf("  2. LRU     (Least Recently Used)\n");
        printf("  3. Optimal (Belady's Algorithm)\n");
        printf("  4. LFU     (Least Frequently Used)\n");
        printf("Choice: ");
        fflush(stdout);

        int choice = 0;
        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "pagesim: invalid choice.\n");
            free(refs);
            return -1;
        }
        while ((c = getchar()) != '\n' && c != EOF);

        sim_result_t result;
        switch (choice) {
            case 1: result = fifo   (refs, n, frames); print_result("FIFO",    result, n); break;
            case 2: result = lru    (refs, n, frames); print_result("LRU",     result, n); break;
            case 3: result = optimal(refs, n, frames); print_result("Optimal", result, n); break;
            case 4: result = lfu    (refs, n, frames); print_result("LFU",     result, n); break;
            default:
                fprintf(stderr, "pagesim: unknown selection '%d'.\n", choice);
                free(refs);
                return -1;
        }

        /* Ask if user wants to try another algorithm */
        printf("\nDo you want to check with another algorithm? (y/n): ");
        fflush(stdout);
        char ans[8];
        if (!fgets(ans, sizeof(ans), stdin)) break;
        if (ans[0] != 'y' && ans[0] != 'Y') {
            printf("Exiting pagesim. Goodbye!\n");
            break;
        }
    }

    free(refs);
    return 0;
}
