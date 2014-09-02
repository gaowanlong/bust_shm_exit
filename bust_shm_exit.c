/*
 * Create a bunch of shared memory segments, then a bunch of threads
 * that continually clone and exit.
 *
 * Should see tasks start getting blocked in exit_shm:
 *
 *	exit_shm+0x4c/0xa0
 *	do_exit+0x2f4/0xb70
 *	do_group_exit+0x54/0xf0
 *
 * Build with:
 *
 * gcc -O2 -o bust_shm_exit bust_shm_exit.c -lpthread
 *
 * Copyright (C) 2013 Anton Blanchard <anton@au.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define LENGTH (64 * 1024)

#define KEY_START 0x10000000

static void new_shm_segment(int key)
{
	int flags = SHM_R | SHM_W;
	void *shmaddr;
	int shmid;

	shmid = shmget(key, LENGTH, IPC_CREAT | flags);
	if (shmid < 0) {
		perror("shmget");
		exit(1);
	}

	shmaddr = shmat(shmid, NULL, 0);
	if (shmaddr == (char *)-1) {
		perror("shmat");
		shmctl(shmid, IPC_RMID, NULL);
		exit(1);
	}
}

static unsigned long long parse_size(char *str)
{
	unsigned long long res;
	unsigned long long multiplier = 1;

	if (strchr(str, 'k') || strchr(str, 'K'))
		multiplier = 1000;
	else if (strchr(str, 'm') || strchr(str, 'M'))
		multiplier = 1000 * 1000;

	res = strtoull(str, NULL, 0) * multiplier;

	return res;
}

static void *do_nothing(void *junk)
{
	return NULL;
}

static void *doit(void *junk)
{
	int parent_pid = (unsigned long)junk;
	pthread_attr_t attr;

	pthread_attr_init(&attr);

	if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN)) {
		perror("pthread_attr_setstacksize");
		exit(1);
	}

	while (1) {
		pthread_t tid;

		if (pthread_create(&tid, &attr, do_nothing, NULL)) {
			perror("pthread_create");
			exit(1);
		}

		pthread_join(tid, NULL);

		usleep(1000);

		if (kill(parent_pid, 0))
			return;
	}
}

int main(int argc, char *argv[])
{
	unsigned long long nr_segments, nr_threads;
	unsigned long long i;

	if (argc != 3) {
		fprintf(stderr, "Usage: bust_exit_shm "
				"<nr_segments> <nr_threads>\n");
		exit(1);
	}

	nr_segments = parse_size(argv[1]);
	nr_threads = parse_size(argv[2]);

	for (i = 0; i < nr_segments; i++)
		new_shm_segment(KEY_START + i);

	for (i = 0; i < nr_threads - 1; i++) {
		pthread_attr_t attr;
		pthread_t tid;
		void *arg = (void *)(unsigned long)getpid();

		pthread_attr_init(&attr);

		if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN)) {
			perror("pthread_attr_setstacksize");
			exit(1);
		}

		if (pthread_create(&tid, &attr, doit, arg)) {
			perror("pthread_create");
			exit(1);
		}
	}

	doit(NULL);

	return 0;
}
