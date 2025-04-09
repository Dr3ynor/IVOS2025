// Based on https://c9x.me/articles/gthreads/code0.html
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "gthr.h"

// Dummy function to simulate some thread work
void f(void) {
	static int x;
	int i = 0, id;

	id = ++x;
	while (true) {

		printf("F Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("F Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

// Dummy function to simulate some thread work
void g(void) {
	static int x;
	int i = 0, id;

	id = ++x;
	while (true) {
		printf("G Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("G Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

int main(void) {
	select_algorithm();
	gt_init();            // initialize threads, see gthr.c
	gt_create(f, 1);         // set f() as first thread
	gt_create(f, MaxPriority);         // set f() as second thread
	gt_create(g, 5);         // set g() as third thread
	gt_create(g, 10);         // set g() as fourth thread
	gt_return(1);         // wait until all threads terminate
}
