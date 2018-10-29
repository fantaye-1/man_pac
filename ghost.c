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

// Following defines and typedef describe a set of flags:
//	from bit 3 to 0: (colliding) top, bot, left, right.
// Thus, when return value is:
//	0: valid position, 1: (colliding) right, 2: left, 3: right & left ... so on.
#define COLLISION_TOP 8
#define COLLISION_BOT 4
#define COLLISION_LEFT 2
#define COLLISION_RIGHT 1

typedef unsigned char collision_side_t;

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
	ghost->moveBehavior = order; // Initial movement behavior is determined by the order#
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

// Checks if which side the ghost is colliding to.
//	return value of 0 indicates no collision, otherwise collision on some sides.
collision_side_t
ghost_collision_check(dims_t *screen, int xTemp, int yTemp)
{
	collision_side_t result = 0;

	//FIXME: need collision check with other ghosts

	// Top side check
	if (yTemp > screen->y)
		result = result | COLLISION_TOP;

	// Bot side check
	if (yTemp < 0)
		result = result | COLLISION_BOT;

	// Left side check
	if (xTemp < 0)
		result = result | COLLISION_LEFT;

	// Right side check
	if (xTemp > screen->x)
		result = result | COLLISION_RIGHT;

	return result;
}

// Determines next position of this ghost.
// Explanation:
//	Ghost will move in the diagonal directions and deflect perpendicularly on collision.
//	moveBehavior variable determines which diagonal direction:
//		0: upper left, 1: upper right, 2: lower left, 3: lower right
//	moveBehavior variable will change so that ghost will be deflected in
//		perpendicular way if it collides.
coord_t
next_position(ghost_t *ghost, dims_t *screen)
{
	collision_side_t collisionCheck = 0;

	do
	{
		// Changes moveBehavior variable depending on the colliding side
		//	If no side is in collision (initial loop), leave it unchanged.
		if (collisionCheck & COLLISION_TOP)
			ghost->moveBehavior = ghost->moveBehavior + 2;
		if (collisionCheck & COLLISION_BOT)
			ghost->moveBehavior = ghost->moveBehavior - 2;
		if (collisionCheck & COLLISION_LEFT)
			ghost->moveBehavior++;
		if (collisionCheck & COLLISION_RIGHT)
			ghost->moveBehavior--; 

		int xTemp = ghost->pos->x;
		int yTemp = ghost->pos->y;

		// Move ghost in a way decided by moveBehavior variable.
		if (ghost->moveBehavior % 2 == 0)
			--xTemp;
		else
			++xTemp;

		if (ghost->moveBehavior < 2)
			++yTemp;
		else
			--yTemp;
		}
		// Loop executes again when collisionCheck indicates some collision
		while (collisionCheck = ghost_collision_check(*screen, xTemp, yTemp));

	coord_t new_pos = {.x = xTemp, .y = yTemp};
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
