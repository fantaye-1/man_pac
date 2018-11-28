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

void *target_struct;

// Window/GUI loop
// Each paramter accepts any struct pointer that will be casted 
// according to the type mentioned in the second parameter.
// struct_type = 0: ghost/ struct_type = 1: manpac
void
window_loop(void *incoming_struct, char struct_type)
{
	target_struct = incoming_struct;
	
	// Display params
	Display *disp;
	int screen_id;
	int screen_w;
	int screen_h;
	int x_unit, y_unit;
	int i, last_count, x, y;
	int count;
	int once;
	int coord_x;
	int coord_y;
	Colormap colormap;
	ghost_t *target_ghost; // Target ghost

	// X11 Window
	Window win;
	XSizeHints *size;
	XEvent ev;
	Pixmap pixmap;
	Atom win_type_key;
	Atom win_type_val;

	XColor color, _nop_color; // Alloc'd color
	if (struct_type) // Manpac
	{
		count = 4; // Number of ghosts remaining
		once = 0; // Print ghosts once in the begging
	}

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

	if (struct_type) // Manpac
	{
		x_unit = screen_w / 30;
		y_unit = screen_h / 30;
		
		// Center in screen
		x = (screen_w - manpac_width) / 2;
		y = (screen_h - manpac_height) / 2;
		
		// Setup manpac color
		XAllocNamedColor(disp, colormap, // Display, color map
						"yellow", &color, &_nop_color);
						
		coord_x = 0;
		coord_y = 0;
		#define target_width manpac_width
		#define target_height manpac_height
		#define target_bits manpac_bits
	}
	else // Ghosts
	{

		// Setup ghost color
		switch (((ghost_t*)target_struct)->order)
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
		sem_wait(&((ghost_t*)target_struct)->mutex);
		set_next_position(screen_w, screen_h);
		sem_post(&((ghost_t*)target_struct)->mutex);
		
		coord_x = ((ghost_t*)target_struct)->pos.x;
		coord_y = ((ghost_t*)target_struct)->pos.y;
		#ifndef target_width
		#define target_width ghost_width
		#endif
		#ifndef target_height
		#define target_height ghost_height
		#endif
		#ifndef target_bits
		#define target_bits ghost_bits
		#endif
	}

	// Create window
	win = XCreateSimpleWindow(disp, // Display
			XRootWindow(disp, screen_id), // Framing window
			coord_x, coord_y, // Coordinates
			target_width, target_height, // Dimensions
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
			target_bits, // Ghost bitmap
			target_width, // Ghost width
			target_height); // Ghost height
	XShapeCombineMask(disp, // Display
			win, // Window
			ShapeBounding, // Bound window to shape
			0, 0, // Mask x, y offset
			pixmap, // Mask pixmap
			ShapeSet); // Do set shape operation

	XMapWindow(disp, win); // Draw window
	XFlush(disp); // Blit

	/* Event Loop */
	if (struct_type) // Manpac
	{
		while (count) // While ghosts remain
		{
			last_count = count;
			count = 0;
			for (i = 0; i < 4; i++)
			{
				if (((shared_t*)target_struct)->ghosts[i].pid)
				{
					count++;
				}
			}

			if (!once || last_count != count)
			{
				printf("%d Ghosts remaining:\n", count);
				for (i = 0; i < 4; i++)
				{
					if (((shared_t*)target_struct)->ghosts[i].pid)
					{
						print_color_ghost(((shared_t*)target_struct)->ghosts[i].pid,
								((shared_t*)target_struct)->ghosts[i].order);
					}
				}
				once = 1; // Ran at least once
			}

			target_ghost = collided_ghost(x, y);
			if (target_ghost && target_ghost->pid)
			{
				kill(target_ghost->pid, SIGTERM);
			}

			switch (((shared_t*)target_struct)->manpac_dir)
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
	}
	else // Ghost
	{
	while ( ((ghost_t *)target_struct)->pid != 0) // While alive
		{
			usleep(100 * 1000); // Wait .1 seconds (in microseconds);
			sem_wait(&((ghost_t*)target_struct)->mutex);
			set_next_position(screen_w, screen_h);
			sem_post(&((ghost_t*)target_struct)->mutex);
			XMoveWindow(disp, win, ((ghost_t*)target_struct)->pos.x, ((ghost_t*)target_struct)->pos.y); // Move window
			XFlush(disp); // Blit
		}
	}
	
	XDestroyWindow(disp, win);
	XCloseDisplay(disp);
}

// Returns a Ghost that is collided with the rect <pos, dims> or NULL if none
ghost_t*
collided_ghost(int x, int y)
{
	ghost_t *ghost = NULL;
	int i;

	for (i = 0; i < 4; i++)
	{
		ghost = &((shared_t*)target_struct)->ghosts[i];

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

// Sets next position of this ghost.
coord_t
set_next_position(int screen_w, int screen_h)
{
	coord_t pos = ((ghost_t*)target_struct)->pos;
	int move_dir = ((ghost_t*)target_struct)->ghost_dir;
	screen_h -= 20 + ghost_height; // Offset for menubar height
	int y_unit = screen_h / 30; // vertical movement unit
	int x_unit = screen_w / 30; // horizontal movement unit

	if (pos.x == 0 && pos.y == 0) // Uninitialized
	{
		pos.x = x_unit + ((ghost_t*)target_struct)->order * ghost_width;
		pos.y = 20 + y_unit; // Offset for menubar height

		((ghost_t*)target_struct)->pos = pos;
		return pos;
	}

	// Move one pixel toward predefined direction
	switch (((ghost_t*)target_struct)->ghost_dir)
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
	switch (((ghost_t*)target_struct)->order)
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

	((ghost_t*)target_struct)->pos = pos;
	((ghost_t*)target_struct)->ghost_dir = move_dir;

	return pos;
}

