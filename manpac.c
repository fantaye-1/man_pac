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
#include "shared_kernel.h"

static shared_t *shared;
static int shm_fd, proc_fd;

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
destroy(void)
{
	int i;

	munmap(shared, SHM_SIZE);
	shm_unlink(SHM_NAME);
	sem_destroy(&shared->ready);
	for (i = 0; i < 4; i++)
	{
		sem_destroy(&shared->ghosts[i].mutex);
	}

	// Signal to Kernel to remove manpac
	ioctl(proc_fd, REMOVE_MANPAC_SIGNAL, getpid());
	close(proc_fd);
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
	return;
}

// Arrow Key Handler
void
arrow_handler(int signal, siginfo_t *info, void *context)
{
	switch (info->si_code)
	{
		case UP_ARROW:
			shared->manpac_dir = DIR_UP;
			break;
		case DOWN_ARROW:
			shared->manpac_dir = DIR_DOWN;
			break;
		case LEFT_ARROW:
			shared->manpac_dir = DIR_LEFT;
			break;
		case RIGHT_ARROW:
			shared->manpac_dir = DIR_RIGHT;
			break;
	}
}

int
main(int argc, char** argv)
{
	int error, shm_fd, i;
	shared->manpac_dir = -1;
	struct sigaction term_action = {.sa_handler = term_handler};
	struct sigaction arrow_action = {
		.sa_sigaction = arrow_handler,
		.sa_flags = SA_SIGINFO
	};

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
	sigaction(SIGTERM, &term_action, NULL);
	sigaction(SIGUSR1, &arrow_action, NULL);

	// Initialize ready semaphore
	sem_init(&shared->ready, 1, 0);

	// Signal to Kernel to add manpac
	proc_fd = open("/proc", O_NONBLOCK);
	if (proc_fd < 0)
	{
		printf("Could not open /proc for ioctl\n");
	}
	else
	{
		// Add ghost to kernel list
		ioctl(proc_fd, ADD_MANPAC_SIGNAL, getpid());
	}

	// Spawn ghosts
	error = spawn_ghosts();
	if (error)
	{
		printf("Could not spawn ghosts\n");
		destroy();
		return EXIT_FAILURE;
	}

	// Wait until all Ghosts are ready
	printf("Waiting for Ghosts to initialize!\n");
	sem_wait(&shared->ready);
	printf("Watching for Ghosts!\n");

	// Run window/GUI loop
	window_loop(shared, 1);

	printf("All Ghosts have terminated!\n");
	destroy();
	return EXIT_SUCCESS;
}

