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
static unsigned char font[2048];

static WINDOW term_win;
static unsigned int to_refresh = 1;

static guint32 keyboard_map[256];
chtype acs_map[128];

#define MAX_X 85
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
	int y = win->_cury;
	int which = (x & 7);
	int addres = (a & 255) * 8;
	int pos, i;
	unsigned char z[8];

//fprintf(stderr, "win->_attrs = %d\n", win->_attrs);
	if (win->_attrs & A_REVERSE) {
		for (i = 0; i < 8; i++) {
			z[i] = ~font[addres + i];
		}
	} else {
		for (i = 0; i < 8; i++) {
			z[i] = font[addres + i];
		}
	}

	x = (x * 6) >> 4;
	pos = (x & 31) + ((y & 7) << 5) + ((y & 24) << 8);

	switch (which) {
	case 0:
		for (i = 0; i < 8; i++) {
		display1[pos] = (display1[pos] & 3) | (z[i] & 252);
		pos += 256;
		}
		break;

	case 1:
		for (i = 0; i < 8; i++) {
			display1[pos] = (display1[pos] & 252) | ((z[i] >> 6) & 3);
			display2[pos] = (display2[pos] & 15) | ((z[i] << 2) & 240);
			pos += 256;
		}
		break;

	case 2:
		for (i = 0; i < 8; i++) {
			display2[pos] = (display2[pos] & 240) | ((z[i] >> 4) & 15);
			display1[pos + 1] = (display1[pos + 1] & 63) | ((z[i] << 4) & 192);
			pos += 256;
		}
		break;

	case 3:
		for (i = 0; i < 8; i++) {
			display1[pos] = (display1[pos] & 192) | ((z[i] >> 2) & 63);
			pos += 256;
		}
		break;

	case 4:
		for (i = 0; i < 8; i++) {
			display2[pos] = (display2[pos] & 3) | (z[i]  & 252);
			pos += 256;
		}
		break;

	case 5:
		for (i = 0; i < 8; i++) {
			display2[pos] = (display2[pos] & 252) | ((z[i] >> 6) & 3);
			display1[pos + 1] = (display1[pos + 1] & 15) | ((z[i] << 2) & 240);
			pos += 256;
		}
		break;

	case 6:
		for (i = 0; i < 8; i++) {
			display1[pos] = (display1[pos] & 240) | ((z[i] >> 4) & 15);
			display2[pos] = (display2[pos] & 63) | ((z[i] << 4) & 192);
			pos += 256;
		}
		break;

	case 7:
		for (i = 0; i < 8; i++) {
			display2[pos] = (display2[pos] & 192) | ((z[i] >> 2) & 63);
			pos += 256;
		}
		break;

	default:
		break;
	}

	++x1;
	if (x1 >= MAX_X) {
		x1 = 0;
		++y;
		if (y == MAX_Y) {
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
	/* show cursor */
	int x = win->_curx;
	int y = win->_cury;
	int which = (x & 7);
	int pos, i;
	static int UNDERLINE = 252;

	x = (x * 6) >> 4;
	pos = (x & 31) + ((y & 7) << 5) + ((y & 24) << 8) + 7 * 256;

	switch (which) {
	case 0:
		display1[pos] = (display1[pos] & 3) | (UNDERLINE & 252);
		break;

	case 1:
		display1[pos] = (display1[pos] & 252) | ((UNDERLINE >> 6) & 3);
		display2[pos] = (display2[pos] & 15) | ((UNDERLINE << 2) & 240);
		break;

	case 2:
		display2[pos] = (display2[pos] & 240) | ((UNDERLINE >> 4) & 15);
		display1[pos + 1] = (display1[pos + 1] & 63) | ((UNDERLINE << 4) & 192);
		break;

	case 3:
		display1[pos] = (display1[pos] & 192) | ((UNDERLINE >> 2) & 63);
		break;

	case 4:
		display2[pos] = (display2[pos] & 3) | (UNDERLINE  & 252);
		break;

	case 5:
		display2[pos] = (display2[pos] & 252) | ((UNDERLINE >> 6) & 3);
		display1[pos + 1] = (display1[pos + 1] & 15) | ((UNDERLINE << 2) & 240);
		break;

	case 6:
		display1[pos] = (display1[pos] & 240) | ((UNDERLINE >> 4) & 15);
		display2[pos] = (display2[pos] & 63) | ((UNDERLINE << 4) & 192);
		break;

	case 7:
		display2[pos] = (display2[pos] & 192) | ((UNDERLINE >> 2) & 63);
		break;

	default:
		break;
	}
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
	if (y >= 0 && y < MAX_Y) {
		win->_cury = y;
	}
	if (x >= 0 && x < MAX_X) {
		win->_curx = x;
	}
	return 0;
}


static void *
send_loop(void *arg)
{
	int sockfd = *(int *)arg;
	static int counter;

	while (TRUE) {
		if (to_refresh) {
			struct pollfd fd_array;
			int pos, to_write;

			fd_array.fd = sockfd;
			fd_array.events = POLLOUT;

			to_refresh = 0;
			counter = 0;

			for (pos = 0, to_write = 1024;;) {
				int sent;
				int retval = poll(&fd_array, 1, 10);
				// no data or poll() error.
				if (retval <= 0) {
					continue;
				}

				sent = write(sockfd, display1 + pos, to_write);
				//fprintf(stderr, "sent = %d\n", sent);

				if (sent < 0) {
					//fprintf(stderr, "sent = %d\n", sent);
					continue;
				}
				pos += sent;
				if (pos >= 6144) break;
				to_write = (6144 - pos) > 1024 ? 1024 : (6144 - pos);
			}

			for (pos = 0, to_write = 1024;;) {
				int sent;
				int retval = poll(&fd_array, 1, 10);
				// no data or poll() error.
				if (retval <= 0) {
					continue;
				}

				sent = write(sockfd, display2 + pos, to_write);

//				fprintf(stderr, "sent = %d\n", sent);
				if (sent < 0) {
					continue;
				}
				pos += sent;
				if (pos >= 6144) break;
				to_write = (6144 - pos) > 1024 ? 1024 : (6144 - pos);
			}
		} else {
			static struct timespec t = {
				.tv_sec = 0,
				.tv_nsec = 10000000
			};
			nanosleep(&t, NULL);
			++counter;
			if (counter == 20) {
				to_refresh = 1;
			}
		}
	}
}

static void
init_acs(void)
{
	int i;

	for (i = 0; i < 128; i++) {
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

	for (i = 0; i < 256; i++) {
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

	if (argc < 2) {
		printf("Need a host as the arg\n");
		exit(255);
	}

	he = gethostbyname(argv[1]);
	if (!he) {
		perror("gethostbyname");
		exit(255);
	}

	stream = fopen("FONT6.BIN", "rb");
	if (!stream) {
		perror("fopen");
		exit(255);
	}
	fread(font, 1, 2048, stream);
	fclose(stream);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		exit(255);
	}

	printf("Connecting...\n");
	memset(&remoteaddr, 0, sizeof(remoteaddr));
	remoteaddr.sin_family = AF_INET;
	remoteaddr.sin_port = htons(2000);
	memcpy(&(remoteaddr.sin_addr), he->h_addr, he->h_length);
	if(connect(sockfd, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0) {
		perror("connect");
		exit(255);
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	init_acs();
	init_keyboard();

	locale=setlocale(LC_ALL,"C");
	/* create the terminal and have it run bash */
	vterm = vterm_create(85, 24, VTERM_FLAG_VT100);
	vterm_set_colors(vterm, COLOR_WHITE, COLOR_BLACK);
	vterm_wnd_set(vterm, &term_win);

	/* keep reading keypresses from the user and passing them to the terminal;
	* also, redraw the terminal to the window at each iteration */
	ch = '\0';

	pthread_create(&fred, NULL, send_loop, &sockfd);

	while (TRUE) {
		struct pollfd fd_array;
		int count, retval;
		int bytes = vterm_read_pipe(vterm);

		if (bytes > 0) {
			vterm_wnd_update(vterm);
			++to_refresh;
		}
		if (bytes == -1) break;

		fd_array.fd = sockfd;
		fd_array.events = POLLIN;
		// wait 10 millisecond for data on pty file descriptor.
		retval = poll(&fd_array, 1, 10);
		// no data or poll() error.
		if (retval <= 0) {
			continue;
		}

		count = read(sockfd, &key, 1);
		if (count == 1) {
			guint32 ch = keyboard_map[key];
			//printf("%d\n", key);
			vterm_write_pipe(vterm, ch);
		}
	}
	close(sockfd);
	return 0;
}
