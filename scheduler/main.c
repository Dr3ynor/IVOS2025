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

#define SHARED_BUFFER_SIZE 5

struct semaphore_t sem;
int shared_buffer[SHARED_BUFFER_SIZE];
int write_index = 0;
int read_index = 0;

void init_shared_buffer()
{
	for (int i = 0; i < SHARED_BUFFER_SIZE; i++)
	{
		shared_buffer[i] = -1; // -1 indicates empty slot
	}
}

void print_buffer()
{
	printf("Buffer: [");
	for (int i = 0; i < SHARED_BUFFER_SIZE; i++)
	{
		printf("%d", shared_buffer[i]);
		if (i < SHARED_BUFFER_SIZE - 1)
		{
			printf(", ");
		}
	}
	printf("]\n");
}

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
		gt_sem_wait(&sem); // wait for semaphore
		shared_buffer[write_index] = id; // produce item
		write_index = (write_index + 1) % SHARED_BUFFER_SIZE;
		// print_buffer();
		// sleep(1000); // simulate work
		gt_sem_post(&sem); // signal semaphore
	}
}

void g_sem(void)
{
	static int x;
	int i = 0, id;

	id = ++x;
	while (true)
	{
		gt_sem_wait(&sem); // wait for semaphore
		int item = shared_buffer[read_index]; // consume item
		read_index = (read_index + 1) % SHARED_BUFFER_SIZE;
		// print_buffer();
		gt_sem_post(&sem); // signal semaphore
	}
}


int main(void)
{
	select_algorithm();

	gt_init(); // initialize threads, see gthr.c

	gt_sem_init(&sem, 1); // initialize semaphore
	init_shared_buffer(); // initialize shared buffer
	print_buffer();		  // print initial buffer state

	/*
	gt_create(f, 1);		   // set f() as first thread
	gt_create(f, MaxPriority); // set f() as second thread
	gt_create(g, 5);		   // set g() as third thread
	gt_create(g, 10);		   // set g() as fourth thread
	
*/
	
	gt_create(f_sem, 10);		   // set f() as first thread
	gt_create(f_sem, MaxPriority); // set f() as second thread
	gt_create(f_sem, 5);		   // set g() as third thread
	gt_create(f_sem, 10);		   // set g() as fourth thread
	
	// TODO: ROUNDROBIN S SEMAFOREM NEFUNGUJE

	gt_return(1);			   // wait until all threads terminate
}
