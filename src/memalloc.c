#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memalloc.h"
#include "builtin.h"
#include "parser.h"

typedef struct {
    int id;
    int size;
    int allocated_proc; // 0 if free, >0 PID
    int original_size;
} MemoryBlock;

typedef struct {
    int pid;
    int size;
    int block_assigned; // -1 if not allocated, block ID if allocated
} ProcRequest;

static void print_alloc_table(MemoryBlock *blocks, int num_blocks, ProcRequest *procs, int num_procs, const char *algo_name) {
    (void)num_blocks;
    printf("=====================================================\n");
    printf(" Contiguous Memory Allocation: %s\n", algo_name);
    printf("=====================================================\n");
    printf("Process  Size Requested  Status      Block Assigned  Block Size  Internal Frag\n");
    printf("-----------------------------------------------------------------------------\n");

    int total_internal_frag = 0;
    int allocated_procs = 0;

    for (int i = 0; i < num_procs; i++) {
        if (procs[i].block_assigned != -1) {
            int b_idx = procs[i].block_assigned;
            int frag = blocks[b_idx].original_size - procs[i].size;
            printf("P%-6d  %-14d  Allocated   Block %-8d  %-10d  %d KB\n",
                   procs[i].pid, procs[i].size, blocks[b_idx].id, blocks[b_idx].original_size, frag);
            total_internal_frag += frag;
            allocated_procs++;
        } else {
            printf("P%-6d  %-14d  NOT ALLOC   --              --          --\n",
                   procs[i].pid, procs[i].size);
        }
    }
    printf("-----------------------------------------------------------------------------\n");
    printf("Processes Allocated: %d / %d\n", allocated_procs, num_procs);
    printf("Total Internal Fragmentation: %d KB\n\n", total_internal_frag);
}

static void run_first_fit(MemoryBlock *orig_blocks, int nb, ProcRequest *orig_procs, int np, int quiet, int *out_alloc, int *out_frag) {
    MemoryBlock blocks[nb];
    ProcRequest procs[np];
    memcpy(blocks, orig_blocks, sizeof(MemoryBlock) * nb);
    memcpy(procs, orig_procs, sizeof(ProcRequest) * np);

    for (int i = 0; i < np; i++) {
        procs[i].block_assigned = -1;
        for (int j = 0; j < nb; j++) {
            if (blocks[j].allocated_proc == 0 && blocks[j].size >= procs[i].size) {
                blocks[j].allocated_proc = procs[i].pid;
                procs[i].block_assigned = j;
                break;
            }
        }
    }

    int alloc_cnt = 0, frag_sum = 0;
    for (int i = 0; i < np; i++) {
        if (procs[i].block_assigned != -1) {
            alloc_cnt++;
            frag_sum += blocks[procs[i].block_assigned].original_size - procs[i].size;
        }
    }

    if (out_alloc) *out_alloc = alloc_cnt;
    if (out_frag) *out_frag = frag_sum;

    if (!quiet) {
        print_alloc_table(blocks, nb, procs, np, "First-Fit Allocation");
    }
}

static void run_best_fit(MemoryBlock *orig_blocks, int nb, ProcRequest *orig_procs, int np, int quiet, int *out_alloc, int *out_frag) {
    MemoryBlock blocks[nb];
    ProcRequest procs[np];
    memcpy(blocks, orig_blocks, sizeof(MemoryBlock) * nb);
    memcpy(procs, orig_procs, sizeof(ProcRequest) * np);

    for (int i = 0; i < np; i++) {
        procs[i].block_assigned = -1;
        int best_idx = -1;
        int min_space = 1e9;

        for (int j = 0; j < nb; j++) {
            if (blocks[j].allocated_proc == 0 && blocks[j].size >= procs[i].size) {
                if (blocks[j].size < min_space) {
                    min_space = blocks[j].size;
                    best_idx = j;
                }
            }
        }

        if (best_idx != -1) {
            blocks[best_idx].allocated_proc = procs[i].pid;
            procs[i].block_assigned = best_idx;
        }
    }

    int alloc_cnt = 0, frag_sum = 0;
    for (int i = 0; i < np; i++) {
        if (procs[i].block_assigned != -1) {
            alloc_cnt++;
            frag_sum += blocks[procs[i].block_assigned].original_size - procs[i].size;
        }
    }

    if (out_alloc) *out_alloc = alloc_cnt;
    if (out_frag) *out_frag = frag_sum;

    if (!quiet) {
        print_alloc_table(blocks, nb, procs, np, "Best-Fit Allocation");
    }
}

static void run_worst_fit(MemoryBlock *orig_blocks, int nb, ProcRequest *orig_procs, int np, int quiet, int *out_alloc, int *out_frag) {
    MemoryBlock blocks[nb];
    ProcRequest procs[np];
    memcpy(blocks, orig_blocks, sizeof(MemoryBlock) * nb);
    memcpy(procs, orig_procs, sizeof(ProcRequest) * np);

    for (int i = 0; i < np; i++) {
        procs[i].block_assigned = -1;
        int worst_idx = -1;
        int max_space = -1;

        for (int j = 0; j < nb; j++) {
            if (blocks[j].allocated_proc == 0 && blocks[j].size >= procs[i].size) {
                if (blocks[j].size > max_space) {
                    max_space = blocks[j].size;
                    worst_idx = j;
                }
            }
        }

        if (worst_idx != -1) {
            blocks[worst_idx].allocated_proc = procs[i].pid;
            procs[i].block_assigned = worst_idx;
        }
    }

    int alloc_cnt = 0, frag_sum = 0;
    for (int i = 0; i < np; i++) {
        if (procs[i].block_assigned != -1) {
            alloc_cnt++;
            frag_sum += blocks[procs[i].block_assigned].original_size - procs[i].size;
        }
    }

    if (out_alloc) *out_alloc = alloc_cnt;
    if (out_frag) *out_frag = frag_sum;

    if (!quiet) {
        print_alloc_table(blocks, nb, procs, np, "Worst-Fit Allocation");
    }
}

int builtin_memalloc(command_t *cmd) {
    MemoryBlock blocks[] = {
        {1, 100, 0, 100},
        {2, 500, 0, 500},
        {3, 200, 0, 200},
        {4, 300, 0, 300},
        {5, 600, 0, 600}
    };
    int nb = 5;

    ProcRequest procs[] = {
        {1, 212, -1},
        {2, 417, -1},
        {3, 112, -1},
        {4, 426, -1}
    };
    int np = 4;

    if (cmd->argc == 1 || strcmp(cmd->argv[1], "compare") == 0 || strcmp(cmd->argv[1], "all") == 0) {
        int alloc_ff, frag_ff;
        int alloc_bf, frag_bf;
        int alloc_wf, frag_wf;

        run_first_fit(blocks, nb, procs, np, 1, &alloc_ff, &frag_ff);
        run_best_fit(blocks, nb, procs, np, 1, &alloc_bf, &frag_bf);
        run_worst_fit(blocks, nb, procs, np, 1, &alloc_wf, &frag_wf);

        printf("===========================================================\n");
        printf(" Contiguous Memory Allocation Comparison\n");
        printf(" Memory Blocks:  B1: 100KB, B2: 500KB, B3: 200KB, B4: 300KB, B5: 600KB\n");
        printf(" Process Demands: P1: 212KB, P2: 417KB, P3: 112KB, P4: 426KB\n");
        printf("===========================================================\n");
        printf("Algorithm       Allocated Processes    Total Internal Frag\n");
        printf("-----------------------------------------------------------\n");
        printf("First-Fit     : %d / %-16d %d KB\n", alloc_ff, np, frag_ff);
        printf("Best-Fit      : %d / %-16d %d KB\n", alloc_bf, np, frag_bf);
        printf("Worst-Fit     : %d / %-16d %d KB\n", alloc_wf, np, frag_wf);
        printf("===========================================================\n");
        printf("Tip: Run individual strategies: 'memalloc firstfit', 'memalloc bestfit', or 'memalloc worstfit'\n");
        return 0;
    }

    if (strcmp(cmd->argv[1], "firstfit") == 0 || strcmp(cmd->argv[1], "ff") == 0) {
        run_first_fit(blocks, nb, procs, np, 0, NULL, NULL);
        return 0;
    } else if (strcmp(cmd->argv[1], "bestfit") == 0 || strcmp(cmd->argv[1], "bf") == 0) {
        run_best_fit(blocks, nb, procs, np, 0, NULL, NULL);
        return 0;
    } else if (strcmp(cmd->argv[1], "worstfit") == 0 || strcmp(cmd->argv[1], "wf") == 0) {
        run_worst_fit(blocks, nb, procs, np, 0, NULL, NULL);
        return 0;
    } else {
        printf("Usage: memalloc [firstfit | bestfit | worstfit | compare]\n");
        return -1;
    }
}
