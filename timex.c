/* A demonstration of the use of poll to avoid blocking. This allows
 * multiple sockets to be read and accepted, and interleaved with keypresses.
 *
 * Compile with:
 * zcc +zx -vn -O2 -o nonblockserv.bin nonblockserv.c -lndos -llibsocket */

#include <stdio.h>
#include <stdlib.h>
#include <im2.h>
#include <input.h>		/* for in_Inkey() */
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sockpoll.h>

#define KBUFSZ 8

char kbuf[KBUFSZ];	/* Circular keyboard buffer */
int bufoffset;		/* buffer offset */
int readoffset;		/* where we've got to reading the buffer */

uchar in_KeyDebounce = 1;       // no debouncing
uchar in_KeyStartRepeat = 20;   // wait 20/50s before key starts repeating
uchar in_KeyRepeatPeriod = 5;  // repeat every 10/50s
uint in_KbdState;               // reserved


char
getSingleKeypress()
{
	uchar k=0;
	if (readoffset != bufoffset) {
#asm
		di
#endasm
		k = *(kbuf + readoffset);
		readoffset++;
		if (readoffset == KBUFSZ)
			readoffset = 0;

#asm
		ei
#endasm
	}
	return k;
}

/* The ISR handles filling the keyboard buffer, which is a circular
 * buffer. The keyboard handler in the 'main thread' should pick characters
 * off this buffer till it catches up */
M_BEGIN_ISR(isr)
{
	uchar k = in_GetKey();

	if (k) {
		*(kbuf+bufoffset) = k;

		bufoffset++;
		if (bufoffset == KBUFSZ)
			bufoffset=0;
	}
}
M_END_ISR


/* Initialization routine that should be called when the client starts */
void
inputinit()
{
	/* IM2 keyboard polling routine setup - this from the
 	   example in the z88dk wiki documentation */
	#asm
	di
	#endasm
/*
	im2_Init(0xd300);
	memset(0xd300, 0xd4, 257);
	bpoke(0xd4d4, 195);
	wpoke(0xd4d5, isr);*/

	im2_Init(0xfd00);
	memset(0xfd00, 0xfe, 257);
	bpoke(0xfefe, 195);
	wpoke(0xfeff, isr);

	/* initialize the keyboard buffer */
	memset(kbuf, 0, sizeof(kbuf));
	bufoffset = 0;
	readoffset = 0;

	#asm
	ei
	#endasm
}

/* De-initialize IM2 etc. to return to BASIC. */
void
inputexit()
{
	#asm
	di
	im 1
	ei
	#endasm
}

main()
{
	int sockfd, connfd, rc;
	struct sockaddr_in my_addr;
	char *display1 = (char *)16384;
	char *display2 = (char *)24576;
	char *display = display1;
	char key[1], ch = ' ', prev_ch = ' ';
	int pos = 0, to_read = 1024, remaining;
	struct pollfd p;	/* the poll information structure */

#asm
	ld a,62
	out (255),a
#endasm
	/* 0x0C clears the screen in the z88dk default console driver */
	putchar(0x0C);

	/* Create the socket */
	/* The first argument, AF_INET is the family. The Spectranet only
	   supports AF_INET at present. SOCK_STREAM in this context is
           for a TCP connection. */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printk("Could not open the socket - rc=%d\n", sockfd);
		return;
	}

	/* Now set up the sockaddr_in structure. */
	/* Zero it out so that any fields we don't set are set to
	   NULL (the structure also contains the local address to bind to). 
	   We will listne to port 2000. */
	memset(&my_addr, 0, sizeof(my_addr));	/* zero out all fields */
	my_addr.sin_family=AF_INET;
	my_addr.sin_port=htons(2000);		/* Port 2000 */

	if (bind(sockfd, &my_addr, sizeof(my_addr)) < 0) {
		printk("Bind failed.\n");
		sockclose(sockfd);
		return;
	}

	/* The socket should now listen. The Spectranet hardware in
	   its present form doesn't support changing the backlog, but
           the second argument to listen should still be a sensible value */
	if (listen(sockfd, 1) < 0) {
		printk("listen failed.\n");
		sockclose(sockfd);
		return;
	}

	printk("Listening on port 2000.\n");

	/* Now wait for things to happen. Contrast this with the server
	   code example in tutorial 2. Note that instead of calling accept()
	   we start a loop, and use the pollall() function to poll all open
	   sockets. When pollall() finds some data ready, it returns
	   status telling us why the socket was ready, so we can act
	   appropriately */

	connfd = accept(sockfd, NULL, NULL);
	if (connfd == 0) {
		printk("accept failed\n");
		return;
	}

	inputinit();
	while(1) {
		rc = poll_fd(connfd);
		if (rc & POLLIN) {
			rc = recv(connfd, display + pos, to_read, 0);
			if (rc < 0) {
				printk("recv failed!\n");
				sockclose(connfd);
				break;
			}
			pos += rc;
			if (pos >= 6144) {
				if (display == display1) {
					display = display2;
				} else {
					display = display1;
				}
				pos = 0;
				to_read = 1024;
				continue;
			}
			remaining = 6144 - pos;
			to_read = remaining > 1024 ? 1024 : remaining;
		}
		ch = getSingleKeypress();
		if (ch == 0) {
			continue;
		}
		key[0] = ch;

		rc = send(connfd, key, 1, 0);
		if (rc < 0) {
			printk("send failed!\n");
			sockclose(connfd);
			break;
		}
	}

	/* Close the listening socket and exit. */
	sockclose(sockfd);
	printk("Finished.\n");
	inputexit();
}
