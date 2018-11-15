#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
// Mapped, shared memory
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
// ioctl
#include <sys/ioctl.h>
// X11/GUI
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
// Bitmaps
#include "ghost.xbm"
#include "manpac.xbm"

// Represents a coordinate pair
typedef struct {
	int x;
	int y;
} coord_t;

// Represents a Ghost process
typedef struct {
	pid_t pid; // Process ID
	coord_t pos; // Pixel-based position
	int order; // Order of instantiation
	int move_dir; // Movement direction - 0:up, 1:down, 2:left, 3:right

	sem_t mutex; // Mutual exclusion lock
} ghost_t;

// Shared memory name
#define SHM_NAME "ManpacGhost2018"

// Shared memory data type
typedef struct {
	ghost_t ghosts[4];
	sem_t ready; // Ready semaphore
} shared_t;

#define SHM_SIZE sizeof(shared_t)
