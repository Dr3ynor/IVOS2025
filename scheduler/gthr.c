#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <limits.h>

#include "gthr.h"
#include "gthr_struct.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"


int total_tickets = 0; // Total number of tickets for lottery scheduling

// PriorityBased
// RoundRobin
// LotteryScheduling
char *algorithms[] = {"RoundRobin", "PriorityBased", "LotteryScheduling"};

char *current_algorithm = "RoundRobin"; // Default scheduling algorithm

void select_algorithm()
{
    printf("Select scheduling algorithm:\n");

    for (int i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++)
    {
        printf("%d. %s\n", i, algorithms[i]);
    }
    printf("Enter the number of the algorithm you want to use: ");
    // Read the selected algorithm number
    int current_algorithm_number = 0;
    scanf("%d", &current_algorithm_number);

    // Validate the input
    if (current_algorithm_number < 0 || current_algorithm_number >= sizeof(algorithms) / sizeof(algorithms[0]))
    {
        printf("Invalid selection. Defaulting to RoundRobin.\n");
        current_algorithm = "RoundRobin";
    }
    else
    {
        current_algorithm = algorithms[current_algorithm_number];
    }
    printf("Selected scheduling algorithm: %s\n", current_algorithm);
}

void gt_sem_init(struct semaphore_t *sem, int initial_value)
{
    sem->value = initial_value; // Initialize semaphore value
    sem->waiting_count = 0;     // Initialize waiting count
    for (int i = 0; i < MaxGThreads; i++)
    {
        sem->waiting_threads[i] = NULL; // Initialize waiting threads array
    }
    printf("Semaphore initialized with value: %d\n", initial_value);
}

void gt_sem_wait(struct semaphore_t *sem)
{
    // printf("waiting thread %ld\n", gt_current - gt_table);
    
    // Block signals to create a critical section
    sigset_t old_mask, new_mask;
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGINT);
    sigaddset(&new_mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &new_mask, &old_mask);
    
    struct timeval now;
    gettimeofday(&now, NULL);

    // Check if semaphore value is greater than 0
    if (sem->value > 0)
    {
        sem->value--; // Decrement semaphore value
        // Restore the original signal mask before returning
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return;       // Exit if semaphore is available
    }

    // If semaphore is not available, block the thread
    gt_current->state = Blocked;
    sem->waiting_threads[sem->waiting_count++] = gt_current;

    // update waittime for the blocked thread
    long waittime = time_diff_us(&gt_current->stats.last_ready, &now);
    gt_current->stats.total_waittime += waittime;
    gt_current->stats.wait_count++;

    // Restore the signal mask before scheduling
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    //gt_schedule(); // switch to another thread
}

void gt_sem_post(struct semaphore_t *sem)
{
    // Block signals to create a critical section
    sigset_t old_mask, new_mask;
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGINT);
    sigaddset(&new_mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &new_mask, &old_mask);
    
    // Check if there are waiting threads
    if (sem->waiting_count > 0)
    {
        // Wake up the first waiting thread
        struct gt *thread_to_wake = sem->waiting_threads[0];
        for (int i = 0; i < sem->waiting_count - 1; i++)
        {
            sem->waiting_threads[i] = sem->waiting_threads[i + 1]; // Shift waiting threads
        }
        sem->waiting_count--; // Decrement waiting count

        thread_to_wake->state = Ready;                         // Set the woken thread to Ready
        gettimeofday(&thread_to_wake->stats.last_ready, NULL); // Update last ready time
    }
    else
    {
        sem->value++; // Increment semaphore value if no threads are waiting
    }
    
    // Restore the original signal mask
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

void assign_tickets(struct gt *thread)
{
    thread->tickets = thread->base_priority; // Assign tickets based on priority
    if (thread->tickets <= 0)
    {
        thread->tickets = 1; // Ensure at least one ticket
    }

    // Range of tickets for this thread
    thread->ticket_start = total_tickets + 1;
    thread->ticket_end = total_tickets + thread->tickets;

    total_tickets += thread->tickets;
}

int select_winning_ticket()
{
    if (total_tickets <= 0)
        return -1;

    return (rand() % total_tickets) + 1;
}

long time_diff_us(struct timeval *start, struct timeval *end)
{
    long diff = ((end->tv_sec - start->tv_sec) * 1000000) +
                (end->tv_usec - start->tv_usec);
    // Ensure time difference is never negative
    return diff > 0 ? diff : 0;
}

// Initialize thread statistics
void gt_init_stats(struct gt_stats *stats)
{
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

double calculate_variance(long sum, long sum_squared, long count)
{
    if (count <= 1)
    {
        return 0.0;
    }
    double mean = (double)sum / count;
    return ((double)sum_squared / count) - (mean * mean);
}

const char *get_color_for_value(long value, long min, long max)
{
    // If min and max are the same, or value is out of range, use reset color
    if (min == max || value < min || value > max)
    {
        return ANSI_COLOR_RESET;
    }

    // Calculate percentage of value between min and max
    double percentage = (double)(value - min) / (max - min);

    if (percentage < 0.33)
    {
        return ANSI_COLOR_GREEN;
    }
    else if (percentage < 0.66)
    {
        return ANSI_COLOR_YELLOW;
    }
    else
    {
        return ANSI_COLOR_RED;
    }
}

// function triggered periodically by timer (SIGALRM)
void gt_alarm_handle(int sig)
{
    gt_schedule();
}

// Function to get global min/max values for runtime and waittime
void get_global_min_max(long *min_runtime, long *max_runtime, long *min_waittime, long *max_waittime)
{
    *min_runtime = LONG_MAX;
    *max_runtime = 0;
    *min_waittime = LONG_MAX;
    *max_waittime = 0;

    struct timeval now;
    gettimeofday(&now, NULL);

    for (int i = 0; i < MaxGThreads; i++)
    {
        if (gt_table[i].state == Unused && gt_table[i].stats.run_count == 0)
        {
            continue;
        }

        // Calculate current metrics if thread is active
        long current_runtime = gt_table[i].state == Running ? time_diff_us(&gt_table[i].stats.last_run, &now) : 0;

        // Update min/max runtime
        long thread_min_runtime = gt_table[i].stats.min_runtime;
        long thread_max_runtime = gt_table[i].stats.max_runtime;

        if (thread_min_runtime != LONG_MAX && thread_min_runtime < *min_runtime)
        {
            *min_runtime = thread_min_runtime;
        }

        if (thread_max_runtime > *max_runtime)
        {
            *max_runtime = thread_max_runtime;
        }

        // Update min/max waittime
        long thread_min_waittime = gt_table[i].stats.min_waittime;
        long thread_max_waittime = gt_table[i].stats.max_waittime;

        if (thread_min_waittime != LONG_MAX && thread_min_waittime < *min_waittime)
        {
            *min_waittime = thread_min_waittime;
        }

        if (thread_max_waittime > *max_waittime)
        {
            *max_waittime = thread_max_waittime;
        }
    }

    // Handle case where no valid values were found
    if (*min_runtime == LONG_MAX)
        *min_runtime = 0;
    if (*min_waittime == LONG_MAX)
        *min_waittime = 0;
}

// SIGINT handler to print thread statistics
void gt_print_stats()
{
    struct timeval now;
    gettimeofday(&now, NULL);

    // Get global min/max values for color coding
    long global_min_runtime, global_max_runtime, global_min_waittime, global_max_waittime;
    get_global_min_max(&global_min_runtime, &global_max_runtime, &global_min_waittime, &global_max_waittime);

    printf("\n----- Thread Statistics -----\n");
    printf("%-6s %-12s %-12s %-12s %-12s %-12s %-12s %-12s %-8s\n",
           "ID", "State", "Runtime(μs)", "Waittime(μs)", "Avg Run(μs)", "Avg Wait(μs)", "Std. Run", "Std. Wait", "Curr/Base Priority (RR)");

    
    for (int i = 0; i < MaxGThreads; i++)
    {
        if (gt_table[i].state == Unused && gt_table[i].stats.run_count == 0)
        {
            continue; // Skip unused threads that have never run
        }

        // Calculate current metrics if thread is active
        long current_runtime = gt_table[i].state == Running ? time_diff_us(&gt_table[i].stats.last_run, &now) : 0;

        long current_waittime = gt_table[i].state == Ready ? time_diff_us(&gt_table[i].stats.last_ready, &now) : 0;

        // Check for negative values
        if (current_runtime < 0)
        {
            current_runtime = 0;
        }

        if (current_waittime < 0)
        {
            current_waittime = 0;
        }

        // Calculate total runtime and waittime including current period
        long total_runtime = gt_table[i].stats.total_runtime + current_runtime;
        long total_waittime = gt_table[i].stats.total_waittime + current_waittime;

        // Calculate averages
        double avg_runtime = gt_table[i].stats.run_count > 0 ? (double)gt_table[i].stats.sum_runtime / gt_table[i].stats.run_count : 0;

        double avg_waittime = gt_table[i].stats.wait_count > 0 ? (double)gt_table[i].stats.sum_waittime / gt_table[i].stats.wait_count : 0;

        // Calculate variances
        double var_runtime = calculate_variance(
            gt_table[i].stats.sum_runtime,
            gt_table[i].stats.sum_squared_runtime,
            gt_table[i].stats.run_count);

        double var_waittime = calculate_variance(
            gt_table[i].stats.sum_waittime,
            gt_table[i].stats.sum_squared_waittime,
            gt_table[i].stats.wait_count);
        var_runtime = sqrt(var_runtime);
        var_waittime = sqrt(var_waittime);
        var_waittime = sqrt(var_waittime);

        // Get state as string and color
        const char *state;
        const char *state_color;
        switch (gt_table[i].state)
        {
        case Running:
            state = "Running";
            state_color = ANSI_COLOR_GREEN;
            break;
        case Ready:
            state = "Ready";
            state_color = ANSI_COLOR_BLUE;
            break;
        case Unused:
            state = "Unused";
            state_color = ANSI_COLOR_MAGENTA;
            break;
        case Blocked:
            state = "Blocked";
            state_color = ANSI_COLOR_RED;
            break;
        default:
            state = "Unknown";
            state_color = ANSI_COLOR_YELLOW;
        }

        // Get colors for runtime and waittime
        const char *runtime_color = get_color_for_value(total_runtime, global_min_runtime, global_max_runtime);
        const char *waittime_color = get_color_for_value(total_waittime, global_min_waittime, global_max_waittime);
        const char *avg_runtime_color = get_color_for_value(
            avg_runtime,
            gt_table[i].stats.run_count > 0 ? (double)global_min_runtime / gt_table[i].stats.run_count : (double)global_min_runtime,
            gt_table[i].stats.run_count > 0 ? (double)global_max_runtime / gt_table[i].stats.run_count : (double)global_max_runtime
        );
        const char *avg_waittime_color = get_color_for_value(
            avg_waittime,
            gt_table[i].stats.wait_count > 0 ? (double)global_min_waittime / gt_table[i].stats.wait_count : (double)global_min_waittime,
            gt_table[i].stats.wait_count > 0 ? (double)global_max_waittime / gt_table[i].stats.wait_count : (double)global_max_waittime
        );
        // Priority color (red for low, yellow for medium, green for high)
        const char *priority_color;
        if (gt_table[i].current_priority > gt_table[i].base_priority)
        {
            // Boosted priority
            priority_color = ANSI_COLOR_CYAN;
        }
        else if (gt_table[i].base_priority < MaxPriority / 3)
        {
            priority_color = ANSI_COLOR_RED;
        }
        else if (gt_table[i].base_priority < 2 * MaxPriority / 3)
        {
            priority_color = ANSI_COLOR_YELLOW;
        }
        else
        {
            priority_color = ANSI_COLOR_GREEN;
        }

        printf("%-6d %s%-12s%s %s%-12ld%s %s%-12ld%s %s%-12.2f%s %s%-12.2f%s %-12.2f %-12.2f %s%d/%d%s\n",
               i,
               state_color, state, ANSI_COLOR_RESET,
               runtime_color, total_runtime, ANSI_COLOR_RESET,
               waittime_color, total_waittime, ANSI_COLOR_RESET,
               avg_runtime_color, avg_runtime, ANSI_COLOR_RESET,
               avg_waittime_color, avg_waittime, ANSI_COLOR_RESET,
               var_runtime, var_waittime,
               priority_color, gt_table[i].current_priority, gt_table[i].base_priority, ANSI_COLOR_RESET);
        // printf("Tickets: %d (Range: %d-%d)\n\n\n", gt_table[i].tickets, gt_table[i].ticket_start, gt_table[i].ticket_end);
    }

    printf("--- Additional Thread Info ---\n");
    printf("Current scheduling algorithm: %s\n", current_algorithm);

    for (int i = 0; i < MaxGThreads; i++)
    {
        if (gt_table[i].state != Unused || gt_table[i].stats.run_count > 0)
        {
            const char *state_color;
            switch (gt_table[i].state)
            {
            case Running:
                state_color = ANSI_COLOR_GREEN;
                break;
            case Ready:
                state_color = ANSI_COLOR_BLUE;
                break;
            case Unused:
                state_color = ANSI_COLOR_MAGENTA;
                break;
            default:
                state_color = ANSI_COLOR_YELLOW;
            }

            printf("Thread %d (%s%s%s):\n", i,
                   state_color,
                   gt_table[i].state == Running ? "Running" : gt_table[i].state == Ready ? "Ready"
                                                          : gt_table[i].state == Unused  ? "Unused"
                                                          : gt_table[i].state == Blocked ? "Blocked"
                                                                                         : "Unknown",
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
            printf("  Base priority: %d, Current priority: %d\n",
                   gt_table[i].base_priority, gt_table[i].current_priority);
            printf("  RSP: %lx\n", gt_table[i].ctx.rsp);
            printf("Tickets: %d (Range: %d-%d)\n\n\n", gt_table[i].tickets, gt_table[i].ticket_start, gt_table[i].ticket_end);
        }
    }
    printf("-----------------------------\n");
    printf("Total Tickets: %d\n", total_tickets);
    exit(0); // Exit after printing stats
}

// initialize first thread as current context
void gt_init(void)
{
    gt_current = &gt_table[0];                       // Initialize current thread with thread #0
    gt_current->state = Running;                     // Set current to running
    gt_init_stats(&gt_current->stats);               // Initialize statistics for the first thread
    gettimeofday(&gt_current->stats.last_run, NULL); // Record start time

    // Initialize priority
    gt_current->base_priority = MaxPriority;
    gt_current->current_priority = MaxPriority;
    gettimeofday(&gt_current->last_scheduled, NULL);

    srand(time(NULL)); // initialize random seed
    // Assign tickets for the current thread
    gt_current->tickets = MaxPriority; // Default number of tickets for the initial thread
    gt_current->ticket_start = 1;
    gt_current->ticket_end = gt_current->tickets;
    total_tickets = gt_current->tickets;

    signal(SIGALRM, gt_alarm_handle); // Register SIGALRM signal
    signal(SIGINT, gt_print_stats);   // Register SIGINT for statistics display
}

// exit thread
void __attribute__((noreturn)) gt_return(int ret)
{
    if (gt_current != &gt_table[0])
    { // if not an initial thread,
        // Update runtime before exiting
        struct timeval now;
        gettimeofday(&now, NULL);
        long runtime = time_diff_us(&gt_current->stats.last_run, &now);
        gt_current->stats.total_runtime += runtime;

        gt_current->state = Unused;               // set current thread as unused
        free((void *)(gt_current->ctx.rsp + 16)); // free the stack
        gt_schedule();                            // yield and make possible to switch to another thread
        assert(!"reachable");                     // this code should never be reachable ... (if yes, returning function on stack was corrupted)
    }
    while (gt_schedule())
        ; // if initial thread, wait for other to terminate
    exit(ret);
}

bool gt_schedule(void)
{
    struct gt_context *old, *new;
    struct timeval now;
    struct gt *selected = NULL;
    gettimeofday(&now, NULL);

    // Reset the alarm signal for the scheduler
    gt_reset_sig(SIGALRM);

    // Update statistics for current running thread
    update_runtime_stats(gt_current, &now);

    // Select next thread based on scheduling algorithm
    if (strcmp(current_algorithm, "RoundRobin") == 0) {
        bool found = round_robin_select(&selected);
        if (!found) return false;
    } 
    else if (strcmp(current_algorithm, "PriorityBased") == 0) {
        bool found = priority_based_select(&selected);
        if (!found) return false;
    } 
    else if (strcmp(current_algorithm, "LotteryScheduling") == 0) {
        bool found = lottery_scheduling_select(&selected);
        if (!found) return false;
    }
    
    // Update statistics for the newly selected thread
    update_waittime_stats(selected, &now);

    // Prepare for context switch
    prepare_for_switch(gt_current, selected, &now);

    // Perform the context switch
    old = &gt_current->ctx;         // Prepare pointers to context of current
    new = &selected->ctx;          // and new thread
    gt_current = selected;         // Switch current indicator to new thread
    gt_switch(old, new);           // Perform context switch
    
    return true;
}

// Updates runtime statistics for the given thread
void update_runtime_stats(struct gt *thread, struct timeval *now)
{
    if (thread->state == Running) {
        long runtime = time_diff_us(&thread->stats.last_run, now);
        thread->stats.total_runtime += runtime;
        thread->stats.run_count++;

        // Update min/max/sum/sum_squared for runtime
        if (runtime < thread->stats.min_runtime) {
            thread->stats.min_runtime = runtime;
        }
        if (runtime > thread->stats.max_runtime) {
            thread->stats.max_runtime = runtime;
        }
        thread->stats.sum_runtime += runtime;
        thread->stats.sum_squared_runtime += (runtime * runtime);
    }
}

// Updates waittime statistics for the thread that's about to run
void update_waittime_stats(struct gt *thread, struct timeval *now)
{
    if (thread->state == Ready) {
        long waittime = time_diff_us(&thread->stats.last_ready, now);
        thread->stats.total_waittime += waittime;
        thread->stats.wait_count++;

        // Update min/max/sum/sum_squared for waittime
        if (waittime < thread->stats.min_waittime) {
            thread->stats.min_waittime = waittime;
        }
        if (waittime > thread->stats.max_waittime) {
            thread->stats.max_waittime = waittime;
        }
        thread->stats.sum_waittime += waittime;
        thread->stats.sum_squared_waittime += (waittime * waittime);

        thread->current_priority = thread->base_priority; // Reset current priority to base
    }
}

// Prepare both threads for context switch
void prepare_for_switch(struct gt *current, struct gt *next, struct timeval *now)
{
    if (current->state != Unused) {
        current->state = Ready;
        gettimeofday(&current->stats.last_ready, NULL); // Record when thread became ready
    }

    next->state = Running;
    gettimeofday(&next->stats.last_run, NULL);      // Record when thread starts running
    gettimeofday(&next->last_scheduled, NULL);      // Update last scheduled time
}

// Round Robin scheduling algorithm implementation
bool round_robin_select(struct gt **selected)
{
    struct gt *p = gt_current;
    
    while (p->state != Ready) {
        if (++p == &gt_table[MaxGThreads])
            p = &gt_table[0];
        
        if (p == gt_current)
            return false; // No ready threads found
    }
    
    *selected = p;
    return true;
}

// Priority-based scheduling algorithm implementation
bool priority_based_select(struct gt **selected)
{
    struct gt *p;
    int highest_priority = 0;
    bool found = false;
    
    for (p = &gt_table[0]; p < &gt_table[MaxGThreads]; p++) {
        if (p->state == Ready) {
            // Apply starvation prevention: boost priority if skipped too many times
            p->current_priority++;

            // Cap the boosted priority
            if (p->current_priority > MaxPriority) {
                p->current_priority = MaxPriority;
            }

            // Select thread based on priority
            if (p->current_priority >= highest_priority) {
                highest_priority = p->current_priority;
                *selected = p;
                found = true;
            }
        }
    }
    
    return found;
}

// Lottery scheduling algorithm implementation
bool lottery_scheduling_select(struct gt **selected)
{
    if (total_tickets <= 0) {
        return false;
    }
    
    int winning_ticket = select_winning_ticket();
    struct gt *p;
    
    for (p = &gt_table[0]; p < &gt_table[MaxGThreads]; p++) {
        if ((p->state == Ready || p->state == Running) && 
            winning_ticket >= p->ticket_start && 
            winning_ticket <= p->ticket_end) {
            *selected = p;
            return true;
        }
    }
    
    return false; // No thread found with the winning ticket
}

// return function for terminating thread
void gt_stop(void)
{
    gt_return(0);
}

// create new thread by providing pointer to function that will act like "run" method
int gt_create(void (*f)(void), int priority)
{
    char *stack;
    struct gt *p;

    // Validate priority
    if (priority < 1 || priority > MaxPriority)
    {
        priority = MaxPriority / 2; // Default to medium priority if invalid
    }

    for (p = &gt_table[0];; p++)
    {                                    // Find an empty slot
        if (p == &gt_table[MaxGThreads]) // If we have reached the end, gt_table is full
            return -1;
        else if (p->state == Unused)
            break; // New slot was found
    }

    stack = malloc(StackSize); // Allocate memory for stack
    if (!stack)
        return -1;

    *(uint64_t *)&stack[StackSize - 8] = (uint64_t)gt_stop; // Put returning function gt_stop
    *(uint64_t *)&stack[StackSize - 16] = (uint64_t)f;      // Put provided function as main "run" function
    p->ctx.rsp = (uint64_t)&stack[StackSize - 16];          // Set stack pointer
    p->state = Ready;                                       // Set state

    // Initialize statistics for new thread
    gt_init_stats(&p->stats);
    gettimeofday(&p->stats.last_ready, NULL); // Record when thread became ready

    // Initialize priority fields
    p->base_priority = priority;
    p->current_priority = priority;

    assign_tickets(p); // Assign tickets for the new thread based on priority

    gettimeofday(&p->last_scheduled, NULL);

    return 0;
}

// resets SIGALRM signal
void gt_reset_sig(int sig)
{
    if (sig == SIGALRM)
    {
        alarm(0); // Clear pending alarms if any
    }

    sigset_t set;                         // Create signal set
    sigemptyset(&set);                    // Clear set
    sigaddset(&set, sig);                 // Set signal (we use SIGALRM)
    sigprocmask(SIG_UNBLOCK, &set, NULL); // Fetch and change the signal mask

    if (sig == SIGALRM)
    {
        ualarm(500, 500); // Schedule signal after given number of microseconds
    }
}

// uninterruptible sleep
int gt_uninterruptible_nanosleep(time_t sec, long nanosec)
{
    struct timespec req;
    req.tv_sec = sec;
    req.tv_nsec = nanosec;

    do
    {
        if (0 != nanosleep(&req, &req))
        {
            if (errno != EINTR)
                return -1;
        }
        else
        {
            break;
        }
    } while (req.tv_sec > 0 || req.tv_nsec > 0);
    return 0;
}
