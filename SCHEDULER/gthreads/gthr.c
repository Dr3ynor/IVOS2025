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

// ANSI color codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

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

// Function to calculate variance
double calculate_variance(long sum, long sum_squared, long count) {
    if (count <= 1) {
        return 0.0;
    }
    double mean = (double)sum / count;
    return ((double)sum_squared / count) - (mean * mean);
}

// Function to get color based on value relative to min and max
// value = current value, min = minimum value, max = maximum value
// Returns color code string
const char* get_color_for_value(long value, long min, long max) {
    // If min and max are the same, or value is out of range, use reset color
    if (min == max || value < min || value > max) {
        return ANSI_COLOR_RESET;
    }
    
    // Calculate percentage of value between min and max
    double percentage = (double)(value - min) / (max - min);
    
    // Green for lowest values (best performance)
    if (percentage < 0.33) {
        return ANSI_COLOR_GREEN;
    } 
    // Yellow for middle values
    else if (percentage < 0.66) {
        return ANSI_COLOR_YELLOW;
    }
    // Red for highest values (worst performance)
    else {
        return ANSI_COLOR_RED;
    }
}

// function triggered periodically by timer (SIGALRM)
void gt_alarm_handle(int sig) {
	gt_schedule();
}

// Function to get global min/max values for runtime and waittime
void get_global_min_max(long *min_runtime, long *max_runtime, long *min_waittime, long *max_waittime) {
    *min_runtime = LONG_MAX;
    *max_runtime = 0;
    *min_waittime = LONG_MAX;
    *max_waittime = 0;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    for (int i = 0; i < MaxGThreads; i++) {
        if (gt_table[i].state == Unused && gt_table[i].stats.run_count == 0) {
            continue;
        }
        
        // Calculate current metrics if thread is active
        long current_runtime = gt_table[i].state == Running ? 
            time_diff_us(&gt_table[i].stats.last_run, &now) : 0;
        
        // Update min/max runtime
        long thread_min_runtime = gt_table[i].stats.min_runtime;
        long thread_max_runtime = gt_table[i].stats.max_runtime;
        
        if (thread_min_runtime != LONG_MAX && thread_min_runtime < *min_runtime) {
            *min_runtime = thread_min_runtime;
        }
        
        if (thread_max_runtime > *max_runtime) {
            *max_runtime = thread_max_runtime;
        }
        
        // Update min/max waittime
        long thread_min_waittime = gt_table[i].stats.min_waittime;
        long thread_max_waittime = gt_table[i].stats.max_waittime;
        
        if (thread_min_waittime != LONG_MAX && thread_min_waittime < *min_waittime) {
            *min_waittime = thread_min_waittime;
        }
        
        if (thread_max_waittime > *max_waittime) {
            *max_waittime = thread_max_waittime;
        }
    }
    
    // Handle case where no valid values were found
    if (*min_runtime == LONG_MAX) *min_runtime = 0;
    if (*min_waittime == LONG_MAX) *min_waittime = 0;
}

// SIGINT handler to print thread statistics
void gt_print_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Get global min/max values for color coding
    long global_min_runtime, global_max_runtime, global_min_waittime, global_max_waittime;
    get_global_min_max(&global_min_runtime, &global_max_runtime, &global_min_waittime, &global_max_waittime);
    
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
        double var_runtime = calculate_variance(
            gt_table[i].stats.sum_runtime,
            gt_table[i].stats.sum_squared_runtime,
            gt_table[i].stats.run_count
        );
        
        double var_waittime = calculate_variance(
            gt_table[i].stats.sum_waittime,
            gt_table[i].stats.sum_squared_waittime,
            gt_table[i].stats.wait_count
        );
        
        // Get state as string and color
        const char *state;
        const char *state_color;
        switch (gt_table[i].state) {
            case Running: 
                state = "Running"; 
                state_color = ANSI_COLOR_GREEN;  // Running threads in green
                break;
            case Ready: 
                state = "Ready"; 
                state_color = ANSI_COLOR_BLUE;   // Ready threads in blue
                break;
            case Unused: 
                state = "Unused"; 
                state_color = ANSI_COLOR_MAGENTA; // Unused threads in magenta
                break;
            default: 
                state = "Unknown";
                state_color = ANSI_COLOR_YELLOW; // Unknown state in yellow
        }
        
        // Get colors for runtime and waittime
        const char *runtime_color = get_color_for_value(total_runtime, global_min_runtime, global_max_runtime);
        const char *waittime_color = get_color_for_value(total_waittime, global_min_waittime, global_max_waittime);
        const char *avg_runtime_color = get_color_for_value(avg_runtime, 
                                                          global_min_runtime/gt_table[i].stats.run_count > 0 ? gt_table[i].stats.run_count : 1, 
                                                          global_max_runtime/gt_table[i].stats.run_count > 0 ? gt_table[i].stats.run_count : 1);
        const char *avg_waittime_color = get_color_for_value(avg_waittime, 
                                                           global_min_waittime/gt_table[i].stats.wait_count > 0 ? gt_table[i].stats.wait_count : 1, 
                                                           global_max_waittime/gt_table[i].stats.wait_count > 0 ? gt_table[i].stats.wait_count : 1);
        
        printf("%-6d %s%-12s%s %s%-12ld%s %s%-12ld%s %s%-12.2f%s %s%-12.2f%s %-12.2f %-12.2f\n", 
               i, 
               state_color, state, ANSI_COLOR_RESET,
               runtime_color, total_runtime, ANSI_COLOR_RESET,
               waittime_color, total_waittime, ANSI_COLOR_RESET,
               avg_runtime_color, avg_runtime, ANSI_COLOR_RESET,
               avg_waittime_color, avg_waittime, ANSI_COLOR_RESET,
               var_runtime, var_waittime);
    }
    
    printf("--- Additional Thread Info ---\n");
    for (int i = 0; i < MaxGThreads; i++) {
        if (gt_table[i].state != Unused || gt_table[i].stats.run_count > 0) {
            const char *state_color;
            switch (gt_table[i].state) {
                case Running: state_color = ANSI_COLOR_GREEN; break;
                case Ready: state_color = ANSI_COLOR_BLUE; break;
                case Unused: state_color = ANSI_COLOR_MAGENTA; break;
                default: state_color = ANSI_COLOR_YELLOW;
            }
            
            printf("Thread %d (%s%s%s):\n", i, 
                   state_color, 
                   gt_table[i].state == Running ? "Running" : 
                   gt_table[i].state == Ready ? "Ready" : 
                   gt_table[i].state == Unused ? "Unused" : "Unknown",
                   ANSI_COLOR_RESET);
            
            // Get colors for min/max runtime
            const char *min_runtime_color = get_color_for_value(
                gt_table[i].stats.min_runtime == LONG_MAX ? 0 : gt_table[i].stats.min_runtime,
                global_min_runtime, global_max_runtime);
            const char *max_runtime_color = get_color_for_value(
                gt_table[i].stats.max_runtime,
                global_min_runtime, global_max_runtime);
            
            // Get colors for min/max waittime
            const char *min_waittime_color = get_color_for_value(
                gt_table[i].stats.min_waittime == LONG_MAX ? 0 : gt_table[i].stats.min_waittime,
                global_min_waittime, global_max_waittime);
            const char *max_waittime_color = get_color_for_value(
                gt_table[i].stats.max_waittime,
                global_min_waittime, global_max_waittime);
            
            printf("  Min runtime: %s%ld%s μs, Max runtime: %s%ld%s μs\n", 
                   min_runtime_color, 
                   gt_table[i].stats.min_runtime == LONG_MAX ? 0 : gt_table[i].stats.min_runtime, 
                   ANSI_COLOR_RESET,
                   max_runtime_color,
                   gt_table[i].stats.max_runtime,
                   ANSI_COLOR_RESET);
            
            printf("  Min waittime: %s%ld%s μs, Max waittime: %s%ld%s μs\n", 
                   min_waittime_color,
                   gt_table[i].stats.min_waittime == LONG_MAX ? 0 : gt_table[i].stats.min_waittime, 
                   ANSI_COLOR_RESET,
                   max_waittime_color,
                   gt_table[i].stats.max_waittime,
                   ANSI_COLOR_RESET);
            
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