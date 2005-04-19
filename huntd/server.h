/*	$OpenBSD: server.h,v 1.9 2004/01/16 00:13:19 espie Exp $	*/
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


#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#ifndef __GNUC__
#define __attribute__(x)
#endif

#ifndef SIGINFO
#define SIGINFO SIGUSR1
#endif

/* FIXME: test if we have it! */
#define HAVE_TCPD

/*
 * Choose MAXPL and MAXMON carefully.  The screen is assumed to be
 * 23 lines high and will only tolerate (MAXPL == 17 && MAXMON == 0)
 * or (MAXPL + MAXMON <= 16).
 */
#define MAXPL		14
#define MAXMON		2
#if (MAXPL + MAXMON > 16)
#warning "MAXPL + MAXMON is excessive"
#endif

#define MSGLEN		SCREEN_WIDTH

#define UBOUND		1
#define DBOUND		(HEIGHT - 1)
#define LBOUND		1
#define RBOUND		(WIDTH - 1)

#define NASCII		128

/* Number of boots: */
#define NBOOTS		2

/* Bitmask of directions */
#define NORTH		01
#define SOUTH		02
#define EAST		010
#define WEST		020

# undef CTRL
#define CTRL(x)	((x) & 037)

#define BULREQ		1		/* 0 */
#define GRENREQ		9		/* 1 */
#define SATREQ		25		/* 2 */
#define BOMB7REQ	49		/* 3 */
#define BOMB9REQ	81		/* 4 */
#define BOMB11REQ	121		/* 5 */
#define BOMB13REQ	169		/* 6 */
#define BOMB15REQ	225		/* 7 */
#define BOMB17REQ	289		/* 8 */
#define BOMB19REQ	361		/* 9 */
#define BOMB21REQ	441		/* 10 */
#define MAXBOMB				   11

#define SLIMEREQ	5		/* 0 */
#define SSLIMEREQ	10		/* 1 */
#define SLIME2REQ	15		/* 2 */
#define SLIME3REQ	20		/* 3 */
#define MAXSLIME			   4

#define EXPLEN		16

#define _scan_char(pp)	(((pp)->p_scan < 0) ? ' ' : '*')
#define _cloak_char(pp)	(((pp)->p_cloak < 0) ? _scan_char(pp) : '+')
#define stat_char(pp)	(((pp)->p_flying < 0) ? _cloak_char(pp) : FLYER)

typedef struct bullet_def	BULLET;
typedef struct expl_def		EXPL;
typedef struct player_def	PLAYER;
typedef struct ident_def	IDENT;
typedef struct regen_def	REGEN;

#define	ALL_PLAYERS		((PLAYER *)1)

#define IS_PLAYER(pp)		(pp >= Player && pp < End_player)
#define IS_MONITOR(pp)		(pp >= Monitor && pp < End_monitor)

/* Abbreviates the common construction
 *	for (ppvar = Player; ppvar < End_player; ppvar++) {
 *		[do something]
 *	}
 *	for (ppvar = Monitor; ppvar < End_monitor; ppvar++) {
 *		[do the same thing]
 *	}
 */
#define FOR_EACH_PLAYER(ppvar) \
	for (ppvar=Player\
			; (IS_PLAYER(ppvar)) || (IS_MONITOR(ppvar))\
			; ++ppvar==End_player ? ppvar=Monitor : 0)

/* Max length of message pp should be allowed to send - need to allow room for
 * e.g. "bob[3]: " */
#define MAX_MESSAGE_LENGTH_FOR(pp) \
	MAX_MESSAGE_LENGTH - strlen(pp->p_ident->i_name) - 5

struct ident_def {
	char	i_name[NAMELEN];
	char	i_team;
	long	i_machine;
	long	i_uid;
	float	i_kills;
	int	i_entries;
	float	i_score;
	int	i_absorbed;
	int	i_faced;
	int	i_shot;
	int	i_robbed;
	int	i_slime;
	int	i_missed;
	int	i_ducked;
	int	i_gkills, i_bkills, i_deaths, i_stillb, i_saved;
	IDENT	*i_next;
};

struct player_def {
	IDENT	*p_ident;
	char	p_over;
	int	p_face;
	int	p_undershot;
	int	p_flying;
	int	p_flyx, p_flyy;
	int	p_nboots;
	FILE	*p_output;
	int	p_fd;
	int	p_mask;
	int	p_damage;
	int	p_damcap;
	int	p_ammo;
	int	p_ncshot;
	int	p_scan;
	int	p_cloak;
	int	p_x, p_y;
	int	p_ncount;
	int	p_nexec;
	long	p_nchar;
	char	p_death[MSGLEN];
	char	p_maze[HEIGHT][WIDTH2];
	int	p_curx, p_cury;
	int	p_lastx, p_lasty;
	char	p_cbuf[BUFSIZ];
	int	p_inputmode;
	char	p_com[MAX_MESSAGE_LENGTH];
	int	p_com_len;
	char	*p_message_buf[MAX_BUFFERED_MESSAGES];
	int	p_message_buf_num;
	FLAG	p_message_displayed;
	struct timeval	p_last_message_time;
	struct timeval	p_last_action_time;
	int	p_just_hurt;
};

struct bullet_def {
	int	b_x, b_y;
	int	b_face;
	int	b_charge;
	char	b_type;
	char	b_size;
	char	b_over;
	PLAYER	*b_owner;
	IDENT	*b_score;
	FLAG	b_expl;
	BULLET	*b_next;
};

struct expl_def {
	int	e_x, e_y;
	char	e_char;
	EXPL	*e_next;
};

struct regen_def {
	int	r_x, r_y;
	REGEN	*r_next;
};

struct spawn {
	int		fd;
	int		reading_msg;
	struct sockaddr source;
	socklen_t	sourcelen;
	u_int32_t	uid;
	char		name[NAMELEN+1];
	u_int8_t	team;
	u_int32_t	enter_status;
	char		ttyname[NAMELEN];
	u_int32_t	mode;
	char		msg[BUFSIZ];
	int		msglen;
	struct spawn *	next;
	struct spawn **	prevnext;
	int		inlen;
	char		inbuf[ sizeof (u_int32_t) + NAMELEN +
			       sizeof (u_int8_t) + sizeof (u_int32_t) +
			       NAMELEN + sizeof (u_int32_t) ];
};

extern struct spawn *	Spawn;

extern int	Socket;

/* answer.c */
void	answer_first(void);
int	answer_next(struct spawn *);
int	rand_dir(void);
void	answer_info(FILE *);

/* draw.c */
void	drawmaze(PLAYER *);
void	look(PLAYER *);
void	check(PLAYER *, int, int);
void	check_for_player(PLAYER *, int, int);
void	draw_monitor_status_line(PLAYER *, int, PLAYER *);
void	draw_player_status_line(PLAYER *, int, PLAYER *);
void	showstat(PLAYER *);
void	drawplayer(PLAYER *, FLAG);
void	send_message(PLAYER *, char *);
int	color_of(char);
int	player_sym(PLAYER *, int, int);
int	player_sym_col(PLAYER *, int, int);

/* driver.c */
int	rand_num(int);
void	checkdam(PLAYER *, PLAYER *, IDENT *, int, char);
void	cleanup(int);

/* execute.c */
void	mon_execute(PLAYER *);
int	execute(PLAYER *);
void	add_shot(int, int, int, char, int, PLAYER *, int, char);
BULLET *create_shot(int, int, int, char, int, int, PLAYER *, IDENT *,
	    int, char);
void	ammo_update(PLAYER *);
void	cool_gun(PLAYER *);

/* expl.c */
void	showexpl(int, int, char);
void	showexpl_color(int, int, char, int);
void	rollexpl(void);
void	makemaze(void);
void	clearwalls(void);
int	can_rollexpl(void);

/* makemaze.c */
void	makemaze(void);

/* message.c */
void	start_command(PLAYER *);
void	process_command(PLAYER *, char *);
void	message(PLAYER *, int, char *);
void	message_formatted(PLAYER *, int, const char *, ...)
			__attribute__((format (printf, 3, 4)));
void	send_next_message(PLAYER *);
int	message_ready_for_next(PLAYER *);
int	messages_pending();

/* shots.c */
int	can_moveshots(void);
void	moveshots(void);
PLAYER *play_at(int, int);
int	opposite(int, char);
BULLET *is_bullet(int, int);
void	fixshots(int, int, char);

/* terminal.c */
void	cgoto(PLAYER *, int, int);
void	outch(PLAYER *, char);
void	outch_color(PLAYER *, char, int);
void	outstr(PLAYER *, char *, int);
void	outstr_color(PLAYER *, char *, int, int);
void	outyx(PLAYER *, int, int, const char *, ...)
			__attribute__((format (printf, 4, 5)));
void	clrscr(PLAYER *);
void	ce(PLAYER *);
void	sendcom(PLAYER *, int, ...);
void	flush(PLAYER *);
void	logit(int, const char *, ...)
			__attribute__((format (printf, 2, 3)));
void	logx(int, const char *, ...)
			__attribute__((format (printf, 2, 3)));

/* extern.c */
extern FLAG	Am_monitor;
extern char	Buf[BUFSIZ];
extern char	Maze[HEIGHT][WIDTH2];
extern char	Orig_maze[HEIGHT][WIDTH2];
extern fd_set	Fds_mask;
extern fd_set	Have_inp;
extern int	Nplayer;
extern int	Num_fds;
extern int	Socket;
extern int	Status;
extern int	See_over[NASCII];
extern BULLET *	Bullets;
extern EXPL *	Expl[EXPLEN];
extern EXPL *	Last_expl;
extern PLAYER	Player[MAXPL];
extern PLAYER *	End_player;
extern PLAYER	Boot[NBOOTS];
extern IDENT *	Scores;
extern PLAYER	Monitor[MAXMON];
extern PLAYER *	End_monitor;
extern int	volcano;
extern int	shot_req[MAXBOMB];
extern int	shot_type[MAXBOMB];
extern int	slime_req[MAXSLIME];
extern int	team_color[9];
extern struct timeval	min_message_time;
extern struct timeval	simstep_time;
extern struct timeval	next_round_time;
extern int	num_steps;
