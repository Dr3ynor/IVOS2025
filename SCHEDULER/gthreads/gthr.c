#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <limits.h>


#include "gthr.h"
#include "gthr_struct.h"

// Helper function to get time difference in microseconds
long time_diff_us(struct timeval *start, struct timeval *end) {
    return ((end->tv_sec - start->tv_sec) * 1000000) + 
           (end->tv_usec - start->tv_usec);
}

// Initialize thread statistics
void gt_init_stats(struct gt_stats *stats) {
    gettimeofday(&stats->created_at, NULL);
    gettimeofday(&stats->last_run, NULL);
    gettimeofday(&stats->last_ready, NULL);
    
    stats->total_runtime = 0;
    stats->total_waittime = 0;
    
    stats->min_runtime = LONG_MAX;
    stats->max_runtime = 0;
    stats->sum_runtime = 0;
    stats->sum_squared_runtime = 0;
    stats->run_count = 0;
    
    stats->min_waittime = LONG_MAX;
    stats->max_waittime = 0;
    stats->sum_waittime = 0;
    stats->sum_squared_waittime = 0;
    stats->wait_count = 0;
}

// function triggered periodically by timer (SIGALRM)
void gt_alarm_handle(int sig) {
	gt_schedule();
}

// SIGINT handler to print thread statistics
void gt_print_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    printf("\n----- Thread Statistics -----\n");
    printf("%-6s %-12s %-12s %-12s %-12s %-12s %-12s %-12s\n", 
           "ID", "State", "Runtime(μs)", "Waittime(μs)", "Avg Run(μs)", "Avg Wait(μs)", "Var Run", "Var Wait");
    
    for (int i = 0; i < MaxGThreads; i++) {
        if (gt_table[i].state == Unused && gt_table[i].stats.run_count == 0) {
            continue; // Skip unused threads that have never run
        }
        
        // Calculate current metrics if thread is active
        long current_runtime = gt_table[i].state == Running ? 
            time_diff_us(&gt_table[i].stats.last_run, &now) : 0;
        
        long current_waittime = gt_table[i].state == Ready ? 
            time_diff_us(&gt_table[i].stats.last_ready, &now) : 0;
        
        // Calculate total runtime and waittime including current period
        long total_runtime = gt_table[i].stats.total_runtime + current_runtime;
        long total_waittime = gt_table[i].stats.total_waittime + current_waittime;
        
        // Calculate averages
        double avg_runtime = gt_table[i].stats.run_count > 0 ? 
            (double)gt_table[i].stats.sum_runtime / gt_table[i].stats.run_count : 0;
        
        double avg_waittime = gt_table[i].stats.wait_count > 0 ? 
            (double)gt_table[i].stats.sum_waittime / gt_table[i].stats.wait_count : 0;
        
        // Calculate variances
        double var_runtime = 0;
        if (gt_table[i].stats.run_count > 1) {
            var_runtime = ((double)gt_table[i].stats.sum_squared_runtime / gt_table[i].stats.run_count) - 
                          (avg_runtime * avg_runtime);
        }
        
        double var_waittime = 0;
        if (gt_table[i].stats.wait_count > 1) {
            var_waittime = ((double)gt_table[i].stats.sum_squared_waittime / gt_table[i].stats.wait_count) - 
                          (avg_waittime * avg_waittime);
        }
        
        // Get state as string
        const char *state;
        switch (gt_table[i].state) {
            case Running: state = "Running"; break;
            case Ready: state = "Ready"; break;
            case Unused: state = "Unused"; break;
            default: state = "Unknown";
        }
        
        printf("%-6d %-12s %-12ld %-12ld %-12.2f %-12.2f %-12.2f %-12.2f\n", 
               i, state, total_runtime, total_waittime, 
               avg_runtime, avg_waittime, var_runtime, var_waittime);
    }
    
    printf("--- Additional Thread Info ---\n");
    for (int i = 0; i < MaxGThreads; i++) {
        if (gt_table[i].state != Unused || gt_table[i].stats.run_count > 0) {
            printf("Thread %d:\n", i);
            printf("  Min runtime: %ld μs, Max runtime: %ld μs\n", 
                   gt_table[i].stats.min_runtime == LONG_MAX ? 0 : gt_table[i].stats.min_runtime, 
                   gt_table[i].stats.max_runtime);
            printf("  Min waittime: %ld μs, Max waittime: %ld μs\n", 
                   gt_table[i].stats.min_waittime == LONG_MAX ? 0 : gt_table[i].stats.min_waittime, 
                   gt_table[i].stats.max_waittime);
            printf("  Run count: %ld, Wait count: %ld\n", 
                   gt_table[i].stats.run_count, gt_table[i].stats.wait_count);
            printf("  RSP: %lx\n", gt_table[i].ctx.rsp);
        }
    }
    printf("-----------------------------\n");
    exit(0); // Exit after printing stats
}

// initialize first thread as current context
void gt_init(void) {
	gt_current = & gt_table[0];                                             // initialize current thread with thread #0
	gt_current -> state = Running;                                          // set current to running
    gt_init_stats(&gt_current->stats);                                      // initialize statistics for the first thread
    gettimeofday(&gt_current->stats.last_run, NULL);                        // record start time for the first thread
    
	signal(SIGALRM, gt_alarm_handle);                                       // register SIGALRM, signal from timer generated by alarm
	signal(SIGINT, gt_print_stats);                                         // register SIGINT for statistics display
}

// exit thread
void __attribute__((noreturn)) gt_return(int ret) {
	if (gt_current != & gt_table[0]) {                                      // if not an initial thread,
        // Update runtime before exiting
        struct timeval now;
        gettimeofday(&now, NULL);
        long runtime = time_diff_us(&gt_current->stats.last_run, &now);
        gt_current->stats.total_runtime += runtime;
        
		gt_current -> state = Unused;                                       // set current thread as unused
		free((void*)(gt_current -> ctx.rsp + 16));                          // free the stack
		gt_schedule();                                                      // yield and make possible to switch to another thread
		assert(!"reachable");                                               // this code should never be reachable ... (if yes, returning function on stack was corrupted)
	}
	while (gt_schedule());                                                  // if initial thread, wait for other to terminate
	exit(ret);
}

// switch from one thread to other
bool gt_schedule(void) {
	struct gt * p;
	struct gt_context * old, * new;
    struct timeval now;
    gettimeofday(&now, NULL);

	gt_reset_sig(SIGALRM);                                                  // reset signal

    // Update statistics for current thread if it's running
    if (gt_current->state == Running) {
        long runtime = time_diff_us(&gt_current->stats.last_run, &now);
        gt_current->stats.total_runtime += runtime;
        gt_current->stats.run_count++;
        
        // Update min/max/sum/sum_squared for runtime
        if (runtime < gt_current->stats.min_runtime) {
            gt_current->stats.min_runtime = runtime;
        }
        if (runtime > gt_current->stats.max_runtime) {
            gt_current->stats.max_runtime = runtime;
        }
        gt_current->stats.sum_runtime += runtime;
        gt_current->stats.sum_squared_runtime += (runtime * runtime);
    }

	p = gt_current;
	while (p -> state != Ready) {                                           // iterate through gt_table[] until we find new thread in state Ready
		if (++p == & gt_table[MaxGThreads])                             // at the end rotate to the beginning
			p = & gt_table[0];
		if (p == gt_current)                                            // did not find any other Ready threads
			return false;
	}

    // Update waittime for the thread that's about to run
    if (p->state == Ready) {
        long waittime = time_diff_us(&p->stats.last_ready, &now);
        p->stats.total_waittime += waittime;
        p->stats.wait_count++;
        
        // Update min/max/sum/sum_squared for waittime
        if (waittime < p->stats.min_waittime) {
            p->stats.min_waittime = waittime;
        }
        if (waittime > p->stats.max_waittime) {
            p->stats.max_waittime = waittime;
        }
        p->stats.sum_waittime += waittime;
        p->stats.sum_squared_waittime += (waittime * waittime);
    }

	if (gt_current -> state != Unused) {                                     // switch current to Ready and new thread found in previous loop to Running
		gt_current -> state = Ready;
        gettimeofday(&gt_current->stats.last_ready, NULL);                 // record when thread became ready
    }
    
	p -> state = Running;
    gettimeofday(&p->stats.last_run, NULL);                                 // record when thread starts running
    
	old = & gt_current -> ctx;                                              // prepare pointers to context of current (will become old)
	new = & p -> ctx;                                                       // and new to new thread found in previous loop
	gt_current = p;                                                         // switch current indicator to new thread
	gt_switch(old, new);                                                    // perform context switch (assembly in gtswtch.S)
	return true;
}

// return function for terminating thread
void gt_stop(void) {
	gt_return(0);
}

// create new thread by providing pointer to function that will act like "run" method
int gt_create(void( * f)(void)) {
	char * stack;
	struct gt * p;

	for (p = & gt_table[0];; p++)                                           // find an empty slot
		if (p == & gt_table[MaxGThreads])                               // if we have reached the end, gt_table is full and we cannot create a new thread
			return -1;
		else if (p -> state == Unused)
			break;                                                  // new slot was found

	stack = malloc(StackSize);                                              // allocate memory for stack of newly created thread
	if (!stack)
		return -1;

	*(uint64_t * ) & stack[StackSize - 8] = (uint64_t) gt_stop;             //  put into the stack returning function gt_stop in case function calls return
	*(uint64_t * ) & stack[StackSize - 16] = (uint64_t) f;                  //  put provided function as a main "run" function
	p -> ctx.rsp = (uint64_t) & stack[StackSize - 16];                      //  set stack pointer
	p -> state = Ready;                                                     //  set state
    
    // Initialize statistics for new thread
    gt_init_stats(&p->stats);
    gettimeofday(&p->stats.last_ready, NULL);                               // record when thread became ready

	return 0;
}

// resets SIGALRM signal
void gt_reset_sig(int sig) {
	if (sig == SIGALRM) {
		alarm(0);                                                       // Clear pending alarms if any
	}

	sigset_t set;                                                           // Create signal set
	sigemptyset( & set);                                                    // Clear set
	sigaddset( & set, sig);                                                 // Set signal (we use SIGALRM)
	sigprocmask(SIG_UNBLOCK, & set, NULL);                                  // Fetch and change the signal mask

	if (sig == SIGALRM) {
		ualarm(500, 500);                                               // Schedule signal after given number of microseconds
	}
}

// uninterruptible sleep
int gt_uninterruptible_nanosleep(time_t sec, long nanosec) {
	struct timespec req;
	req.tv_sec = sec;
	req.tv_nsec = nanosec;

	do {
		if (0 != nanosleep( & req, & req)) {
			if (errno != EINTR)
				return -1;
		} else {
			break;
		}
	} while (req.tv_sec > 0 || req.tv_nsec > 0);
	return 0;
}