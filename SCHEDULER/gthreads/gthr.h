enum {
	MaxGThreads = 5,            // Maximum number of threads, used as array size for gttbl
	StackSize = 0x400000,       // Size of stack of each thread
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
	}
	state;
    
    // Thread statistics
    struct gt_stats stats;
};


void gt_init(void);                                                     // initialize gttbl
void gt_return(int ret);                                                // terminate thread
void gt_switch(struct gt_context * old, struct gt_context * new);       // declaration from gtswtch.S
bool gt_schedule(void);                                                 // yield and switch to another thread
void gt_stop(void);                                                     // terminate current thread
int gt_create(void( * f)(void));                                        // create new thread and set f as new "run" function
void gt_reset_sig(int sig);                                             // resets signal
void gt_alarm_handle(int sig);                                          // periodically triggered by alarm
int gt_uninterruptible_nanosleep(time_t sec, long nanosec);             // uninterruptible sleep
void gt_print_stats();                                                  // print thread statistics on SIGINT
