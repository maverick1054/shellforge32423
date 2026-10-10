#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "builtin.h"
#include "history.h"
#include "pagesim.h"
#include "cpusched.h"
#include "memalloc.h"
#include "jobs.h"
#include "job_control.h"

/* =========================================================
   BUILTIN: cd
   ========================================================= */
static int builtin_cd(command_t *cmd)
{
    const char *directory;

    if (cmd->argc == 1) {
        directory = getenv("HOME");
        if (directory == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return -1;
        }
    } else if (cmd->argc == 2) {
        directory = cmd->argv[1];
    } else {
        fprintf(stderr, "cd: too many arguments\n");
        return -1;
    }

    if (chdir(directory) != 0) {
        perror("cd");
        return -1;
    }
    return 0;
}

/* =========================================================
   BUILTIN: pwd
   ========================================================= */
static int builtin_pwd(command_t *cmd)
{
    char current_directory[4096];
    if (cmd->argc > 1) {
        fprintf(stderr, "pwd: too many arguments\n");
        return -1;
    }
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        perror("pwd");
        return -1;
    }
    printf("%s\n", current_directory);
    return 0;
}

/* =========================================================
   BUILTIN: echo
   ========================================================= */
static int builtin_echo(command_t *cmd)
{
    for (int i = 1; i < cmd->argc; i++) {
        printf("%s", cmd->argv[i]);
        if (i < cmd->argc - 1) printf(" ");
    }
    printf("\n");
    return 0;
}

/* =========================================================
   BUILTIN: exit
   ========================================================= */
static int builtin_exit(command_t *cmd)
{
    if (cmd->argc > 2) {
        fprintf(stderr, "exit: too many arguments\n");
        return -1;
    }
    int status = 0;
    if (cmd->argc == 2) status = atoi(cmd->argv[1]);
    exit(status);
}

/* =========================================================
   BUILTIN: clear
   ========================================================= */
static int builtin_clear(command_t *cmd)
{
    if (cmd->argc > 1) {
        fprintf(stderr, "clear: too many arguments\n");
        return -1;
    }
    printf("\033[H\033[J");
    return 0;
}

/* =========================================================
   BUILTIN: help
   ========================================================= */
static int builtin_help(command_t *cmd)
{
    (void)cmd; // silence unused parameter warning
    printf("Shellforge built-in commands:\n");
    printf("  cd [dir]     - change directory\n");
    printf("  pwd          - print working directory\n");
    printf("  echo [args]  - print arguments\n");
    printf("  exit [code]  - exit shell\n");
    printf("  clear        - clear screen\n");
    printf("  help         - show this help\n");
    printf("  history      - show command history\n");
    printf("  whoami       - show current user\n");
    printf("  mkdir <dir>  - create directory\n");
    printf("  rmdir <dir>  - remove directory\n");
    printf("  touch <file> - create empty file\n");
    printf("  cat <file>   - display file contents\n");
    printf("  date         - show current date/time\n");
    printf("  jobs         - list active background/stopped jobs\n");
    printf("  fg <job_id>  - bring background/stopped job to foreground\n");
    printf("  bg <job_id>  - resume stopped job in background\n");
    printf("  pagesim      - page replacement simulator (FIFO, LRU, Optimal)\n");
    printf("  cpusched     - CPU scheduling simulator (FCFS, SJF, RR, Priority)\n");
    printf("  memalloc     - memory allocation simulator (First-Fit, Best-Fit, Worst-Fit)\n");
    return 0;
}

/* =========================================================
   BUILTIN: history
   ========================================================= */
static int builtin_history(command_t *cmd)
{
    if (cmd->argc > 1) {
        fprintf(stderr, "history: too many arguments\n");
        return -1;
    }
    print_history();
    return 0;
}

/* =========================================================
   BUILTIN: whoami
   ========================================================= */
static int builtin_whoami(command_t *cmd)
{
    if (cmd->argc > 1) {
        fprintf(stderr, "whoami: too many arguments\n");
        return -1;
    }
    char *user = getenv("USER");
    if (user) {
        printf("%s\n", user);
        return 0;
    }
    fprintf(stderr, "whoami: USER not set\n");
    return -1;
}

/* =========================================================
   BUILTIN: mkdir
   ========================================================= */
static int builtin_mkdir(command_t *cmd)
{
    if (cmd->argc != 2) {
        fprintf(stderr, "mkdir: usage: mkdir <directory>\n");
        return -1;
    }
    if (mkdir(cmd->argv[1], 0755) != 0) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

/* =========================================================
   BUILTIN: rmdir
   ========================================================= */
static int builtin_rmdir(command_t *cmd)
{
    if (cmd->argc != 2) {
        fprintf(stderr, "rmdir: usage: rmdir <directory>\n");
        return -1;
    }
    if (rmdir(cmd->argv[1]) != 0) {
        perror("rmdir");
        return -1;
    }
    return 0;
}

/* =========================================================
   BUILTIN: touch
   ========================================================= */
static int builtin_touch(command_t *cmd)
{
    if (cmd->argc != 2) {
        fprintf(stderr, "touch: usage: touch <file>\n");
        return -1;
    }
    FILE *fp = fopen(cmd->argv[1], "a");
    if (!fp) {
        perror("touch");
        return -1;
    }
    fclose(fp);
    return 0;
}

/* =========================================================
   BUILTIN: cat
   ========================================================= */
static int builtin_cat(command_t *cmd)
{
    if (cmd->argc != 2) {
        fprintf(stderr, "cat: usage: cat <file>\n");
        return -1;
    }
    FILE *fp = fopen(cmd->argv[1], "r");
    if (!fp) {
        perror("cat");
        return -1;
    }
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp)) {
        fputs(buffer, stdout);
    }
    fclose(fp);
    return 0;
}

/* =========================================================
   BUILTIN: date
   ========================================================= */
static int builtin_date(command_t *cmd)
{
    if (cmd->argc > 1) {
        fprintf(stderr, "date: too many arguments\n");
        return -1;
    }
    time_t now = time(NULL);
    printf("%s", ctime(&now));
    return 0;
}

/* =========================================================
   BUILTIN: jobs
   ========================================================= */
int builtin_jobs(command_t *cmd)
{
    (void)cmd;
    jobs_print();
    return 0;
}

/* =========================================================
   BUILTIN: fg
   ========================================================= */
int builtin_fg(command_t *cmd)
{
    int job_id;
    job_t *job;
    int status;

    if (cmd->argc < 2) {
        printf("fg: usage: fg <job number>\n");
        return 1;
    }

    job_id = atoi(cmd->argv[1]);
    job = job_find(job_id);

    if (job == NULL) {
        printf("fg: no such job: %d\n", job_id);
        return 1;
    }

    /* Give terminal to job */
    give_terminal_to(job->pgid);

    /* Continue stopped process group */
    if (job->state == JOB_STOPPED) {
        kill(-job->pgid, SIGCONT);
    }

    job_continue(job->pgid);

    /* Wait for the process group */
    while (1) {
        pid_t result = waitpid(-job->pgid, &status, WUNTRACED);
        if (result < 0) {
            break;
        }

        if (WIFSTOPPED(status)) {
            job_stop(job->pgid);
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job_done(job->pgid);
            break;
        }
    }

    /* Give terminal back to Shellforge */
    take_terminal_back();

    /* Remove completed job */
    if (job->state == JOB_DONE) {
        job_remove(job_id);
    }

    return 0;
}

/* =========================================================
   BUILTIN: bg
   ========================================================= */
int builtin_bg(command_t *cmd)
{
    int job_id;
    job_t *job;

    if (cmd->argc < 2) {
        fprintf(stderr, "bg: usage: bg <job number>\n");
        return 1;
    }

    job_id = atoi(cmd->argv[1]);
    job = job_find(job_id);

    if (job == NULL) {
        fprintf(stderr, "bg: job not found: %d\n", job_id);
        return 1;
    }

    if (kill(-job->pgid, SIGCONT) == -1) {
        perror("bg: kill");
        return 1;
    }

    job_continue(job->pgid);

    printf("[%d] %s &\n", job->job_id, job->command);

    return 0;
}

/* =========================================================
   CHECK WHETHER COMMAND IS A BUILTIN
   ========================================================= */
int is_builtin(const command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0) return 0;

    const char *name = cmd->argv[0];
    const char *builtins[] = {
        "cd", "pwd", "echo", "exit", "clear", "help", "history",
        "whoami", "mkdir", "rmdir", "touch", "cat", "date",
        "jobs", "fg", "bg",
        "pagesim", "cpusched", "memalloc"
    };

    for (int i = 0; i < 19; i++) {
        if (strcmp(name, builtins[i]) == 0) return 1;
    }
    return 0;
}

/* =========================================================
   EXECUTE BUILTIN
   ========================================================= */
int execute_builtin(command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0) return -1;

    if (strcmp(cmd->argv[0], "cd") == 0) return builtin_cd(cmd);
    if (strcmp(cmd->argv[0], "pwd") == 0) return builtin_pwd(cmd);
    if (strcmp(cmd->argv[0], "echo") == 0) return builtin_echo(cmd);
    if (strcmp(cmd->argv[0], "exit") == 0) return builtin_exit(cmd);
    if (strcmp(cmd->argv[0], "clear") == 0) return builtin_clear(cmd);
    if (strcmp(cmd->argv[0], "help") == 0) return builtin_help(cmd);
    if (strcmp(cmd->argv[0], "history") == 0) return builtin_history(cmd);
    if (strcmp(cmd->argv[0], "whoami") == 0) return builtin_whoami(cmd);
    if (strcmp(cmd->argv[0], "mkdir") == 0) return builtin_mkdir(cmd);
    if (strcmp(cmd->argv[0], "rmdir") == 0) return builtin_rmdir(cmd);
    if (strcmp(cmd->argv[0], "touch") == 0) return builtin_touch(cmd);
    if (strcmp(cmd->argv[0], "cat") == 0) return builtin_cat(cmd);
    if (strcmp(cmd->argv[0], "date") == 0) return builtin_date(cmd);
    if (strcmp(cmd->argv[0], "jobs") == 0) return builtin_jobs(cmd);
    if (strcmp(cmd->argv[0], "fg") == 0) return builtin_fg(cmd);
    if (strcmp(cmd->argv[0], "bg") == 0) return builtin_bg(cmd);
    if (strcmp(cmd->argv[0], "pagesim") == 0) return builtin_pagesim(cmd);
    if (strcmp(cmd->argv[0], "cpusched") == 0) return builtin_cpusched(cmd);
    if (strcmp(cmd->argv[0], "memalloc") == 0) return builtin_memalloc(cmd);

    return -1;
}
