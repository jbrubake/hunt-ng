/*	$OpenBSD: hunt.h,v 1.5 2003/06/11 08:45:33 pjanzen Exp $	*/
/*	$NetBSD: hunt.h,v 1.5 1998/09/13 15:27:28 hubertf Exp $	*/

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

/*
 * Preprocessor define dependencies
 */

/* decrement version number for each change in startup protocol */
/* # define	HUNT_VERSION		(-1) */

/* MDB: this is surely a fork rather than the next version, so change the
 * version number completely: */
# define	HUNT_VERSION		(('m' << 8) | 'b')

/* Hell, and the port (FIXME: change back if this is ever going to be released
 * properly! */
/* # define	HUNT_PORT_NAME		("hunt") */
/* # define	HUNT_PORT		(('h' << 8) | 't') */
# define	HUNT_PORT		(('h' << 8) | 'h') /* "Hacked Hunt" */
# define	HUNT_PORT_NAME		("hunt.hacked")

# define	ADDCH		('a' | 0200)
# define	MOVE		('m' | 0200)
# define	REFRESH		('r' | 0200)
# define	CLRTOEOL	('c' | 0200)
# define	ENDWIN		('e' | 0200)
# define	CLEAR		('C' | 0200)
# define	REDRAW		('R' | 0200)
# define	LAST_PLAYER	('l' | 0200)
# define	BELL		('b' | 0200)
# define	READY		('g' | 0200)
# define	ADDCOLORCH	('A' | 0200)
# define	SETCOLOR	('s' | 0200)
# define	CLEARCOLOR	('S' | 0200)

# define	SCREEN_HEIGHT	24
# define	SCREEN_WIDTH	80
# define	HEIGHT	23
# define	WIDTH	51
# define	SCREEN_WIDTH2	128	/* Next power of 2 >= SCREEN_WIDTH */
# define	WIDTH2	64	/* Next power of 2 >= WIDTH (for fast access) */

# define	NAMELEN		20

# define	Q_QUIT		0
# define	Q_CLOAK		1
# define	Q_FLY		2
# define	Q_SCAN		3
# define	Q_MESSAGE	4

# define	C_PLAYER	0
# define	C_MONITOR	1
# define	C_MESSAGE	2
# define	C_SCORES	3
# define	C_TESTMSG()	(Query_driver ? C_MESSAGE :\
				(Show_scores ? C_SCORES :\
				(Am_monitor ? C_MONITOR :\
				C_PLAYER)))

# define	INPUTMODE_PLAY		0
# define	INPUTMODE_COM		1

#define		MAX_MESSAGE_LENGTH	(SCREEN_WIDTH - COMMAND_PROMPT_LENGTH - 1)

#define		MAX_BUFFERED_MESSAGES	10

#define		COMMAND_PROMPT		":"
#define		COMMAND_PROMPT_LENGTH	1

/* Layout of the scoreboard: */
#define STAT_LABEL_COL	60
#define STAT_VALUE_COL	74
#define STAT_NAME_COL	61
#define STAT_SCAN_COL	(STAT_NAME_COL + 5)
#define STAT_AMMO_ROW	0
#define STAT_GUN_ROW	1
#define STAT_DAM_ROW	2
#define STAT_KILL_ROW	3
#define STAT_PLAY_ROW	5
#define STAT_MON_ROW	(STAT_PLAY_ROW + MAXPL + 1)
#define STAT_NAME_LEN	18

typedef int			FLAG;

/* Objects within the maze: */

# define	DOOR	'#'
# define	WALL1	'-'
# define	WALL2	'|'
# define	WALL3	'+'
# define	WALL4	'/'
# define	WALL5	'\\'
# define	KNIFE	'K'
# define	SHOT	':'
# define	GRENADE	'o'
# define	SATCHEL	'O'
# define	BOMB	'@'
# define	MINE	';'
# define	GMINE	'g'
# define	SLIME	'$'
# define	LAVA	'~'
# define	DSHOT	'?'
# define	FALL	'F'
# define	PAIN	'"'
# define	BOOT		'b'
# define	BOOT_PAIR	'B'

# define	SPACE	' '

# define	ABOVE	'i'
# define	BELOW	'!'
# define	RIGHT	'}'
# define	LEFTS	'{'
# define	FLYER	'&'
# define	is_player(c)	(c == LEFTS || c == RIGHT ||\
				c == ABOVE || c == BELOW || c == FLYER)

/* color pairs */
/* For now, background is always black */
# define	COL_RED	    1
# define	COL_GREEN   2
# define	COL_YELLOW  3
# define	COL_BLUE    4
# define	COL_MAGENTA 5
# define	COL_CYAN    6
# define	COL_WHITE   7

/* Some slightly hacky stuff to let us use bold colours like any other colour
 * */
# define	ATTR_BOLD   (1 << 4)
# define	COL_RED_B     (COL_RED | ATTR_BOLD)
# define	COL_GREEN_B   (COL_GREEN | ATTR_BOLD)
# define	COL_YELLOW_B  (COL_YELLOW | ATTR_BOLD)
# define	COL_BLUE_B    (COL_BLUE | ATTR_BOLD)
# define	COL_MAGENTA_B (COL_MAGENTA | ATTR_BOLD)
# define	COL_CYAN_B    (COL_CYAN | ATTR_BOLD)
# define	COL_WHITE_B   (COL_WHITE | ATTR_BOLD)


# define	COL_DEFAULT		0
# define	COL_MESSAGE		COL_WHITE_B
# define	COL_PLAYER		COL_CYAN_B
# define	COL_OTHER_PLAYER	COL_WHITE_B
# define	COL_EXPLOSION		COL_RED_B
# define	COL_SHOT		COL_MAGENTA_B
# define	COL_GRENADE		COL_MAGENTA_B
# define	COL_SATCHEL		COL_WHITE_B
# define	COL_BOMB		COL_WHITE_B
# define	COL_LAVA		COL_RED_B
# define	COL_SLIME_SHOT		COL_GREEN_B
# define	COL_SLIME		COL_GREEN
# define	COL_DOOR		COL_YELLOW
# define	COL_BOOT		COL_YELLOW
# define	COL_MINE		COL_YELLOW_B
# define	COL_FLYER		COL_YELLOW_B


# ifndef TRUE
# define	TRUE	1
# define	FALSE	0
# endif

