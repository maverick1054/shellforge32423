/*
 * thread_example.c
 * -----------------
 * Demonstrates creation of POSIX threads using pthreads, each performing a
 * simple workload (sleep) and then terminating. The main thread creates the
 * workers, prints a start message for each, and then waits for all of them to
 * finish using pthread_join. Detailed comments explain every step.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>   // for usleep (microsecond sleep)

/* ---------------------------------------------------------------------
 * Worker function executed by each thread.
 *
 * The argument is a pointer to an int that holds the thread's identifier.
 * The function prints a start message, sleeps for a short period to simulate
 * work, prints a finish message, and returns.
 * --------------------------------------------------------------------- */
void *worker(void *arg) {
    int id = *(int *)arg;                 // retrieve thread ID
    printf("Thread %d starting, will sleep %.1f seconds\n", id, (id + 1) * 0.5);
    usleep((id + 1) * 500000);            // sleep (id+1)*0.5 seconds
    printf("Thread %d finished\n", id);
    return NULL;                          // thread exit status (unused)
}

int main(void) {
    /* ---------------------------------------------------------------
     * Declare a separate pthread_t variable for each thread. This
     * makes the relationship between the variable name and the thread
     * explicit, which matches the request to "have pthread_t create and
     * pthread_t join".
     * --------------------------------------------------------------- */
    pthread_t t0, t1, t2, t3, t4;          // thread handles
    int ids[5] = {0, 1, 2, 3, 4};          // IDs passed to workers

    /* ------------------- Create the threads ---------------------- */
    if (pthread_create(&t0, NULL, worker, &ids[0]) != 0) { perror("pthread_create t0"); return 1; }
    printf("Started thread 0\n");
    if (pthread_create(&t1, NULL, worker, &ids[1]) != 0) { perror("pthread_create t1"); return 1; }
    printf("Started thread 1\n");
    if (pthread_create(&t2, NULL, worker, &ids[2]) != 0) { perror("pthread_create t2"); return 1; }
    printf("Started thread 2\n");
    if (pthread_create(&t3, NULL, worker, &ids[3]) != 0) { perror("pthread_create t3"); return 1; }
    printf("Started thread 3\n");
    if (pthread_create(&t4, NULL, worker, &ids[4]) != 0) { perror("pthread_create t4"); return 1; }
    printf("Started thread 4\n");

    /* ---------------------- Join the threads ---------------------- */
    // pthread_join blocks the calling (main) thread until the specified
    // worker thread terminates. This ensures the program does not exit
    // before all workers have completed.
    if (pthread_join(t0, NULL) != 0) { perror("pthread_join t0"); return 1; }
    printf("Joined thread 0\n");
    if (pthread_join(t1, NULL) != 0) { perror("pthread_join t1"); return 1; }
    printf("Joined thread 1\n");
    if (pthread_join(t2, NULL) != 0) { perror("pthread_join t2"); return 1; }
    printf("Joined thread 2\n");
    if (pthread_join(t3, NULL) != 0) { perror("pthread_join t3"); return 1; }
    printf("Joined thread 3\n");
    if (pthread_join(t4, NULL) != 0) { perror("pthread_join t4"); return 1; }
    printf("Joined thread 4\n");

    printf("All threads have completed.\n");
    return 0; // successful termination
}
