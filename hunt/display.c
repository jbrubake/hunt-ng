/*	$OpenBSD: display.c,v 1.4 2002/02/19 19:39:36 millert Exp $	*/

/*
 * Display abstraction.
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 */

#include "../config.h"

#include <sys/cdefs.h>
#include "display.h"

/* Currently never defined: */
#ifdef USE_OLD_CURSES

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>   
#define _USE_OLD_CURSES_
#include <curses.h>
#include <err.h>
#include "hunt.h"

static struct termios saved_tty;

char	screen[SCREEN_HEIGHT][SCREEN_WIDTH2];
char	blanks[SCREEN_WIDTH];
int	cur_row, cur_col;

/*
 * tstp:
 *      Handle stop and start signals
 */
static void
tstp(dummy)
        int dummy;
{
        int     y, x;

        y = cur_row;
        x = cur_col;
        mvcur(cur_row, cur_col, HEIGHT, 0);
        cur_row = HEIGHT;
        cur_col = 0;
        _puts(VE);
        _puts(TE);
        (void) fflush(stdout);
        tcsetattr(0, TCSADRAIN, &__orig_termios);
        (void) kill(getpid(), SIGSTOP);
        (void) signal(SIGTSTP, tstp);
        tcsetattr(0, TCSADRAIN, &saved_tty);
        _puts(TI);
        _puts(VS);
        cur_row = y;
        cur_col = x;
        _puts(tgoto(CM, cur_row, cur_col));
        display_redraw_screen();
        (void) fflush(stdout);
}

/*
 * display_open:
 *	open the display
 */
void
display_open()
{
	char *term;

	if (!isatty(0) || (term = getenv("TERM")) == NULL)
		errx(1, "no terminal type");

        gettmode();
        (void) setterm(term);
        (void) noecho();  
        (void) cbreak();
        tcgetattr(0, &saved_tty);
        _puts(TI);
        _puts(VS);
#ifdef SIGTSTP
        (void) signal(SIGTSTP, tstp);
#endif
}

void
display_init_color()
{
	/* No colour without curses */
}

/*
 * display_beep:
 *	beep
 */
void
display_beep()
{
	(void) putchar('\a');
}

/*
 * display_refresh:
 *	sync the display
 */
void
display_refresh()
{
	(void) fflush(stdout);
}

/*
 * display_clear_eol:
 *	clear to end of line, without moving cursor
 */
void
display_clear_eol()
{
        if (CE != NULL)
                tputs(CE, 1, __cputchar);
        else {
                fwrite(blanks, sizeof (char), SCREEN_WIDTH - cur_col, stdout);
                if (COLS != SCREEN_WIDTH)
                        mvcur(cur_row, SCREEN_WIDTH, cur_row, cur_col);
                else if (AM)
                        mvcur(cur_row + 1, 0, cur_row, cur_col);
                else
                        mvcur(cur_row, SCREEN_WIDTH - 1, cur_row, cur_col);
        }
        memcpy(&screen[cur_row][cur_col], blanks, SCREEN_WIDTH - cur_col);
}

/*
 * display_putchar:
 *	put one character on the screen, move the cursor right one,
 *	with wraparound
 */
void
display_put_ch(ch)
	char ch;
{
        if (!isprint(ch)) {
                fprintf(stderr, "r,c,ch: %d,%d,%d", cur_row, cur_col, ch);
                return;
        }
        screen[cur_row][cur_col] = ch;
        putchar(ch);
        if (++cur_col >= COLS) {
                if (!AM || XN) 
                        putchar('\n');
                cur_col = 0;
                if (++cur_row >= LINES)
                        cur_row = LINES;
        }
}

void
display_put_ch_color(c, color)
	char c;
{
	/* No colour without curses */
	display_put_ch(c);
}

void display_set_colour(color)
	int color;
{
	/* No colour without curses */
}

void display_clear_colour(color)
	int color;
{
	/* No colour without curses */
}

/*
 * display_put_str:
 *	put a string of characters on the screen
 */
void
display_put_str(s)
	char *s;
{
	for( ; *s; s++)
		display_put_ch(*s);
}

/*
 * display_clear_the_screen:
 *	clear the screen; move cursor to top left
 */
void
display_clear_the_screen()
{
        int     i;

        if (blanks[0] == '\0')   
                for (i = 0; i < SCREEN_WIDTH; i++)
                        blanks[i] = ' ';
  
        if (CL != NULL) {
                tputs(CL, LINES, __cputchar);
                for (i = 0; i < SCREEN_HEIGHT; i++)
                        memcpy(screen[i], blanks, SCREEN_WIDTH);
        } else {
                for (i = 0; i < SCREEN_HEIGHT; i++) {
                        mvcur(cur_row, cur_col, i, 0);
                        cur_row = i;
                        cur_col = 0;
                        display_clear_eol();
                }
                mvcur(cur_row, cur_col, 0, 0);
        }
        cur_row = cur_col = 0;
}

/*
 * display_move:
 *	move the cursor
 */
void
display_move(y, x)
	int y, x;
{
	mvcur(cur_row, cur_col, y, x);
	cur_row = y;
	cur_col = x;
}

/*
 * display_getyx:
 *	locate the cursor
 */
void
display_getyx(yp, xp)
	int *yp, *xp;
{
	*xp = cur_col;
	*yp = cur_row;
}

/*
 * display_end:
 *	close the display
 */
void
display_end()
{
	tcsetattr(0, TCSADRAIN, &__orig_termios);
	_puts(VE);
	_puts(TE);
}

/*
 * display_atyx:
 *	return a character from the screen
 */
char
display_atyx(y, x)
	int y, x;
{
	return screen[y][x];
}

/*
 * display_redraw_screen:
 *	redraw the screen
 */
void
display_redraw_screen()
{
	int i;

        mvcur(cur_row, cur_col, 0, 0);
        for (i = 0; i < SCREEN_HEIGHT - 1; i++) {
                fwrite(screen[i], sizeof (char), SCREEN_WIDTH, stdout);
                if (COLS > SCREEN_WIDTH || (COLS == SCREEN_WIDTH && !AM))
                        putchar('\n');
        }
        fwrite(screen[SCREEN_HEIGHT - 1], sizeof (char), SCREEN_WIDTH - 1,
                stdout);
        mvcur(SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, cur_row, cur_col);
}

#else /* CURSES */ /* --------------------------------------------------- */

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#ifdef HAVE_CURSES_H
#include <curses.h>
#else
#error
#endif
#endif

#include "hunt.h"

static int
decode_attr(color)
{
	if (color & ATTR_BOLD)
		return COLOR_PAIR(color & ~ATTR_BOLD) | A_BOLD;
	else 
		return COLOR_PAIR(color);
}


void
display_open()
{
        initscr();
	curs_set(0);
        (void) noecho(); 
        (void) cbreak();  
}

void
display_init_color()
{
	start_color();
	init_pair(COL_RED, COLOR_RED, COLOR_BLACK);
	init_pair(COL_GREEN, COLOR_GREEN, COLOR_BLACK);
	init_pair(COL_YELLOW, COLOR_YELLOW, COLOR_BLACK);
	init_pair(COL_BLUE, COLOR_BLUE, COLOR_BLACK);
	init_pair(COL_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(COL_CYAN, COLOR_CYAN, COLOR_BLACK);
	init_pair(COL_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void
display_beep()
{
	beep();
}

void
display_refresh()
{
	refresh();
}

void
display_clear_eol()
{
	clrtoeol();
}

void
display_put_ch(c)
	char c;
{
	addch(c);
}

void
display_put_ch_color(c, color)
	char c;
	int color;
{
	int attr = decode_attr(color);

	attron(attr);
	addch(c);
	attroff(attr);
}

void display_set_colour(color)
	int color;
{
	attron(decode_attr(color));
}

void display_clear_colour(color)
	int color;
{
	attroff(decode_attr(color));
}

void
display_put_str(s)
	char *s;
{
	addstr(s);
}

void
display_clear_the_screen()
{
        clear();
        move(0, 0);
        display_refresh();
}

void
display_move(y, x)
	int y, x;
{
	move(y, x);
}

void
display_getyx(yp, xp)
	int *yp, *xp;
{
	getyx(stdscr, *yp, *xp);
}

void
display_end()
{
	endwin();
}

char
display_atyx(y, x)
	int y, x;
{
	int oy, ox;
	char c;

	display_getyx(&oy, &ox);
	c = mvwinch(stdscr, y, x) & 0x7f;
	display_move(oy, ox);
	return (c);
}

void
display_redraw_screen()
{
	clearok(stdscr, TRUE);
	touchwin(stdscr);
}

int
display_iserasechar(ch)
	char ch;
{
	return ch == erasechar();
}

int
display_iskillchar(ch)
	char ch;
{
	return ch == killchar();
}

#endif
