#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cpusched.h"
#include "builtin.h"
#include "parser.h"

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int priority;
    int remaining_time;
    int completion_time;
    int waiting_time;
    int turnaround_time;
} Process;

typedef struct {
    int pid;
    int start_time;
    int end_time;
} GanttSegment;

static void print_gantt_chart(GanttSegment *gantt, int count) {
    if (count == 0) return;

    printf("\n--- Gantt Chart ---\n+");
    for (int i = 0; i < count; i++) {
        printf("-------+");
    }
    printf("\n|");
    for (int i = 0; i < count; i++) {
        if (gantt[i].pid == 0) {
            printf(" IDLE  |");
        } else {
            printf("  P%-2d  |", gantt[i].pid);
        }
    }
    printf("\n+");
    for (int i = 0; i < count; i++) {
        printf("-------+");
    }
    printf("\n");
    printf("%-8d", gantt[0].start_time);
    for (int i = 0; i < count; i++) {
        printf("%-8d", gantt[i].end_time);
    }
    printf("\n\n");
}

static void print_results(Process *proc, int n, GanttSegment *gantt, int gantt_count, const char *algo_name) {
    printf("====================================================\n");
    printf(" CPU Scheduling Simulation: %s\n", algo_name);
    printf("====================================================\n");
    printf("PID  Arrival  Burst  Priority  Completion  Turnaround  Waiting\n");
    printf("----------------------------------------------------\n");

    double total_wt = 0, total_tat = 0;
    for (int i = 0; i < n; i++) {
        printf("P%-2d  %-7d  %-5d  %-8d  %-10d  %-10d  %-7d\n",
               proc[i].pid, proc[i].arrival_time, proc[i].burst_time,
               proc[i].priority, proc[i].completion_time,
               proc[i].turnaround_time, proc[i].waiting_time);
        total_wt += proc[i].waiting_time;
        total_tat += proc[i].turnaround_time;
    }
    printf("----------------------------------------------------\n");
    printf("Average Waiting Time (AWT)   : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time (TAT): %.2f units\n", total_tat / n);

    print_gantt_chart(gantt, gantt_count);
}

static void run_fcfs(Process *input, int n, double *out_awt, double *out_tat) {
    Process proc[n];
    memcpy(proc, input, sizeof(Process) * n);

    // Sort by arrival time
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (proc[i].arrival_time > proc[j].arrival_time) {
                Process temp = proc[i]; proc[i] = proc[j]; proc[j] = temp;
            }
        }
    }

    GanttSegment gantt[100];
    int gantt_count = 0;
    int current_time = 0;

    for (int i = 0; i < n; i++) {
        if (current_time < proc[i].arrival_time) {
            gantt[gantt_count++] = (GanttSegment){0, current_time, proc[i].arrival_time};
            current_time = proc[i].arrival_time;
        }
        int start = current_time;
        current_time += proc[i].burst_time;
        proc[i].completion_time = current_time;
        proc[i].turnaround_time = proc[i].completion_time - proc[i].arrival_time;
        proc[i].waiting_time = proc[i].turnaround_time - proc[i].burst_time;
        gantt[gantt_count++] = (GanttSegment){proc[i].pid, start, current_time};
    }

    double total_wt = 0, total_tat = 0;
    for (int i = 0; i < n; i++) {
        total_wt += proc[i].waiting_time;
        total_tat += proc[i].turnaround_time;
    }
    if (out_awt) *out_awt = total_wt / n;
    if (out_tat) *out_tat = total_tat / n;

    if (!out_awt) print_results(proc, n, gantt, gantt_count, "First-Come First-Served (FCFS)");
}

static void run_sjf(Process *input, int n, double *out_awt, double *out_tat) {
    Process proc[n];
    memcpy(proc, input, sizeof(Process) * n);

    bool completed[n];
    memset(completed, 0, sizeof(completed));

    GanttSegment gantt[100];
    int gantt_count = 0;
    int current_time = 0;
    int completed_count = 0;

    while (completed_count < n) {
        int idx = -1;
        int min_burst = 1e9;

        for (int i = 0; i < n; i++) {
            if (!completed[i] && proc[i].arrival_time <= current_time) {
                if (proc[i].burst_time < min_burst) {
                    min_burst = proc[i].burst_time;
                    idx = i;
                }
            }
        }

        if (idx == -1) {
            // Find next arrival time
            int next_arrival = 1e9;
            for (int i = 0; i < n; i++) {
                if (!completed[i] && proc[i].arrival_time < next_arrival) {
                    next_arrival = proc[i].arrival_time;
                }
            }
            gantt[gantt_count++] = (GanttSegment){0, current_time, next_arrival};
            current_time = next_arrival;
            continue;
        }

        int start = current_time;
        current_time += proc[idx].burst_time;
        proc[idx].completion_time = current_time;
        proc[idx].turnaround_time = proc[idx].completion_time - proc[idx].arrival_time;
        proc[idx].waiting_time = proc[idx].turnaround_time - proc[idx].burst_time;
        completed[idx] = true;
        completed_count++;
        gantt[gantt_count++] = (GanttSegment){proc[idx].pid, start, current_time};
    }

    double total_wt = 0, total_tat = 0;
    for (int i = 0; i < n; i++) {
        total_wt += proc[i].waiting_time;
        total_tat += proc[i].turnaround_time;
    }
    if (out_awt) *out_awt = total_wt / n;
    if (out_tat) *out_tat = total_tat / n;

    if (!out_awt) print_results(proc, n, gantt, gantt_count, "Shortest Job First (SJF)");
}

static void run_priority(Process *input, int n, double *out_awt, double *out_tat) {
    Process proc[n];
    memcpy(proc, input, sizeof(Process) * n);

    bool completed[n];
    memset(completed, 0, sizeof(completed));

    GanttSegment gantt[100];
    int gantt_count = 0;
    int current_time = 0;
    int completed_count = 0;

    while (completed_count < n) {
        int idx = -1;
        int highest_priority = 1e9;

        for (int i = 0; i < n; i++) {
            if (!completed[i] && proc[i].arrival_time <= current_time) {
                if (proc[i].priority < highest_priority) {
                    highest_priority = proc[i].priority;
                    idx = i;
                }
            }
        }

        if (idx == -1) {
            int next_arrival = 1e9;
            for (int i = 0; i < n; i++) {
                if (!completed[i] && proc[i].arrival_time < next_arrival) {
                    next_arrival = proc[i].arrival_time;
                }
            }
            gantt[gantt_count++] = (GanttSegment){0, current_time, next_arrival};
            current_time = next_arrival;
            continue;
        }

        int start = current_time;
        current_time += proc[idx].burst_time;
        proc[idx].completion_time = current_time;
        proc[idx].turnaround_time = proc[idx].completion_time - proc[idx].arrival_time;
        proc[idx].waiting_time = proc[idx].turnaround_time - proc[idx].burst_time;
        completed[idx] = true;
        completed_count++;
        gantt[gantt_count++] = (GanttSegment){proc[idx].pid, start, current_time};
    }

    double total_wt = 0, total_tat = 0;
    for (int i = 0; i < n; i++) {
        total_wt += proc[i].waiting_time;
        total_tat += proc[i].turnaround_time;
    }
    if (out_awt) *out_awt = total_wt / n;
    if (out_tat) *out_tat = total_tat / n;

    if (!out_awt) print_results(proc, n, gantt, gantt_count, "Priority Scheduling (Non-Preemptive)");
}

static void run_rr(Process *input, int n, int quantum, double *out_awt, double *out_tat) {
    Process proc[n];
    for (int i = 0; i < n; i++) {
        proc[i] = input[i];
        proc[i].remaining_time = proc[i].burst_time;
    }

    GanttSegment gantt[200];
    int gantt_count = 0;
    int current_time = 0;
    int completed_count = 0;

    int queue[500];
    int front = 0, rear = 0;
    bool in_queue[n];
    memset(in_queue, 0, sizeof(in_queue));

    // Push initial arrived processes
    for (int i = 0; i < n; i++) {
        if (proc[i].arrival_time == 0) {
            queue[rear++] = i;
            in_queue[i] = true;
        }
    }

    while (completed_count < n) {
        if (front == rear) { // Queue empty
            // Check if any process has not arrived yet
            int next_arrival = 1e9;
            for (int i = 0; i < n; i++) {
                if (proc[i].remaining_time > 0 && proc[i].arrival_time < next_arrival) {
                    next_arrival = proc[i].arrival_time;
                }
            }
            if (next_arrival != 1e9) {
                gantt[gantt_count++] = (GanttSegment){0, current_time, next_arrival};
                current_time = next_arrival;
                for (int i = 0; i < n; i++) {
                    if (proc[i].remaining_time > 0 && proc[i].arrival_time <= current_time && !in_queue[i]) {
                        queue[rear++] = i;
                        in_queue[i] = true;
                    }
                }
            }
            continue;
        }

        int idx = queue[front++];
        int exec_time = (proc[idx].remaining_time > quantum) ? quantum : proc[idx].remaining_time;
        int start = current_time;
        current_time += exec_time;
        proc[idx].remaining_time -= exec_time;

        gantt[gantt_count++] = (GanttSegment){proc[idx].pid, start, current_time};

        // Add newly arrived processes during exec_time
        for (int i = 0; i < n; i++) {
            if (i != idx && proc[i].remaining_time > 0 && proc[i].arrival_time <= current_time && !in_queue[i]) {
                queue[rear++] = i;
                in_queue[i] = true;
            }
        }

        if (proc[idx].remaining_time > 0) {
            queue[rear++] = idx; // Re-enqueue current process
        } else {
            proc[idx].completion_time = current_time;
            proc[idx].turnaround_time = proc[idx].completion_time - proc[idx].arrival_time;
            proc[idx].waiting_time = proc[idx].turnaround_time - proc[idx].burst_time;
            completed_count++;
        }
    }

    double total_wt = 0, total_tat = 0;
    for (int i = 0; i < n; i++) {
        total_wt += proc[i].waiting_time;
        total_tat += proc[i].turnaround_time;
    }
    if (out_awt) *out_awt = total_wt / n;
    if (out_tat) *out_tat = total_tat / n;

    if (!out_awt) {
        char title[64];
        snprintf(title, sizeof(title), "Round Robin (RR, Quantum = %d)", quantum);
        print_results(proc, n, gantt, gantt_count, title);
    }
}

int builtin_cpusched(command_t *cmd) {
    Process default_procs[] = {
        {1, 0, 6, 2, 6, 0, 0, 0},
        {2, 1, 3, 1, 3, 0, 0, 0},
        {3, 2, 8, 3, 8, 0, 0, 0},
        {4, 3, 4, 2, 4, 0, 0, 0}
    };
    int n = 4;

    if (cmd->argc == 1 || strcmp(cmd->argv[1], "compare") == 0 || strcmp(cmd->argv[1], "all") == 0) {
        double awt_fcfs, tat_fcfs;
        double awt_sjf, tat_sjf;
        double awt_pri, tat_pri;
        double awt_rr, tat_rr;

        run_fcfs(default_procs, n, &awt_fcfs, &tat_fcfs);
        run_sjf(default_procs, n, &awt_sjf, &tat_sjf);
        run_priority(default_procs, n, &awt_pri, &tat_pri);
        run_rr(default_procs, n, 2, &awt_rr, &tat_rr);

        printf("===========================================================\n");
        printf(" CPU Scheduling Algorithms Performance Comparison\n");
        printf(" Sample Processes: P1(B:6, A:0, P:2), P2(B:3, A:1, P:1),\n");
        printf("                  P3(B:8, A:2, P:3), P4(B:4, A:3, P:2)\n");
        printf("===========================================================\n");
        printf("Algorithm                       Avg Waiting Time   Avg Turnaround Time\n");
        printf("-----------------------------------------------------------\n");
        printf("1. First-Come First-Served     : %-17.2f %.2f\n", awt_fcfs, tat_fcfs);
        printf("2. Shortest Job First (SJF)    : %-17.2f %.2f\n", awt_sjf, tat_sjf);
        printf("3. Priority Scheduling         : %-17.2f %.2f\n", awt_pri, tat_pri);
        printf("4. Round Robin (Quantum = 2)   : %-17.2f %.2f\n", awt_rr, tat_rr);
        printf("===========================================================\n");
        printf("Tip: Run individual algorithms using: 'cpusched fcfs', 'cpusched sjf', 'cpusched priority', or 'cpusched rr <quantum>'\n");
        return 0;
    }

    if (strcmp(cmd->argv[1], "fcfs") == 0) {
        run_fcfs(default_procs, n, NULL, NULL);
        return 0;
    } else if (strcmp(cmd->argv[1], "sjf") == 0) {
        run_sjf(default_procs, n, NULL, NULL);
        return 0;
    } else if (strcmp(cmd->argv[1], "priority") == 0) {
        run_priority(default_procs, n, NULL, NULL);
        return 0;
    } else if (strcmp(cmd->argv[1], "rr") == 0) {
        int q = 2;
        if (cmd->argc >= 3) {
            q = atoi(cmd->argv[2]);
            if (q <= 0) q = 2;
        }
        run_rr(default_procs, n, q, NULL, NULL);
        return 0;
    } else {
        printf("Usage: cpusched [fcfs | sjf | priority | rr <quantum> | compare]\n");
        return -1;
    }
}
