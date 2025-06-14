enum {
    MaxGThreads = 5,     // Maximum number of threads, used as array size for gttbl
    StackSize = 0x400000, // Size of stack of each thread
    MaxPriority = 20,    // Maximum priority level (higher number = higher priority)
};

// Stats structure to track thread performance metrics
struct gt_stats {
    struct timeval created_at;      // When the thread was created
    struct timeval last_run;        // When the thread last started running
    struct timeval last_ready;      // When the thread was last set to ready
    
    long total_runtime;             // Total runtime in microseconds
    long total_waittime;            // Total time spent waiting in microseconds
    
    long min_runtime;               // Minimum runtime between switches
    long max_runtime;               // Maximum runtime between switches
    long sum_runtime;               // Sum of all runtimes for average calculation
    long sum_squared_runtime;       // Sum of squared runtimes for variance calculation
    long run_count;                 // Number of times the thread has run
    
    long min_waittime;              // Minimum time spent waiting
    long max_waittime;              // Maximum time spent waiting
    long sum_waittime;              // Sum of all waittimes for average calculation
    long sum_squared_waittime;      // Sum of squared waittimes for variance calculation
    long wait_count;                // Number of times the thread has waited
};

struct gt {
	// Saved context, switched by gtswtch.S (see for detail)
	struct gt_context {
		uint64_t rsp;
		uint64_t r15;
		uint64_t r14;
		uint64_t r13;
		uint64_t r12;
		uint64_t rbx;
		uint64_t rbp;
	}
	ctx;
	// Thread state
	enum {
		Unused,
		Running,
		Ready,
		Blocked,
	}
	state;
    
    // Thread statistics
    struct gt_stats stats;

	// Priority-related fields
	int base_priority;           // Original assigned priority
	int current_priority;        // Current priority (may be boosted to prevent starvation)
	struct timeval last_scheduled; // Last time this thread was scheduled
	int tickets;			 // Number of tickets for lottery scheduling (if used)
	int ticket_start; // Start of the ticket range for this thread
	int ticket_end;   // End of the ticket range for this thread	

};


struct waiting_queue {
    struct gt *thread;               // Pointer to the thread
    struct waiting_queue *next;      // Pointer to the next element in the queue
};

// Updated semaphore structure
struct semaphore_t {
    int value;                        // Current semaphore value
    struct waiting_queue *head;       // Head of the waiting queue
    struct waiting_queue *tail;       // Tail of the waiting queue
};


void gt_init(void);                                                     // initialize gttbl
void gt_return(int ret);                                                // terminate thread
void gt_switch(struct gt_context * old, struct gt_context * new);       // declaration from gtswtch.S
bool gt_schedule(void);                                                 // yield and switch to another thread
void gt_stop(void);                                                     // terminate current thread
int gt_create(void(*f)(void), int priority);                            // create new thread and set f as new "run" function
void gt_reset_sig(int sig);                                             // resets signal
void gt_alarm_handle(int sig);                                          // periodically triggered by alarm
int gt_uninterruptible_nanosleep(time_t sec, long nanosec);             // uninterruptible sleep
void gt_print_stats();                                                  // print thread statistics on SIGINT
long time_diff_us(struct timeval *start, struct timeval *end);         // calculate time difference in microseconds
void gt_init_stats(struct gt_stats *stats);                            // initialize thread statistics
double calculate_variance(long sum, long sum_squared, long count);    // calculate variance
void assign_tickets(struct gt *thread);                                // assign tickets for lottery scheduling
int select_winning_ticket();                                            // select winning ticket for lottery scheduling
const char* get_color_for_value(long value, long min, long max);       // get color for value based on min/max
void get_global_min_max(long *min_runtime, long *max_runtime, long *min_waittime, long *max_waittime); // get global min/max values
void gt_print_stats();                                                  // print thread statistics
void select_algorithm();                                                  // select scheduling algorithm

void update_runtime_stats(struct gt *thread, struct timeval *now);
void update_waittime_stats(struct gt *thread, struct timeval *now);
void prepare_for_switch(struct gt *current, struct gt *next, struct timeval *now);
bool round_robin_select(struct gt **selected);
bool priority_based_select(struct gt **selected);
bool lottery_scheduling_select(struct gt **selected);


void gt_sem_init(struct semaphore_t* sem, int initial_value);
void gt_sem_wait(struct semaphore_t* sem); // "P" operace
void gt_sem_post(struct semaphore_t* sem); // "V" operace
