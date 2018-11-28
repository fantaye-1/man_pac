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

static int proc_fd = -1;
static ghost_t *ghost;

// Initializes allocated Ghost
int
init_ghost(int order)
{
	sem_wait(&ghost->mutex);
	if (ghost->pid != 0) // Ghost already initialized
	{
		sem_post(&ghost->mutex);
		return 1;
	}

	ghost->pid = getpid();
	ghost->order = order;
	ghost->ghost_dir = DIR_RIGHT;
	ghost->pos.x = 0;
	ghost->pos.y = 0;
	
	sem_post(&ghost->mutex);

	return 0;
}

// SIGTERM handler
void
term_handler(int signal)
{
	printf("Ghost %d received signal to terminate!\n", ghost->order);
	if (proc_fd >= 0) {
		ioctl(proc_fd, REMOVE_GHOST_SIGNAL, ghost->pid);
	}
	sem_wait(&ghost->mutex);
	ghost->pid = 0;
	sem_post(&ghost->mutex);
}


int
main(int argc, char** argv)
{
	int error, shm_fd, i;
	shared_t *shared;
	pid_t sid;
	struct sigaction action = {.sa_handler = term_handler};

	sid = setsid(); // detach process
	if (sid < 0) // detach error
	{
		printf("Could not detach Ghost %d\n", getpid());
		return 1;
	}

	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	shared = mmap(0, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);

	for (i = 0; i < 4; i++)
	{
		ghost = &shared->ghosts[i];
		if (init_ghost(i) == 0)
		{
			break;
		}
	}

	if (ghost->pid == 0)
	{
		printf("Did not initialize Ghost!\n");
		return EXIT_FAILURE;
	}
	
	// Setup SIGTERM handler
	sigaction(SIGTERM, &action, NULL);

	if (ghost->order == 3) // 4th ghost initialized, ready
	{
		sem_post(&shared->ready);
	}
	printf("Ghost %d with PID %d reporting!\n", ghost->order, ghost->pid);

	// Signal to Kernel to add this ghost
	proc_fd = open("/proc", O_NONBLOCK);
	if (proc_fd < 0)
	{
		printf("Could not open /proc for ioctl\n");
	}
	else
	{
		// Add ghost to kernel list
		ioctl(proc_fd, ADD_GHOST_SIGNAL, ghost->pid);
	}

	// Run the window loop
	window_loop(ghost, 0);

	// Remove ghost from kernel list
	if (proc_fd >= 0) {
		ioctl(proc_fd, REMOVE_GHOST_SIGNAL, ghost->pid);
	}

	// Disable ghost
	sem_wait(&ghost->mutex);
	ghost->pid = 0;
	sem_post(&ghost->mutex);
	printf("Ghost %d leaving!\n", ghost->order);

	close(proc_fd);
	return EXIT_SUCCESS;
}

