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
	ghost->moveDir = 3; // Initialize to move to right direction
	// Ghosts will initially line up at the upper left screen
	ghost->pos.x = 5 + order * (ghost->dims.w + 5);
	ghost->pos.y = 5;
	
	sem_post(&ghost->mutex);

	return 0;
}

// SIGTERM handler
void
term_handler(int signal)
{
	printf("Ghost %d received signal to terminate!\n", ghost->order);
	sem_wait(&ghost->mutex);
	ghost->pid = 0;
	sem_post(&ghost->mutex);
}

// Determines next position of this ghost.
coord_t
next_position(ghost_t *ghost, dims_t *screen)
{
	coord_t new_pos = ghost->pos;

	// Move one pixel toward predefined direction
	switch (ghost->moveDir)
	{
		case 0: // Move up
			new_pos.y++;
			break;
		case 1: // Move down
			new_pos.y--;
			break;
		case 2: // Move left
			new_pos.x--;
			break;
		case 3: // Move right
			new_pos.x++;
	}

	// Hard coded paths. Ghosts will move clockwise on the
	//	perimeter line specified in the condition values.
	switch (ghost->order)
	{
		case 0: // Circles around top left
			if (new_pos.x < screen->w / 8) // @ Left boundary: move to top
			{
				ghost->moveDir = 0;
			}
			else if (new_pos.x > screen->w * 7 / 8) // @ Right boundary: move to bottom
			{
				ghost->moveDir = 1;
			}
			else if (new_pos.y < screen->h / 3) // @ Bottom boundary: move to left
			{
				ghost->moveDir = 2;
			}
			else if (new_pos.y > screen->h / 8) // @ Top boundary: move to right
			{
				ghost->moveDir = 3;
			}
			break;
		case 1: // Circles around top right
			if (new_pos.x < screen->w * 2 / 3)
			{
				ghost->moveDir = 0;
			}
			else if (new_pos.x > screen->w * 7 / 8)
			{
				ghost->moveDir = 1;
			}
			else if (new_pos.y < screen->h / 3)
			{
				ghost->moveDir = 2;
			}
			else if (new_pos.y > screen->h / 8)
			{
				ghost->moveDir = 3;
			}
			break;
		case 2: // Circles around low middle
			if (new_pos.x < screen->w / 3)
			{
				ghost->moveDir = 0;
			}
			else if (new_pos.x > screen->w * 2 / 3)
			{
				ghost->moveDir = 1;
			}
			else if (new_pos.y < screen->h / 8)
			{
				ghost->moveDir = 2;
			}
			else if (new_pos.y > screen->h * 2 / 3)
			{
				ghost->moveDir = 3;
			}
			break;
		case 3:	// Circles around the entire screen
			if (new_pos.x < screen->w / 4)
			{
				ghost->moveDir = 0;
			}
			else if (new_pos.x > screen->w * 3 / 4)
			{
				ghost->moveDir = 1;
			}
			else if (new_pos.y < screen->h / 4)
			{
				ghost->moveDir = 2;
			}
			else if (new_pos.y > screen->h * 3 / 4)
			{
				ghost->moveDir = 3;
			}
	}
	return new_pos;
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
	sleep(5);

	// Disable ghost
	sem_wait(&ghost->mutex);
	ghost->pid = 0;
	sem_post(&ghost->mutex);
	printf("Ghost %d leaving!\n", ghost->order);

	return EXIT_SUCCESS;
}
