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


struct semaphore_t sem;

// Dummy function to simulate some thread work
void f(void)
{
	static int x;
	int i = 0, id;

	id = ++x;
	while (true)
	{

		printf("F Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("F Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

// Dummy function to simulate some thread work
void g(void)
{
	static int x;
	int i = 0, id;

	id = ++x;
	while (true)
	{
		printf("G Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("G Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

void f_sem(void)
{
	static int x;
	int i = 0, id;
	id = ++x;
	while (true)
	{

		// printf("Semaphore value before WAIT: %d\n", sem.value);
		gt_sem_wait(&sem);
		printf("SEMAPHORE IN CRITICAL SECTION WITH ID %d\n",id);


		// printf("Semaphore value: %d\n", sem.value);
		// printf("F Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		// printf("F Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);

		gt_sem_post(&sem); // Signal semaphore
	}
}

void g_sem(void)
{
	static int x;
	int i = 0, id;
	id = ++x;

	while (true)
	{
		gt_sem_wait(&sem);

		printf("G Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("G Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);

		gt_sem_post(&sem); // Signal semaphore
	}
}


int main(void)
{
	select_algorithm();

	gt_init(); // initialize threads, see gthr.c

	gt_sem_init(&sem, 1); // initialize semaphore

	/*
	gt_create(f, 1);		   // set f() as first thread
	gt_create(f, MaxPriority); // set f() as second thread
	gt_create(g, 5);		   // set g() as third thread
	gt_create(g, 10);		   // set g() as fourth thread
	*/
	
	gt_create(f_sem, 18);		   // set f() as first thread
	gt_create(f_sem, 12); // set f() as second thread
	gt_create(f_sem, 6);		   // set g() as third thread
	gt_create(f_sem, 2);		   // set g() as fourth thread
	
	// TODO: ROUNDROBIN S SEMAFOREM NEFUNGUJE

	gt_return(1);			   // wait until all threads terminate
}
