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

// ANSI Colors
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define BLUE "\x1B[34m"
#define PURPLE "\x1B[35m"
#define RESET "\x1B[0m"

static shared_t *shared;
static int shm_fd, proc_fd;
static int dir = -1;

// Returns a Ghost that is collided with the rect <pos, dims> or NULL if none
ghost_t*
collided_ghost(int x, int y)
{
	ghost_t *ghost = NULL;
	int i;

	for (i = 0; i < 4; i++)
	{
		ghost = &shared->ghosts[i];

		if (x < ghost->pos.x + ghost_width &&
			y < ghost->pos.y + ghost_height &&
			x + manpac_width > ghost->pos.x &&
			y + manpac_height > ghost->pos.y)
		{
			return ghost;
		}
	}

	return NULL;
}

// Assign a ghost color and print
void
print_color_ghost(int pid, int order)
{	
	switch (order)
	{
		case 0: // Ghost 0 is Red
			printf(RED "%d" RESET "\n", pid);
			break;
		case 1: // Ghost 1 is Green
			printf(GREEN "%d" RESET "\n", pid);
			break;
		case 2: // Ghost 2 is Purple
			printf(PURPLE "%d" RESET "\n", pid);
			break;
		case 3: // Ghost 3 is Blue
			printf(BLUE "%d" RESET "\n", pid);
			break;
	}
}

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
			dir = DIR_UP;
			break;
		case DOWN_ARROW:
			dir = DIR_DOWN;
			break;
		case LEFT_ARROW:
			dir = DIR_LEFT;
			break;
		case RIGHT_ARROW:
			dir = DIR_RIGHT;
			break;
	}
}

// Window/GUI loop
void
window_loop(void)
{
	// Display params
	Display *disp;
	int screen_id, screen_w, screen_h;
	int x_unit, y_unit;
	Colormap colormap;

	// X11 Window
	Window win;
	XSizeHints *size;
	XEvent ev;
	Pixmap pixmap;
	Atom win_type_key;
	Atom win_type_val;

	// Manpac
	XColor color, _nop_color; // Alloc'd color
	int count = 4; // Number of ghosts remaining
	int once = 0; // Print ghosts once in the begging
	ghost_t *target; // Target ghost
	int i, last_count, x, y;

	// Setup display
	disp = XOpenDisplay(NULL);
	if (!disp)
	{
		printf("Could not open display!\n");
		return;
	}
	screen_id = DefaultScreen(disp);
	screen_w = DisplayWidth(disp, screen_id);
	screen_h = DisplayHeight(disp, screen_id);
	x_unit = screen_w / 30;
	y_unit = screen_h / 30;
	colormap = DefaultColormap(disp, screen_id);

	// Center in screen
	x = (screen_w - manpac_width) / 2;
	y = (screen_h - manpac_height) / 2;

	// Setup manpac color
	XAllocNamedColor(disp, colormap, // Display, color map
					"yellow", &color, &_nop_color);

	// Create window
	win = XCreateSimpleWindow(disp, // Display
			XRootWindow(disp, screen_id), // Framing window
			0, 0, // Coordinates
			manpac_width, manpac_height, // Dimensions
			0, // Border width
			color.pixel, // Border color
			color.pixel); // Background color
	// Render as "tooltip" to hide with no window decorations above everything
	win_type_key = XInternAtom(disp, "_NET_WM_WINDOW_TYPE", False);
	win_type_val = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
	XChangeProperty(disp, // Display
			win, // Window
			win_type_key, // Key
			XA_ATOM, // Type
			32, // Format
			PropModeReplace, // Replace property
			(unsigned char *)&win_type_val, // Value
			1); // Number of elements (scalar = 1)

	// Force size in window manager
	size = XAllocSizeHints();
	size->flags = PPosition; // Program-specified position
	XSetWMProperties(disp, // Display
			win, // Window
			NULL, NULL, NULL, 0,
			size, // Size hints
			NULL, NULL);

	// Shape mask from bitmap
	pixmap = XCreateBitmapFromData(disp, // Display
			win, // Window
			manpac_bits, // Ghost bitmap
			manpac_width, // Ghost width
			manpac_height); // Ghost height
	XShapeCombineMask(disp, // Display
			win, // Window
			ShapeBounding, // Bound window to shape
			0, 0, // Mask x, y offset
			pixmap, // Mask pixmap
			ShapeSet); // Do set shape operation

	XMapWindow(disp, win); // Draw window
	XFlush(disp); // Blit

	/* Event loop */
	while (count) // While ghosts remain
	{
		last_count = count;
		count = 0;
		for (i = 0; i < 4; i++)
		{
			if (shared->ghosts[i].pid)
			{
				count++;
			}
		}

		if (!once || last_count != count)
		{
			printf("%d Ghosts remaining:\n", count);
			for (i = 0; i < 4; i++)
			{
				if (shared->ghosts[i].pid)
				{
					print_color_ghost(shared->ghosts[i].pid,
							shared->ghosts[i].order);
				}
			}
			once = 1; // Ran at least once
		}

		target = collided_ghost(x, y);
		if (target && target->pid)
		{
			kill(target->pid, SIGTERM);
		}

		switch (dir)
		{
			case DIR_UP:
				if (y > y_unit * 2)
				{
					y -= y_unit;
				}
				break;
			case DIR_DOWN:
				if (y < screen_h - y_unit - manpac_height)
				{
					y += y_unit;
				}
				break;
			case DIR_LEFT:
				if (x >= x_unit)
				{
					x -= x_unit;
				}
				break;
			case DIR_RIGHT:
				if (x < screen_w - x_unit)
				{
					x += x_unit;
				}
				break;
		}

		XMoveWindow(disp, win, x, y); // Move window
		XFlush(disp); // Blit

		usleep(50 * 1000); // Wait .1 seconds (in microseconds);
	}

	XDestroyWindow(disp, win);
	XCloseDisplay(disp);
}

int
main(int argc, char** argv)
{
	int error, shm_fd, i;
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
	window_loop();

	printf("All Ghosts have terminated!\n");
	destroy();
	return EXIT_SUCCESS;
}
