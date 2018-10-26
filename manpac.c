/*
 * A program -- TODO write what we do here
 *
 * Licensed under GPLv3.
 * Copyright (c) 2018 Abrham Fantaye, Chase Colman, Youngsoo Kang.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "shared.h"

static shared_t *shared;

// Spawns four ghosts
int
spawn_ghosts(void)
{
	int i;
	pid_t pid;

	for (i = 0; i < 4; i++)
	{
		pid = fork();
		if (pid < 0) // fork error
		{
			printf("Could not fork %d\n", i);
			return 1;
		}

		if (pid == 0) // child process
		{
			execlp("./ghost", "ghost", NULL); // execute ghost
		}
	}

	return 0;
}

// Teardown
void
destroy(int shm_fd)
{
	int i;

	munmap(shared, SHM_SIZE);
	shm_unlink(SHM_NAME);
	sem_destroy(&shared->ready);
	for (i = 0; i < 4; i++)
	{
		sem_destroy(&shared->ghosts[i].mutex);
	}
}

// SIGTERM handler
void
term_handler(int signal)
{
	int i;
	ghost_t *ghost;
	printf("Manpac received signal to terminate!\n");
	for (i = 0; i < 4; i++)
	{
		ghost = &shared->ghosts[i];
		sem_wait(&ghost->mutex);
		// Could result in a race condition in early startup
		// Realistically don't need to check all are 0
		ghost->pid = 0;
		sem_post(&ghost->mutex);
	}
}

int
main(int argc, char** argv)
{
	int error, shm_fd, i, count;
	ghost_t *ghost;
	struct sigaction action = {.sa_handler = term_handler};

	// Create shared memory for interprocess communication
	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (shm_fd == -1)
	{
		printf("Could not create shared memory\n");
		return EXIT_FAILURE;
	}

	// Set size of shared memory object
	ftruncate(shm_fd, SHM_SIZE);

	// Map shared memory
	shared = mmap(0, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);

	// Initialize mutex locks
	for (i = 0; i < 4; i++)
	{
		shared->ghosts[i].pid = 0;
		sem_init(&shared->ghosts[i].mutex, 1, 1);
	}

	// Setup SIGTERM handler
	sigaction(SIGTERM, &action, NULL);

	// Initialize ready semaphore
	sem_init(&shared->ready, 1, 0);

	// Spawn ghosts
	error = spawn_ghosts();
	if (error)
	{
		printf("Could not spawn ghosts\n");
		destroy(shm_fd);
		return EXIT_FAILURE;
	}

	// Wait until all Ghosts are ready
	printf("Waiting for Ghosts to initialize!\n");
	sem_wait(&shared->ready);
	printf("Watching for Ghosts!\n");

	count = 4;
	while (count)
	{
		count = 0;
		for (i = 0; i < 4; i++)
		{
			ghost = &shared->ghosts[i];
			if (ghost->pid != 0)
			{
				// TODO print ghost info here
				count++;
			}
		}
		printf("%d Ghosts remaining\n", count);
		sleep(1); // Refresh every second
	}


	printf("All Ghosts have terminated!\n");
	destroy(shm_fd);
	return EXIT_SUCCESS;
}
