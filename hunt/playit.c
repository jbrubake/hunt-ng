/*	$OpenBSD: playit.c,v 1.8 2003/06/11 08:45:25 pjanzen Exp $	*/
/*	$NetBSD: playit.c,v 1.4 1997/10/20 00:37:15 lukem Exp $	*/
/*
 * Copyright (c) 1983-2003, Regents of the University of California.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * + Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 * + Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution.
 * + Neither the name of the University of California, San Francisco nor 
 *   the names of its contributors may be used to endorse or promote 
 *   products derived from this software without specific prior written 
 *   permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../config.h"

#include <sys/file.h>
#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "hunt.h"
#include "display.h"
#include "client.h"

static int	nchar_send;
static FLAG	Last_player;
static int	Otto_expect;

# define	MAX_SEND	5

/*
 * ibuf is the input buffer used for the stream from the driver.
 * It is small because we do not check for user input when there
 * are characters in the input buffer.
 */
static int		icnt = 0;
static unsigned char	ibuf[256], *iptr = ibuf;

#define	GETCHR()	(--icnt < 0 ? getchr() : *iptr++)

static	char		team_of_color(int);
static	unsigned char	getchr(void);
static	void		send_stuff(void);

static int		inputmode = INPUTMODE_PLAY;

static int		com_y;
static int		com_x;


/*
 * playit:
 *	Play a given game, handling all the curses commands from
 *	the driver.
 */
void
playit()
{
	int		ch;
	int		clr = 0;
	int		y, x;
	u_int32_t	version;
	int		otto_y, otto_x;
	char		otto_face = ' ';
	int		chars_processed;

	if (read(Socket, &version, sizeof version) != sizeof version) {
		bad_con();
		/* NOTREACHED */
	}
	if (ntohl(version) != HUNT_VERSION) {
		bad_ver();
		/* NOTREACHED */
	}
	errno = 0;
	nchar_send = MAX_SEND;
	Otto_expect = 0;
	while ((ch = GETCHR()) != EOF) {
		switch (ch & 0377) {
		  case MOVE:
			y = GETCHR();
			x = GETCHR();
			display_move(y, x);
			break;

		  case CLRTOEOL:
			display_clear_eol();
			break;
		  case CLEAR:
			display_clear_the_screen();
			break;
		  case REFRESH:
			display_refresh();
			break;
		  case REDRAW:
			display_redraw_screen();
			display_refresh();
			break;
		  case ENDWIN:
			display_refresh();
			if ((ch = GETCHR()) == LAST_PLAYER)
				Last_player = TRUE;
			ch = EOF;
			goto out;
		  case BELL:
			if (!no_beep)
			    display_beep();
			break;
		  case READY:
			chars_processed = GETCHR();
			display_refresh();
			if (nchar_send < 0)
				tcflush(STDIN_FILENO, TCIFLUSH);
			nchar_send += chars_processed;
			if (nchar_send > MAX_SEND)
				nchar_send = MAX_SEND;
			if (Otto_mode) {
				/*
				 * The driver returns the number of keypresses
				 * that it has processed. Use this to figure
				 * out if otto's commands have completed.
				 */
				Otto_expect -= chars_processed;
				if (Otto_expect == 0) {
					/* not very fair! */
					static char buf[MAX_SEND * 2];
					int len;

					/* Ask otto what it wants to do: */
					len = otto(otto_y, otto_x, otto_face,
						buf, sizeof buf);
					if (len) {
						/* Pass it on to the driver: */
						write(Socket, buf, len);
						/* Update expectations: */
						Otto_expect += len;
					}
				}
			}
			break;
		  case SETCOLOR:
			if (Use_Color)
				display_set_colour(GETCHR());
			break;
		  case CLEARCOLOR:
			if (Use_Color)
				display_clear_colour(GETCHR());
			break;
		  case ADDCOLORCH:
			clr = GETCHR();
			/* FALLTHROUGH */
		  case ADDCH:
			ch = GETCHR();
			/* FALLTHROUGH */
		  default:
			if (!isprint(ch))
				ch = ' ';

			if (clr && Use_Color) {
				display_put_ch_color(ch, clr);
			}
			else {
				if (clr && !Use_Color &&
						(ch == '{' ||
						 ch == '}' ||
						 ch == 'i' ||
						 ch == '!')) {
					/* We aren't using colour, but the
					 * server differentiates between teams
					 * by colour, so we may have to change
					 * the character to a team digit
					 * (losing facing information in the
					 * process). 
					 */
					int nteam = team_of_color(clr);

					/* Monitors see all team members as
					 * digits, and team members see their
					 * team-mates as digits.
					 */
					if (nteam != ' ' &&
							(Am_monitor ||
							 team == nteam))
						ch = nteam;
				}

				/* Output the colorless character */
				display_put_ch(ch);
			}

			if (Otto_mode)
				switch (ch) {
				case '<':
				case '>':
				case '^':
				case 'v':
					otto_face = ch;
					display_getyx(&otto_y, &otto_x);
					otto_x--;
					break;
				}

			clr = 0;

			break;
		}
	}
out:
	(void) close(Socket);
}

/*
 * team_of_color:
 *	Get team character corresponding to a color
 *	XXX: this should be kept in sync with the team_colors array in
 *	huntd/extern.c
 *	(and yes, I know this is all rather crufty - but I can't see a better
 *	way)
 */
static char
team_of_color(int clr)
{
	switch (clr) {
		case COL_RED:		return '1';
		case COL_GREEN:		return '2';
		case COL_MAGENTA:	return '3';
		case COL_CYAN:		return '4';
		case COL_RED_B:		return '5';
		case COL_GREEN_B:	return '6';
		case COL_YELLOW_B:	return '7';
		case COL_BLUE_B:	return '8';
		case COL_MAGENTA_B:	return '9';
		default:		return ' ';
	}
}

/*
 * getchr:
 *	Grab input and pass it along to the driver
 *	Return any characters from the driver
 *	When this routine is called by GETCHR, we already know there are
 *	no characters in the input buffer.
 */
static unsigned char
getchr()
{
	fd_set	readfds, s_readfds;
	int	nfds, s_nfds;

	FD_ZERO(&s_readfds);
	FD_SET(Socket, &s_readfds);
	FD_SET(STDIN_FILENO, &s_readfds);
	s_nfds = (Socket > STDIN_FILENO) ? Socket : STDIN_FILENO;
	s_nfds++;

one_more_time:
	do {
		errno = 0;
		readfds = s_readfds;
		nfds = s_nfds;
		nfds = select(nfds, &readfds, NULL, NULL, NULL);
	} while (nfds <= 0 && errno == EINTR);

	if (FD_ISSET(STDIN_FILENO, &readfds))
		send_stuff();
	if (!FD_ISSET(Socket, &readfds))
		goto one_more_time;
	icnt = read(Socket, ibuf, sizeof ibuf);
	if (icnt <= 0) {
		bad_con();
		/* NOTREACHED */
	}
	iptr = ibuf;
	icnt--;
	return *iptr++;
}

/*
 * send_stuff:
 *	Send standard input characters to the driver
 */
static void
send_stuff()
{
	int		count;
	char		*sp, *nsp;
	static char	inp[BUFSIZ];
	static char	Buf[BUFSIZ];

	/* Drain the user's keystrokes: */
	count = read(STDIN_FILENO, Buf, sizeof Buf);
	if (count < 0)
		err(1, "read");
	if (count == 0)
		return;

	if (nchar_send <= 0) {
	    if (!no_beep)
		display_beep();
	    return;
	}

	/*
	 * look for 'q'uit commands; if we find one,
	 * confirm it.  If it is not confirmed, strip
	 * it out of the input
	 */
	Buf[count] = '\0';
	for (sp = Buf, nsp = inp; *sp != '\0'; sp++, nsp++) {
		if (inputmode == INPUTMODE_PLAY) {
			*nsp = map_key[(int)*sp];
			if (*nsp == 'q') {
				intr(0);
				nsp--;
			}
			if (*nsp == '\n' || *nsp == '\r')
				inputmode = INPUTMODE_COM;
		}
		else if (inputmode == INPUTMODE_COM) {
			/* don't map in com-mode */
			int ret;

			*nsp = *sp;
			/*
			display_move(com_y, com_x);
			ret = process_message_char(*nsp, &com_buf_p,
					com_buf, COM_MAX_LENGTH);
			if (ret == 1)
				inputmode = INPUTMODE_PLAY;
			else display_getyx(&com_y, &com_x);
			*/

			if (*nsp == '\n' || *nsp == '\r')
				inputmode = INPUTMODE_PLAY;

			/* Send term-independent special chars */
			else if (display_iserasechar(*nsp))
				*nsp = '\b';
			else if (display_iskillchar(*nsp))
				*nsp = 21;

			/*
			else if (ret == 2) {
			*/
				/* No room in buffer - ring the bell but don't
				 * send the character */
			/*
				display_beep();
				nsp--;
			}
			*/

		}
	}
	count = nsp - inp;
	if (count) {
		nchar_send -= count;
		if (nchar_send < 0)
		{
			count += nchar_send;
			nchar_send = 0;
		}
		(void) write(Socket, inp, count);
		if (Otto_mode) {
			/*
			 * The user can insert commands over otto.
			 * So, otto shouldn't be alarmed when the 
			 * server processes more than otto asks for.
			 */
			Otto_expect += count;
		}
	}
}

/*
 * quit:
 *	Handle the end of the game when the player dies
 */
int
quit(old_status)
	int	old_status;
{
	int	explain, ch;

	inputmode = INPUTMODE_PLAY;
	if (Last_player)
		return Q_QUIT;
	if (Otto_mode)
		return otto_quit(old_status);
	display_move(HEIGHT, 0);
	display_put_str("Re-enter game [ynwo]? ");
	display_clear_eol();
	explain = FALSE;
	for (;;) {
		display_refresh();
		if (isupper(ch = getchar()))
			ch = tolower(ch);
		if (ch == 'y')
			return old_status;
		else if (ch == 'o')
			break;
		else if (ch == 'n') {
			display_move(HEIGHT, 0);
			display_put_str("Write a parting message [yn]? ");
			display_clear_eol();
			display_refresh();
			for (;;) {
				if (isupper(ch = getchar()))
					ch = tolower(ch);
				if (ch == 'y')
					goto get_message;
				if (ch == 'n')
					return Q_QUIT;
			}
		}
		else if (ch == 'w') {
			static	char	buf[WIDTH + WIDTH % 2];
			char		*cp, c;

get_message:
			c = ch;		/* save how we got here */
			display_move(HEIGHT, 0);
			display_put_str("Message: ");
			display_clear_eol();
			display_refresh();
			cp = buf;
			do { display_refresh(); }
			while (!process_message_char(getchar(), &cp, buf, sizeof buf));
			*cp = '\0';
			Send_message = buf;
			return (c == 'w') ? old_status : Q_MESSAGE;
		}
		display_beep();
		if (!explain) {
			display_put_str("(Yes, No, Write message, or Options) ");
			explain = TRUE;
		}
	}

	display_move(HEIGHT, 0);
	display_put_str("Scan, Cloak, Flying, or Quit? ");
	display_clear_eol();
	display_refresh();
	explain = FALSE;
	for (;;) {
		if (isupper(ch = getchar()))
			ch = tolower(ch);
		if (ch == 's')
			return Q_SCAN;
		else if (ch == 'c')
			return Q_CLOAK;
		else if (ch == 'f')
			return Q_FLY;
		else if (ch == 'q')
			return Q_QUIT;
		display_beep();
		if (!explain) {
			display_put_str("[SCFQ] ");
			explain = TRUE;
		}
		display_refresh();
	}
}

/*
 * do_message:
 *	Send a message to the driver and return
 */
void
do_message(char* message)
{
	u_int32_t	version;

	if (read(Socket, &version, sizeof version) != sizeof version) {
		bad_con();
		/* NOTREACHED */
	}
	if (ntohl(version) != HUNT_VERSION) {
		bad_ver();
		/* NOTREACHED */
	}
	if (write(Socket, message, strlen(message)) < 0) {
		bad_con();
		/* NOTREACHED */
	}
	(void) close(Socket);
}

int
process_message_char(char ch, char **cpref, char *buf, int buflen)
{ 
	/* Cursor must be set to the right pos before calling
	 * Returns: 1 if message complete, 0 else */
	if (ch == '\n' || ch == '\r')
		return 1;
	if (display_iserasechar(ch))
	{
		if (*cpref > buf) {
			int y, x;

			display_getyx(&y, &x);
			display_move(y, x - 1);
			*cpref -= 1;
			display_clear_eol();
		}
		return 0;
	}
	else if (display_iskillchar(ch))
	{
		int y, x;

		display_getyx(&y, &x);
		display_move(y, x - (*cpref - buf));
		*cpref = buf;
		display_clear_eol();
		return 0;
	} else if (!isprint(ch)) {
		display_beep();
		return 0;
	}
	else if (*cpref + 1 >= buf + buflen)
		/* Not room for another character and the \0 */
		return -1;
	else {
		display_put_ch(ch);
		*((*cpref)++) = ch;
		return 0;
	}
}
