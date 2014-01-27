/*
Copyright (C) 2012 rkd77

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
This library is based on the vshell.c from the libvterm library
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>

#include <vterm.h>

#include <curses.h>

static unsigned char display1[6144];
static unsigned char display2[6144];

static unsigned char display11[6144];
static unsigned char display22[6144];

static unsigned char bufor[12288 * 3];
static unsigned char font[2048];

static unsigned char *changes;

static WINDOW term_win;
static unsigned int to_refresh = 1;

static guint32 keyboard_map[256];
chtype acs_map[128];

#ifndef MAX_X
#define MAX_X 64
#endif

#define MAX_Y 24

bool
has_colors(void)
{
	//fprintf(stderr, "has_colors\n");
	return FALSE;
}

int
waddch(WINDOW *win, const chtype a)
{
	int x1 = win->_curx;
	int x = x1;
	unsigned char *display = (x & 1) ? display2 : display1;
	int y = win->_cury;
	int addres = a * 8;
	int pos, i;
	unsigned char z[8];

	if (win->_attrs & A_REVERSE)
	{
		for (i = 0; i < 8; i++)
		{
			z[i] = ~font[addres + i];
		}
	}
	else
	{
		for (i = 0; i < 8; i++)
		{
			z[i] = font[addres + i];
		}
	}

	x >>= 1;
	pos = (x & 31) + ((y & 7) << 5) + ((y & 24) << 8);

	for (i = 0; i < 8; i++)
	{
		display[pos] = z[i];
		pos += 256;
	}

	++x1;
	if (x1 >= MAX_X)
	{
		x1 = 0;
		++y;
		if (y == MAX_Y)
		{
			y = 0;
		}
	}
	win->_curx = x1;
	win->_cury = y;

	return 0;
}

int
wchgat(WINDOW *win, int n, attr_t attr, short color, const void *opts)
{
	int x = win->_curx;
	unsigned char *display = (x & 1) ? display2 : display1;
	int y = win->_cury;
	int pos;

	x >>= 1;
	pos = (x & 31) + ((y & 7) << 5) + ((y & 24) << 8) + 7 * 256;

	display[pos] = 255;
	return 1;
}

int
beep(void)
{
	//fprintf(stderr, "beep\n");
	return 0;
}

int
pair_content(short pair, short *f, short *b)
{
	//fprintf(stderr, "pair_content: pair = %d\n", pair);
	return 0;
}

int COLOR_PAIRS = 3;

int
wmove(WINDOW *win, int y, int x)
{
	//fprintf(stderr, "wmove: y=%d, x=%d\n", y, x);
	if (y >= 0 && y < MAX_Y)
	{
		win->_cury = y;
	}
	if (x >= 0 && x < MAX_X)
	{
		win->_curx = x;
	}
	return 0;
}

static void
writescr(void)
{
	unsigned int i;

	changes = bufor;
	for (i = 0; i < 6144; ++i)
	{
		if (display11[i] != display1[i])
		{
			*((unsigned short *)changes) = (unsigned short)(i + 16384);
			changes += 2;
			*((unsigned char *)changes) = display11[i] = display1[i];
			++changes;
		}
	}
	for (i = 0; i < 6144; ++i)
	{
		if (display22[i] != display2[i])
		{
			*((unsigned short *)changes) = (unsigned short)(i + 24576);
			changes += 2;
			*((unsigned char *)changes) = display22[i] = display2[i];
			++changes;
		}
	}
}

static void *
send_loop(void *arg)
{
	int sockfd = *(int *)arg;
	static int counter;

	while (TRUE)
	{
		if (to_refresh)
		{
			int pos, to_write;

			to_refresh = 0;
			writescr();

			for (pos = 0, to_write = (unsigned char *)changes - bufor; to_write;)
			{
				int sent = write(sockfd, bufor + pos, to_write);

				if (sent < 0)
				{
					fprintf(stderr, "sent = %d\n",sent);
					break;
				};
				pos += sent;
				to_write -= sent;
			}
		}
		else
		{
			static struct timespec t = {
				.tv_sec = 0,
				.tv_nsec = 10000000
			};
			nanosleep(&t, NULL);
		}
	}
}

static void
init_acs(void)
{
	int i;

	for (i = 0; i < 128; i++)
	{
		acs_map[i] = 32;
	}

	acs_map['a'] = 177;
	acs_map['x'] = 179;
	acs_map['u'] = 180;
	acs_map['k'] = 191;
	acs_map['j'] = 217;
	acs_map['m'] = 192;
	acs_map['v'] = 193;
	acs_map['n'] = 197;
	acs_map['l'] = 218;
	acs_map['t'] = 195;
	acs_map['w'] = 194;
	acs_map['q'] = 196;

}

static void
init_keyboard(void)
{
	guint32 i;

	for (i = 0; i < 256; i++)
	{
		keyboard_map[i] = i;
	}

	keyboard_map[13] = 10; // ENTER
	keyboard_map[12] = 127; // BACKSPACE
	keyboard_map[7] = 27; // ESC
	keyboard_map[10] = KEY_DOWN;
	keyboard_map[11] = KEY_UP;
	keyboard_map[8] = KEY_LEFT;
	keyboard_map[9] = KEY_RIGHT;
	keyboard_map[130] = 9;
}

int
main(int argc, char **argv)
{
	vterm_t *vterm;
	int i, j, ch;
	char *locale;
	ssize_t bytes;
	FILE *stream;
	struct sockaddr_in remoteaddr;
	struct hostent *he;
	int sockfd, sent;
	pthread_t fred;
	unsigned char key;

	if (argc < 2)
	{
		printf("Need a host as the arg\n");
		exit(255);
	}

	he = gethostbyname(argv[1]);
	if (!he)
	{
		perror("gethostbyname");
		exit(255);
	}

	stream = fopen("FONT8.BIN", "rb");
	if (!stream)
	{
		perror("fopen");
		exit(255);
	}
	fread(font, 1, 2048, stream);
	fclose(stream);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("socket");
		exit(255);
	}

	printf("Connecting...\n");
	memset(&remoteaddr, 0, sizeof(remoteaddr));
	remoteaddr.sin_family = AF_INET;
	remoteaddr.sin_port = htons(2000);
	memcpy(&(remoteaddr.sin_addr), he->h_addr, he->h_length);
	if (connect(sockfd, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0)
	{
		perror("connect");
		exit(255);
	}

	//fcntl(sockfd, F_SETFL, O_NONBLOCK);
	init_acs();
	init_keyboard();

	locale = setlocale(LC_ALL,"C");
	term_win._use_keypad = TRUE;

	/* create the terminal and have it run bash */
	vterm = vterm_create(MAX_X, 24, VTERM_FLAG_VT100);
	vterm_set_colors(vterm, COLOR_WHITE, COLOR_BLACK);
	vterm_wnd_set(vterm, &term_win);

	/* keep reading keypresses from the user and passing them to the terminal;
	* also, redraw the terminal to the window at each iteration */
	ch = '\0';

	pthread_create(&fred, NULL, send_loop, &sockfd);

	while (TRUE)
	{
		struct pollfd fd_array;
		int count, retval;
		int bytes = vterm_read_pipe(vterm);

		if (bytes > 0)
		{
			vterm_wnd_update(vterm);
			to_refresh = 1;
		}
		if (bytes == -1) break;

		fd_array.fd = sockfd;
		fd_array.events = POLLIN;
		// wait 10 millisecond for data on pty file descriptor.
		retval = poll(&fd_array, 1, 10);
		// no data or poll() error.
		if (retval <= 0)
		{
			continue;
		}

		count = read(sockfd, &key, 1);
		if (count == 1)
		{
			guint32 ch = keyboard_map[key];
			//printf("%d\n", key);
			vterm_write_pipe(vterm, ch);
		}
	}
	close(sockfd);
	return 0;
}
