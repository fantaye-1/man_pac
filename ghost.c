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
	ghost->move_dir = DIR_RIGHT;
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

// Sets next position of this ghost.
coord_t
set_next_position(int screen_w, int screen_h)
{
	coord_t pos = ghost->pos;
	int move_dir = ghost->move_dir;
	screen_h -= 20 + ghost_height; // Offset for menubar height
	int y_unit = screen_h / 30; // vertical movement unit
	int x_unit = screen_w / 30; // horizontal movement unit

	if (pos.x == 0 && pos.y == 0) // Uninitialized
	{
		pos.x = x_unit + ghost->order * ghost_width;
		pos.y = 20 + y_unit; // Offset for menubar height

		ghost->pos = pos;
		return pos;
	}

	// Move one pixel toward predefined direction
	switch (move_dir)
	{
		case DIR_UP: // Move up
			pos.y -= y_unit;
			break;
		case DIR_DOWN: // Move down
			pos.y += y_unit;
			break;
		case DIR_LEFT: // Move left
			pos.x -= x_unit;
			break;
		case DIR_RIGHT: // Move right
			pos.x += x_unit;
			break;
	}

	// Hard coded paths. Ghosts will move clockwise on the
	//	perimeter line specified in the condition values.
	switch (ghost->order)
	{
		case 0: // Circles around top left
			// @ Top boundary: move right
			if (move_dir == DIR_UP && pos.y < (y_unit * 2))
			{
				move_dir = DIR_RIGHT;
			}
			// @ Left boundary: move to top
			if (move_dir == DIR_LEFT && pos.x < x_unit)
			{
				move_dir = DIR_UP;
			}
			// @ Right boundary: move to bottom
			if (move_dir == DIR_RIGHT && pos.x > (screen_w / 2 - x_unit - ghost_width))
			{
				move_dir = DIR_DOWN;
			}
			// @ Bottom boundary: move to left
			if (move_dir == DIR_DOWN && pos.y > (screen_h / 2))
			{
				move_dir = DIR_LEFT;
			}
			break;
		case 1: // Circles around top right
			// @ Top boundary: move right
			if (move_dir == DIR_UP && pos.y < (y_unit * 2))
			{
				move_dir = DIR_RIGHT;
			}
			// @ Left boundary: move to top
			if (move_dir == DIR_LEFT && pos.x < (screen_w / 2 - x_unit * 2))
			{
				move_dir = DIR_UP;
			}
			// @ Right boundary: move to bottom
			if (move_dir == DIR_RIGHT &&
					pos.x > (screen_w - x_unit - ghost_width))
			{
				move_dir = DIR_DOWN;
			}
			// @ Bottom boundary: move to left
			if (move_dir == DIR_DOWN && pos.y > screen_h / 2)
			{
				move_dir = DIR_LEFT;
			}
			break;
		case 2: // Circles around bottom left
			// @ Top boundary: move right
			if (move_dir == DIR_UP && pos.y < (screen_h / 2 + y_unit))
			{
				move_dir = DIR_RIGHT;
			}
			// @ Left boundary: move to top
			if (move_dir == DIR_LEFT && pos.x < x_unit)
			{
				move_dir = DIR_UP;
			}
			// @ Right boundary: move to bottom
			if (move_dir == DIR_RIGHT && pos.x > (screen_w / 2 - x_unit * 2))
			{
				move_dir = DIR_DOWN;
			}
			// @ Bottom boundary: move to left
			if (move_dir == DIR_DOWN &&
					pos.y > (screen_h - ghost_height + y_unit))
			{
				move_dir = DIR_LEFT;
			}
			break;
		case 3:	// Circles around bottom right
			// @ Top boundary: move right
			if (move_dir == DIR_UP && pos.y < (screen_h / 2 + y_unit))
			{
				move_dir = DIR_RIGHT;
			}
			// @ Left boundary: move to top
			if (move_dir == DIR_LEFT && pos.x < (screen_w / 2 - x_unit * 2))
			{
				move_dir = DIR_UP;
			}
			// @ Right boundary: move to bottom
			if (move_dir == DIR_RIGHT &&
					pos.x > (screen_w - x_unit - ghost_width))
			{
				move_dir = DIR_DOWN;
			}
			// @ Bottom boundary: move to left
			if (move_dir == DIR_DOWN &&
					pos.y > (screen_h - ghost_height + y_unit))
			{
				move_dir = DIR_LEFT;
			}
	}

	ghost->pos = pos;
	ghost->move_dir = move_dir;

	return pos;
}

// Window/GUI loop
void
window_loop(void)
{
	// Display params
	Display *disp;
	int screen_id;
	int screen_w;
	int screen_h;
	Colormap colormap;

	// X11 Window
	Window win;
	XSizeHints *size;
	XEvent ev;
	Pixmap pixmap;
	Atom win_type_key;
	Atom win_type_val;

	// Ghost
	XColor color, _nop_color; // Alloc'd ghost color

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
	colormap = DefaultColormap(disp, screen_id);

	// Setup ghost color
	switch (ghost->order)
	{
		case 0: // Ghost 0 is Red
			XAllocNamedColor(disp, colormap, // Display, color map
					"red", &color, &_nop_color);
			break;
		case 1: // Ghost 1 is Green
			XAllocNamedColor(disp, colormap, // Display, color map
					"green", &color, &_nop_color);
			break;
		case 2: // Ghost 2 is Purple
			XAllocNamedColor(disp, colormap, // Display, color map
					"purple", &color, &_nop_color);
			break;
		case 3: // Ghost 3 is Blue
			XAllocNamedColor(disp, colormap, // Display, color map
					"blue", &color, &_nop_color);
			break;
	}

	// Set initial position of ghost
	sem_wait(&ghost->mutex);
	set_next_position(screen_w, screen_h);
	sem_post(&ghost->mutex);

	// Create window
	win = XCreateSimpleWindow(disp, // Display
			XRootWindow(disp, screen_id), // Framing window
			ghost->pos.x, ghost->pos.y, // Coordinates
			ghost_width, ghost_height, // Dimensions
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
			ghost_bits, // Ghost bitmap
			ghost_width, // Ghost width
			ghost_height); // Ghost height
	XShapeCombineMask(disp, // Display
			win, // Window
			ShapeBounding, // Bound window to shape
			0, 0, // Mask x, y offset
			pixmap, // Mask pixmap
			ShapeSet); // Do set shape operation

	XMapWindow(disp, win); // Draw window
	XFlush(disp); // Blit

	while (ghost->pid != 0) // While alive
	{
		usleep(100 * 1000); // Wait .1 seconds (in microseconds);
		sem_wait(&ghost->mutex);
		set_next_position(screen_w, screen_h);
		sem_post(&ghost->mutex);
		XMoveWindow(disp, win, ghost->pos.x, ghost->pos.y); // Move window
		XFlush(disp); // Blit
	}

	XDestroyWindow(disp, win);
	XCloseDisplay(disp);
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
	window_loop();

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
