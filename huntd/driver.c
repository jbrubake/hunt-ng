/*	$OpenBSD: driver.c,v 1.16 2004/01/16 00:13:19 espie Exp $	*/
/*	$NetBSD: driver.c,v 1.5 1997/10/20 00:37:16 lukem Exp $	*/
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

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <paths.h>
#include <fcntl.h>

#ifdef HAVE_LIBWRAP
#include <tcpd.h>
#endif

#include "hunt.h"
#include "conf.h"
#include "server.h"

char	*First_arg;		/* pointer to argv[0] */
uint16_t Server_port;
int	Server_socket;		/* test socket to answer datagrams */
FLAG	should_announce = TRUE;	/* true if listening on standard port */
u_short	sock_port;		/* port # of tcp listen socket */
u_short	stat_port;		/* port # of statistics tcp socket */
struct in_addr Server_addr = { INADDR_ANY };	/* address to bind to */

static	void	clear_scores(void);
static	int	havechar(PLAYER *);
static	int	havechars();
static	void	sleep_till_next_round();
static	void	set_next_round_time();
static	void	init(void);
	int	main(int, char *[]);
static	void	init_random_seed(void);
static	void	makeboots(void);
static	void	send_stats(void);
static	void	zap(PLAYER *, FLAG);
static	void	stepsim();
static  void	announce_game(void);
static	void	siginfo(int);
static	void	print_stats(FILE *);
static	void	handle_wkport(int);

/*
 * main:
 *	The main program.
 */
int
main(ac, av)
	int	ac;
	char	**av;
{
	PLAYER		*pp;
	int		process_more;
	static fd_set	read_fds;
	static FLAG	first = TRUE;
	static FLAG	server = FALSE;
	extern int	optind;
	extern char	*optarg;
	int		c;
	static struct timeval	linger = { 0, 0 };
	static struct timeval	timeout = { 0, 0 }, *to;
	struct spawn	*sp, *spnext;
	int		ret;
	int		nready;
	int		fd;

	First_arg = av[0];

	config();

	while ((c = getopt(ac, av, "sp:a:D:")) != -1) {
		switch (c) {
		  case 's':
			server = TRUE;
			break;
		  case 'p':
			should_announce = FALSE;
			Server_port = atoi(optarg);
			break;
		  case 'a':
			if (!inet_aton(optarg, &Server_addr))
				err(1, "bad interface address: %s", optarg);
			break;
		  case 'D':
			config_arg(optarg);
			break;
		  default:
erred:
			fprintf(stderr, "Usage: %s [-s] [-p port] [-a addr]\n",
			    av[0]);
			exit(2);
		}
	}
	if (optind < ac)
		goto erred;

	/* Open syslog: */
	openlog("huntd", LOG_PID | (conf_logerr && !server? LOG_PERROR : 0),
		LOG_DAEMON);

	/* Initialise game parameters: */
	init();

again:
	do {
		/* A note on modes of operation for this main loop:
		 *  There are three modes, selected by conf variables:
		 *	1. rounds==0, simstep=0:
		 *	    The classic mode. All socket reading is blocking,
		 *	    which means that the simulation proceeds at a rate
		 *	    determined by the activity of the players. If
		 *	    no-one types anything, nothing happens - bullets
		 *	    hang motionless in the air, lava freezes in the
		 *	    corridors. Conversely, you can speed up time by
		 *	    mashing the keyboard - a great way to make your
		 *	    bullet leap undodgebly at your mark, which is
		 *	    sadly clearly cheating. This mode does have the
		 *	    efficiency advantage that the server sleeps when
		 *	    nothing's happening in the game.
		 *	2. rounds==0, simstep>0:
		 *	    Similar to 1, but the simulation ticks at least
		 *	    once every simstep microseconds. The
		 *	    keyboard-mashing exploit still works. This mode appears to
		 *	    have been added sometime in the transition from
		 *	    NetBSD to OpenBSD.
		 *	3. rounds==1, simstep>0:
		 *	    Similar to 2, but the simulation now ticks
		 *	    *precisely* once every simstep microseconds. This
		 *	    makes for a much fairer game in my opinion, though
		 *	    I'm not sure how much of an efficiency hit the
		 *	    frequent calls to gettimeofday(2) and its ilk are.
		 *	    Added by me (MDB).
		 *  Note that the logical fourth mode (rounds==1, simstep==0)
		 *  is just a less efficient version of 1.
		 */

		/* First, poll to see if we can get input */
		do {
			read_fds = Fds_mask;
			errno = 0;
			timerclear(&timeout);
			nready = select(Num_fds, &read_fds, NULL, NULL, 
			    &timeout);
			if (nready < 0 && errno != EINTR) {
				logit(LOG_ERR, "select");
				cleanup(1);
			}
		} while (nready < 0);

		if (nready == 0) {
			/*
			 * Nothing was ready. We do some work now
			 * to see if the simulation has any pending work
			 * to do, and decide if we need to block 
			 * indefinitely or just timeout.
			 */
			if (conf_rounds && havechars()) {
				/* Already got some chars to handle - no need
				 * to block yet */
			}
			else
				do {
					if (conf_simstep && 
							(conf_cool_time ||
							 can_moveshots() ||
							 messages_pending()))
					{
						/*
						 * block for a short time before continuing
						 * with explosions, bullets and whatnot
						 */
						to = &timeout;
						to->tv_sec =  simstep_time.tv_sec;
						to->tv_usec = simstep_time.tv_usec;
					} else
						/*
						 * since there's nothing going on,
						 * just block waiting for external activity
						 */
						to = NULL;

					read_fds = Fds_mask;
					errno = 0;
					nready = select(Num_fds, &read_fds, NULL, NULL, 
							to);
					if (nready < 0 && errno != EINTR) {
						logit(LOG_ERR, "select");
						cleanup(1);
					}
				} while (nready < 0);
		}

		/* Remember which descriptors are active: */
		Have_inp = read_fds;

		/* Answer new player connections: */
		if (FD_ISSET(Socket, &Have_inp))
			answer_first();

		/* Continue answering new player connections: */
		for (sp = Spawn; sp; ) {
			spnext = sp->next;
			fd = sp->fd;
			if (FD_ISSET(fd, &Have_inp) && answer_next(sp)) {
				/*
				 * Remove from the spawn list. (fd remains in 
				 * read set).
				 */
				*sp->prevnext = sp->next;
				if (sp->next)
					sp->next->prevnext = sp->prevnext;
				free(sp);

				/* We probably consumed all data. */
				FD_CLR(fd, &Have_inp);

				/* Announce game if this is the first spawn. */
				if (first && should_announce)
					announce_game();
				first = FALSE;
			} 
			sp = spnext;
		}

		/* Process input and move bullets until we've exhausted input
		 * (or one action each if (conf_rounds)) */
		do {
			process_more = FALSE;
			if (conf_rounds) {
				sleep_till_next_round();
				set_next_round_time();
			}
			if (can_moveshots())
			    moveshots();

			num_steps++;
			if (conf_cool_time && (num_steps % conf_cool_time == 0))
				FOR_EACH_PLAYER(pp)
					cool_gun(pp);

			for (pp = Player; pp < End_player; )
				if (pp->p_death[0] != '\0')
					zap(pp, TRUE);
				else
					pp++;
			for (pp = Monitor; pp < End_monitor; )
				if (pp->p_death[0] != '\0')
					zap(pp, FALSE);
				else
					pp++;

			FOR_EACH_PLAYER(pp) {
				if (havechar(pp)) {
					if (IS_PLAYER(pp)) {
						/* act on characters until one causes
						 * an actual action: */
						while (!execute(pp) && havechar(pp))
							pp->p_nexec++;
						pp->p_nexec++;
					}
					else {
						mon_execute(pp);
						pp->p_nexec++;
					}
					if (!conf_rounds)
						process_more = TRUE;
				}
				if (message_ready_for_next(pp))
					send_next_message(pp);
			}
		} while (process_more);

		/* Handle a datagram sent to the server socket: */
		if (FD_ISSET(Server_socket, &Have_inp))
			handle_wkport(Server_socket);

		/* Answer statistics connections: */
		if (FD_ISSET(Status, &Have_inp))
			send_stats();

		/* Flush/synchronize all the displays: */
		FOR_EACH_PLAYER(pp) {
			/*if (FD_ISSET(pp->p_fd, &read_fds)) {*/
			if (pp->p_nexec) {
				sendcom(pp, READY, pp->p_nexec);
				pp->p_nexec = 0;
			}
			if (pp->p_output_unflushed) {
			    flush(pp);
			    pp->p_output_unflushed = 0;
			}
		}
	} while (Nplayer > 0);

	/* No more players! */

	/* No players yet or a continuous game? */
	if (first || conf_linger < 0)
		goto again;

	/* Wait a short while for one to come back: */
	read_fds = Fds_mask;
	linger.tv_sec = conf_linger;
	while ((ret = select(Num_fds, &read_fds, NULL, NULL, &linger)) < 0) {
		if (errno != EINTR) {
			logit(LOG_WARNING, "select");
			break;
		}
		read_fds = Fds_mask;
		linger.tv_sec = conf_linger;
		linger.tv_usec = 0;
	}
	if (ret > 0)
		/* Someone returned! Resume the game: */
		goto again;
	/* else, it timed out, and the game is really over. */

	/* If we are an inetd server, we should re-init the map and restart: */
	if (server) {
		clear_scores();
		makemaze();
		clearwalls();
		makeboots();
		first = TRUE;
		goto again;
	}

	/* Get rid of any attached monitors: */
	for (pp = Monitor; pp < End_monitor; )
		zap(pp, FALSE);

	/* Fin: */
	cleanup(0);
	exit(0);
}

/*
 * sleep_till_next_round:
 *	Sleep if necessary until simstep_time has passed since last round
 */
static void
sleep_till_next_round()
{
	struct timeval now, till;

	gettimeofday(&now, NULL);
	timersub(&next_round_time, &now, &till);
	/*logit(LOG_INFO, "sleep_till_next_round: till = %d.%6d", till.tv_sec, till.tv_usec);*/
	if (till.tv_sec >= 0)
		select(0, NULL, NULL, NULL, &till);
}

/*
 * set_next_round_time:
 *	Set time next round mustn't start before
 */
static void
set_next_round_time()
{
	struct timeval now;

	gettimeofday(&now, NULL);
	/* next_round_time = now+simstep_time: */
	timeradd(&now, &simstep_time, &next_round_time);
}

/*
 * init:
 *	Initialize the global parameters.
 */
static void
init()
{
	int	i;
	struct sockaddr_in	test_port;
	int	true = 1;
	socklen_t	len;
	struct sockaddr_in	addr;
	struct sigaction	sact;
	struct servent *se;

	(void) setsid();
	/* MDB: AFAICT, the following is superfluous once we've done a setsid,
	 * and more to the point appears to give an error when we are already
	 * the process leader (if we run from xinetd or similar). I'm not
	 * entirely clear on what's going on here, but I'm commenting this out
	 * for now.
	if (setpgid(getpid(), getpid()) == -1)
		err(1, "setpgid");
	*/

	sact.sa_flags = SA_RESTART;
	sigemptyset(&sact.sa_mask);

	/* Ignore HUP, QUIT and PIPE: */
	sact.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &sact, NULL) == -1)
		err(1, "sigaction SIGHUP");
	if (sigaction(SIGQUIT, &sact, NULL) == -1)
		err(1, "sigaction SIGQUIT");
	if (sigaction(SIGPIPE, &sact, NULL) == -1)
		err(1, "sigaction SIGPIPE");

	/* Clean up gracefully on INT and TERM: */
	sact.sa_handler = cleanup;
	if (sigaction(SIGINT, &sact, NULL) == -1)
		err(1, "sigaction SIGINT");
	if (sigaction(SIGTERM, &sact, NULL) == -1)
		err(1, "sigaction SIGTERM");

	/* Handle INFO: */
	sact.sa_handler = siginfo;
	if (sigaction(SIGINFO, &sact, NULL) == -1)
		err(1, "sigaction SIGINFO");

	if (chdir("/") == -1)
		warn("chdir");
	(void) umask(0777);

	/* Initialize statistics socket: */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = Server_addr.s_addr;
	addr.sin_port = 0;

	Status = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(Status, (struct sockaddr *) &addr, sizeof addr) < 0) {
		logit(LOG_ERR, "bind");
		cleanup(1);
	}
	if (listen(Status, 5) == -1) {
		logit(LOG_ERR, "listen");
		cleanup(1);
	}

	len = sizeof (struct sockaddr_in);
	if (getsockname(Status, (struct sockaddr *) &addr, &len) < 0)  {
		logit(LOG_ERR, "getsockname");
		cleanup(1);
	}
	stat_port = ntohs(addr.sin_port);

	/* Initialize main socket: */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = Server_addr.s_addr;
	addr.sin_port = 0;

	Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (bind(Socket, (struct sockaddr *) &addr, sizeof addr) < 0) {
		logit(LOG_ERR, "bind");
		cleanup(1);
	}
	if (listen(Socket, 5) == -1) {
		logit(LOG_ERR, "listen");
		cleanup(1);
	}

	len = sizeof (struct sockaddr_in);
	if (getsockname(Socket, (struct sockaddr *) &addr, &len) < 0)  {
		logit(LOG_ERR, "getsockname");
		cleanup(1);
	}
	sock_port = ntohs(addr.sin_port);

	/* Initialize minimal select mask */
	FD_ZERO(&Fds_mask);
	FD_SET(Socket, &Fds_mask);
	FD_SET(Status, &Fds_mask);
	Num_fds = ((Socket > Status) ? Socket : Status) + 1;

	/* Find the port that huntd should run on */
	if (Server_port == 0) {
		se = getservbyname(HUNT_PORT_NAME, "udp");
		if (se != NULL)
			Server_port = ntohs(se->s_port);
		else
			Server_port = HUNT_PORT;
	}

	/* Check if stdin is a socket: */
	len = sizeof (struct sockaddr_in);
	if (getsockname(STDIN_FILENO, (struct sockaddr *) &test_port, &len) >= 0
	    && test_port.sin_family == AF_INET) {
		/* We are probably running from inetd:  don't log to stderr */
		Server_socket = STDIN_FILENO;
		conf_logerr = 0;
		if (test_port.sin_port != htons((u_short) Server_port)) {
			/* Private game */
			should_announce = FALSE;
			Server_port = ntohs(test_port.sin_port);
		}
	} else {
		/* We need to listen on a socket: */
		test_port = addr;
		test_port.sin_port = htons((u_short) Server_port);

		Server_socket = socket(AF_INET, SOCK_DGRAM, 0);

#ifdef SO_REUSEPORT
		/* Permit multiple huntd's on the same port. */
		if (setsockopt(Server_socket, SOL_SOCKET, SO_REUSEPORT, &true, 
		    sizeof true) < 0)
			logit(LOG_ERR, "setsockopt SO_REUSEADDR");
#endif

		if (bind(Server_socket, (struct sockaddr *) &test_port,
		    sizeof test_port) < 0) {
			logit(LOG_ERR, "bind port %d", Server_port);
			cleanup(1);
		}

		/* Datagram sockets do not need a listen() call. */
	}

	/* We'll handle the broadcast listener in the main loop: */
	FD_SET(Server_socket, &Fds_mask);
	if (Server_socket + 1 > Num_fds)
		Num_fds = Server_socket + 1;

	/* Initialise the random seed: */
	init_random_seed();

	/* Dig the maze: */
	makemaze();

	/* Create some boots, if needed: */
	makeboots();

	/* Construct a table of what objects a player can see over: */
	for (i = 0; i < NASCII; i++)
		See_over[i] = TRUE;
	See_over[DOOR] = FALSE;
	See_over[WALL1] = FALSE;
	See_over[WALL2] = FALSE;
	See_over[WALL3] = FALSE;
	See_over[WALL4] = FALSE;
	See_over[WALL5] = FALSE;

	/* make timevals from integers given in conf files */
	min_message_time.tv_sec = conf_message_time / 1000000;
	min_message_time.tv_usec = conf_message_time % 1000000;
	simstep_time.tv_sec = conf_simstep / 1000000;
	simstep_time.tv_usec = conf_simstep % 1000000;

	next_round_time.tv_sec = next_round_time.tv_usec = 0;

	logx(LOG_INFO, "game started");
}

/*
 * init_random_seed:
 *	Initialise the random() seed with current time
 */
static void
init_random_seed()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	srandom(t.tv_usec);
}

/*
 * makeboots:
 *	Put the boots in the maze
 */
static void
makeboots()
{
	int	x, y;
	PLAYER	*pp;

	if (conf_boots) {
		do {
			x = rand_num(WIDTH - 1) + 1;
			y = rand_num(HEIGHT - 1) + 1;
		} while (Maze[y][x] != SPACE);
		Maze[y][x] = BOOT_PAIR;
	}

	for (pp = Boot; pp < &Boot[NBOOTS]; pp++)
		pp->p_flying = -1;
}


/*
 * checkdam:
 *	Apply damage to the victim from an attacker.
 *	If the victim dies as a result, give points to 'credit',
 */
void
checkdam(victim, attacker, credit, damage, shot_type)
	PLAYER	*victim, *attacker;
	IDENT	*credit;
	int	damage;
	char	shot_type;
{
	char	*cp;
	int	y;

	/* Don't do anything if the victim is already in the throes of death */
	if (victim->p_death[0] != '\0')
		return;

	/* Weaken slime attacks by 0.5 * number of boots the victim has on: */
	if (shot_type == SLIME)
		switch (victim->p_nboots) {
		  default:
			break;
		  case 1:
			damage = (damage + 1) / 2;
			break;
		  case 2:
			if (attacker != NULL) {
				if (attacker == victim)
					message(attacker, FALSE, "You have boots on!");
				else
					message(attacker, FALSE, "He has boots on!");
			}
			return;
		}

	/* The victim sustains some damage: */
	victim->p_damage += damage;

	/* Check if the victim survives the hit: */
	if (victim->p_damage <= victim->p_damcap) {
		/* They survive. */

		/* Update damage counter */
		outyx(victim, STAT_DAM_ROW, STAT_VALUE_COL, "%2d",
			victim->p_damage);

		if (conf_show_pain) {
			/* Victim grimaces */
			/* FIXME: Doesn't show for stabbing, for some 
			 *  reason */
			Maze[victim->p_y][victim->p_x] = PAIN;
			victim->p_just_hurt = 1;
			sendcom(ALL_PLAYERS, REFRESH);
		}
		if (conf_pain_message) {
			switch (shot_type) {
				/* TODO: Better messages... */
				default:
					cp = "Ow!";
					break;
				case FALL:
					cp = "Oomph";
					break;
				case KNIFE:
					cp = "You've been stabbed!";
					break;
				case SHOT:
					cp = "You've been shot!";
					break;
				case GRENADE:
					cp = "Boom";
					break;
				case SATCHEL:
				case BOMB:
					cp = "BOOM!";
					break;
				case MINE:
					cp = "You stepped on a mine!";
					break;
				case GMINE:
					cp = "You just exploded a really big mine!";
					break;
				case SLIME:
					cp = "You got slimed!";
					break;
				case LAVA:
					cp = "That burns!";
					break;
				case DSHOT:
					cp = "EX-TER-MIN-ATE!";
					break;
			}
			message(victim, TRUE, cp);
		}
		return;
	}

	/* Didn't survive */

	/* Describe how the victim died: */
	switch (shot_type) {
	  default:
		cp = "Killed";
		break;
	  case FALL:
		cp = "Killed on impact";
		break;
	  case KNIFE:
		cp = "Stabbed to death";
		victim->p_ammo = 0;		/* No exploding */
		break;
	  case SHOT:
		cp = "Shot to death";
		break;
	  case GRENADE:
	  case SATCHEL:
	  case BOMB:
		cp = "Bombed";
		break;
	  case MINE:
	  case GMINE:
		cp = "Blown apart";
		break;
	  case SLIME:
		cp = "Slimed";
		if (credit != NULL)
			credit->i_slime++;
		break;
	  case LAVA:
		cp = "Baked";
		break;
	  case DSHOT:
		cp = "Eliminated";
		break;
	}

	if (credit == NULL) {
		char *blame;

		/*
		 * Nobody is taking the credit for the kill.
		 * Attribute it to either a mine or 'act of God'.
		 */
		switch (shot_type) {
		case MINE:
		case GMINE:
			blame = "a mine";
			break;
		default:
			blame = "act of God";
			break;
		}

		/* Set the death message: */
		(void) snprintf(victim->p_death, sizeof victim->p_death, 
			"| %s by %s |", cp, blame);

		/* No further score crediting needed. */
		return;
	}

	/* Set the death message: */
	(void) snprintf(victim->p_death, sizeof victim->p_death, 
		"| %s by %s |", cp, credit->i_name);

	if (victim == attacker) {
		/* No use killing yourself. */
		credit->i_kills--;
		credit->i_bkills++;
	} 
	else if (victim->p_ident->i_team == ' '
	    || victim->p_ident->i_team != credit->i_team) {
		/* A cross-team kill: */
		credit->i_kills++;
		credit->i_gkills++;
	}
	else {
		/* They killed someone on the same team: */
		credit->i_kills--;
		credit->i_bkills++;
	}

	/* Compute the new credited score: */
	credit->i_score = credit->i_kills / (double) credit->i_entries;

	/* The victim accrues one death: */
	victim->p_ident->i_deaths++;

	/* Account for 'Stillborn' deaths */
	if (victim->p_nchar == 0)
		victim->p_ident->i_stillb++;

	if (attacker) {
		/* Give the attacker player a bit more strength */
		attacker->p_damcap += conf_killgain;
		attacker->p_damage -= conf_killgain;
		if (attacker->p_damage < 0)
			attacker->p_damage = 0;

		/* Tell the attacker he killed the victim */
		message_formatted(attacker, FALSE, "You killed %s!",
			victim->p_ident->i_name);

		/* Tell the attacker his new strength: */
		outyx(attacker, STAT_DAM_ROW, STAT_VALUE_COL, "%2d/%2d", 
			attacker->p_damage, attacker->p_damcap);

		/* Tell the attacker his new 'kill count': */
		outyx(attacker, STAT_KILL_ROW, STAT_VALUE_COL, "%3d",
			(attacker->p_damcap - conf_maxdam) / 2);

		/* Update the attacker's score for everyone else */
		y = STAT_PLAY_ROW + 1 + (attacker - Player);
		outyx(ALL_PLAYERS, y, STAT_NAME_COL,
			"%5.2f", attacker->p_ident->i_score);
	}
}

/*
 * zap:
 *	Kill off a player and take them out of the game.
 *	The 'was_player' flag indicates that the player was not
 *	a monitor and needs extra cleaning up.
 */
static void
zap(pp, was_player)
	PLAYER	*pp;
	FLAG	was_player;
{
	int	len;
	BULLET	*bp;
	PLAYER	*np;
	int	x, y;
	int	savefd;

	if (was_player) {
		/* If they died from a shot, clean up shrapnel */
		if (pp->p_undershot)
			fixshots(pp->p_y, pp->p_x, pp->p_over);
		/* Let the player see their last position: */
		drawplayer(pp, FALSE);
		/* Remove from game: */
		Nplayer--;
	}

	/* Display the cause of death in the centre of the screen: */
	if (conf_color) sendcom(pp, SETCOLOR, COL_MESSAGE);

	len = strlen(pp->p_death);
	x = (WIDTH - len) / 2;
	outyx(pp, HEIGHT / 2, x, "%s", pp->p_death);

	/* Put some horizontal lines around and below the death message: */
	memset(pp->p_death + 1, '-', len - 2);
	pp->p_death[0] = '+';
	pp->p_death[len - 1] = '+';
	outyx(pp, HEIGHT / 2 - 1, x, "%s", pp->p_death);
	outyx(pp, HEIGHT / 2 + 1, x, "%s", pp->p_death);

	if (conf_color) sendcom(pp, CLEARCOLOR, COL_MESSAGE);

	/* Move to bottom left */
	cgoto(pp, HEIGHT, 0);

	savefd = pp->p_fd;

	if (was_player) {
		int	expl_charge;
		int	expl_type;
		int	ammo_exploding;

		/* Check all the bullets: */
		for (bp = Bullets; bp != NULL; bp = bp->b_next) {
			if (bp->b_owner == pp)
				/* Zapped players can't own bullets: */
				bp->b_owner = NULL;
			if (bp->b_x == pp->p_x && bp->b_y == pp->p_y)
				/* Bullets over the player are now over air: */
				bp->b_over = SPACE;
		}

		/* Explode a random fraction of the player's ammo: */
		ammo_exploding = rand_num(pp->p_ammo);

		/* Determine the type and amount of detonation: */
		expl_charge = rand_num(ammo_exploding + 1);
		if (pp->p_ammo == 0)
			/* Ignore the no-ammo case: */
			expl_charge = 0;
		else if (ammo_exploding >= pp->p_ammo - 1) {
			/* Maximal explosions always appear as slime: */
			expl_charge = pp->p_ammo;
			expl_type = SLIME;
		} else {
			/*
			 * Figure out the best effective explosion
			 * type to use, given the amount of charge
			 */
			int btype, stype;
			for (btype = MAXBOMB - 1; btype > 0; btype--)
				if (expl_charge >= shot_req[btype])
					break;
			for (stype = MAXSLIME - 1; stype > 0; stype--)
				if (expl_charge >= slime_req[stype])
					break;
			/* Pick the larger of the bomb or slime: */
			if (btype >= 0 && stype >= 0) {
				if (shot_req[btype] > slime_req[btype])
					btype = -1;
			}
			if (btype >= 0)  {
				expl_type = shot_type[btype];
				expl_charge = shot_req[btype];
			} else
				expl_type = SLIME;
		}

		if (expl_charge > 0) {
			char buf[BUFSIZ];

			/* Detonate: */
			(void) add_shot(expl_type, pp->p_y, pp->p_x, 
			    pp->p_face, expl_charge, (PLAYER *) NULL, 
			    TRUE, SPACE);

			/* Explain what the explosion is about. */
			snprintf(buf, sizeof buf, "%s detonated.", 
				pp->p_ident->i_name);
			message(ALL_PLAYERS, FALSE, buf);

			while (pp->p_nboots-- > 0) {
				/* Throw one of the boots away: */
				for (np = Boot; np < &Boot[NBOOTS]; np++)
					if (np->p_flying < 0)
						break;
#ifdef DIAGNOSTIC
				if (np >= &Boot[NBOOTS])
					err(1, "Too many boots");
#endif
				/* Start the boots from where the player is */
				np->p_undershot = FALSE;
				np->p_x = pp->p_x;
				np->p_y = pp->p_y;
				/* Throw for up to 20 steps */
				np->p_flying = rand_num(20);
				np->p_flyx = 2 * rand_num(6) - 5;
				np->p_flyy = 2 * rand_num(6) - 5;
				np->p_over = SPACE;
				np->p_face = BOOT;
				showexpl_color(np->p_y, np->p_x, BOOT, COL_BOOT);
			}
		}
		/* No explosion. Leave the player's boots behind. */
		else if (pp->p_nboots > 0) {
			if (pp->p_nboots == 2)
				Maze[pp->p_y][pp->p_x] = BOOT_PAIR;
			else
				Maze[pp->p_y][pp->p_x] = BOOT;
			if (pp->p_undershot)
				fixshots(pp->p_y, pp->p_x,
					Maze[pp->p_y][pp->p_x]);
		}

		/* Any unexploded ammo builds up in the volcano: */
		volcano += pp->p_ammo - expl_charge;

		/* Volcano eruption: */
		if (conf_volcano && rand_num(100) < volcano / 
		    conf_volcano_max) {
			/* Erupt near the middle of the map */
			do {
				x = rand_num(WIDTH / 2) + WIDTH / 4;
				y = rand_num(HEIGHT / 2) + HEIGHT / 4;
			} while (Maze[y][x] != SPACE);

			/* Convert volcano charge into lava: */
			(void) add_shot(LAVA, y, x, LEFTS, volcano,
				(PLAYER *) NULL, TRUE, SPACE);
			volcano = 0;

			/* Tell eveyone what's happening */
			message(ALL_PLAYERS, TRUE, "Volcano eruption.");
		}

		/* Drone: */
		if (conf_drone && rand_num(100) < 2) {
			/* Find a starting place near the middle of the map: */
			do {
				x = rand_num(WIDTH / 2) + WIDTH / 4;
				y = rand_num(HEIGHT / 2) + HEIGHT / 4;
			} while (Maze[y][x] != SPACE);

			/* Start the drone going: */
			add_shot(DSHOT, y, x, rand_dir(),
				shot_req[conf_mindshot +
				rand_num(MAXBOMB - conf_mindshot)],
				(PLAYER *) NULL, FALSE, SPACE);
		}

		/* Free any leftover messages */
		while (pp->p_message_buf_num) {
#ifdef MESSAGE_DEBUG
		    logit(LOG_INFO, "X %s - %d %x %s", pp->p_ident->i_name,
			pp->p_message_buf_num-1,
			pp->p_message_buf[pp->p_message_buf_num-1],
			pp->p_message_buf[pp->p_message_buf_num-1]);
#endif
			free(pp->p_message_buf[--(pp->p_message_buf_num)]);
		}

		/* Tell the zapped player's client to shut down. */
		sendcom(pp, ENDWIN, ' ');
		(void) fclose(pp->p_output);

		/* Close up the gap in the Player array: */
		End_player--;
		if (pp != End_player) {
			/* Move the last player into the gap: */
			memcpy(pp, End_player, sizeof *pp);
			draw_player_status_line(ALL_PLAYERS,
				STAT_PLAY_ROW + 1 + (pp - Player), 
				pp);
		}

		/* Erase the last player from the display: */
		cgoto(ALL_PLAYERS, STAT_PLAY_ROW + 1 + Nplayer, STAT_NAME_COL);
		ce(ALL_PLAYERS);
	}
	else {
		/* Zap a monitor */

		/* Free any leftover messages */
		while (pp->p_message_buf_num)
			free(pp->p_message_buf[--(pp->p_message_buf_num)]);

		/* Close the session: */
		sendcom(pp, ENDWIN, LAST_PLAYER);
		(void) fclose(pp->p_output);

		/* shuffle the monitor table */
		End_monitor--;
		if (pp != End_monitor) {
			memcpy(pp, End_monitor, sizeof *pp);
			draw_monitor_status_line(ALL_PLAYERS, 
				STAT_MON_ROW + 1 + (pp - Player),
				pp);
		}

		/* Erase the last monitor in the list */
		cgoto(ALL_PLAYERS,
			STAT_MON_ROW + 1 + (End_monitor - Monitor),
			STAT_NAME_COL);
		ce(ALL_PLAYERS);
	}

	/* Update the file descriptor sets used by select: */
	FD_CLR(savefd, &Fds_mask);
	if (Num_fds == savefd + 1) {
		Num_fds = Socket;
		if (Server_socket > Socket)
			Num_fds = Server_socket;
		FOR_EACH_PLAYER(np)
			if (np->p_fd > Num_fds)
				Num_fds = np->p_fd;
		Num_fds++;
	}
}

/*
 * rand_num:
 *	Return a random number in a given range.
 */
int
rand_num(range)
	int	range;
{
	if (range == 0)
		return 0;
	return (random() % range);
}

/*
 * havechar:
 *	Check to see if we have any characters in the input queue; if
 *	we do, read them, stash them away, and return TRUE; else return
 *	FALSE.
 */
static int
havechar(pp)
	PLAYER	*pp;
{
	int ret;

	/* Do we already have characters? */
	if (pp->p_ncount < pp->p_nchar)
		return TRUE;
	/* Ignore if nothing to read. */
	if (!FD_ISSET(pp->p_fd, &Have_inp))
		return FALSE;
	/* Remove the player from the read set until we have drained them: */
	FD_CLR(pp->p_fd, &Have_inp);

	/* Suck their keypresses into a buffer: */
check_again:
	errno = 0;
	ret = read(pp->p_fd, pp->p_cbuf, sizeof pp->p_cbuf);
	if (ret == -1) {
		if (errno == EINTR)
			goto check_again;
		if (errno == EAGAIN) {
#ifdef DEBUG
			warn("Have_inp is wrong for %d", pp->p_fd);
#endif
			return FALSE;
		}
		logit(LOG_INFO, "read");
	}
	if (ret > 0) {
		/* Got some data */
		pp->p_nchar = ret;
	} else {
		/* Connection was lost/closed: */
		pp->p_inputmode = INPUTMODE_PLAY;
		pp->p_cbuf[0] = 'q';
		pp->p_nchar = 1;
	}
	/* Reset pointer into read buffer */
	pp->p_ncount = 0;
	return TRUE;
}

/*
 * havechars:
 *	Check if any player has unprocessed characters in the input buffer
 */
static int
havechars()
{
	PLAYER	*pp;

	FOR_EACH_PLAYER(pp)
		if (pp->p_ncount < pp->p_nchar)
			return TRUE;
	return FALSE;
}

/*
 * cleanup:
 *	Exit with the given value, cleaning up any droppings lying around
 */
void
cleanup(eval)
	int	eval;
{
	PLAYER	*pp;

	/* Place their cursor in a friendly position: */
	cgoto(ALL_PLAYERS, HEIGHT, 0);

	/* Send them all the ENDWIN command: */
	sendcom(ALL_PLAYERS, ENDWIN, LAST_PLAYER);

	/* And close their connections: */
	FOR_EACH_PLAYER(pp)
		(void) fclose(pp->p_output);

	/* Close the server socket: */
	(void) close(Socket);

	/* The end: */
	logx(LOG_INFO, "game over");
	exit(eval);
}

/*
 * send_stats:
 *	Accept a connection to the statistics port, and emit
 *	the stats.
 */
static void
send_stats()
{
	FILE	*fp;
	int	s;
	struct sockaddr_in	sockstruct;
	socklen_t	socklen;
	int	flags;

	/* Accept a connection to the statistics socket: */
	socklen = sizeof sockstruct;
	s = accept(Status, (struct sockaddr *) &sockstruct, &socklen);
	if (s < 0) {
		if (errno == EINTR)
			return;
		logx(LOG_ERR, "accept");
		return;
	}

#ifdef HAVE_LIBWRAP
	{
	    struct request_info ri;
	    /* Check for access permissions: */
	    request_init(&ri, RQ_DAEMON, "huntd", RQ_FILE, s, 0);
	    fromhost(&ri);
	    if (hosts_access(&ri) == 0) {
		logx(LOG_INFO, "rejected connection from %s", eval_client(&ri));
		close(s);
		return;
	    }
	}
#endif

	/* Don't allow the writes to block: */
	flags = fcntl(s, F_GETFL, 0);
	flags |= O_NDELAY;
	(void) fcntl(s, F_SETFL, flags);

	fp = fdopen(s, "w");
	if (fp == NULL) {
		logit(LOG_ERR, "fdopen");
		(void) close(s);
		return;
	}

	print_stats(fp);

	(void) fclose(fp);
}

/*
 * print_stats:
 * 	emit the game statistics
 */
void
print_stats(fp)
	FILE *fp;
{
	IDENT	*ip;
	PLAYER  *pp;

	/* Send the statistics as raw text down the socket: */
	fputs("Name\t\tScore\tDucked\tAbsorb\tFaced\tShot\tRobbed\tMissed\tSlimeK\n", fp);
	for (ip = Scores; ip != NULL; ip = ip->i_next) {
		fprintf(fp, "%s%c%c%c\t", ip->i_name,
			ip->i_team == ' ' ? ' ' : '[',
			ip->i_team,
			ip->i_team == ' ' ? ' ' : ']'
		);
		if (strlen(ip->i_name) + 3 < 8)
			putc('\t', fp);
		fprintf(fp, "%.2f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
			ip->i_score, ip->i_ducked, ip->i_absorbed,
			ip->i_faced, ip->i_shot, ip->i_robbed,
			ip->i_missed, ip->i_slime);
	}
	fputs("\n\nName\t\tEnemy\tFriend\tDeaths\tStill\tSaved\tConnect\n", fp);
	for (ip = Scores; ip != NULL; ip = ip->i_next) {
		fprintf(fp, "%s%c%c%c\t", ip->i_name,
			ip->i_team == ' ' ? ' ' : '[',
			ip->i_team,
			ip->i_team == ' ' ? ' ' : ']'
		);
		if (strlen(ip->i_name) + 3 < 8)
			putc('\t', fp);
		fprintf(fp, "%d\t%d\t%d\t%d\t%d\t",
			ip->i_gkills, ip->i_bkills, ip->i_deaths,
			ip->i_stillb, ip->i_saved);
		for (pp = Player; pp < End_player; pp++)
			if (pp->p_ident == ip)
				putc('p', fp);
		for (pp = Monitor; pp < End_monitor; pp++)
			if (pp->p_ident == ip)
				putc('m', fp);
		putc('\n', fp);
	}
}


/*
 * Send the game statistics to the controlling tty
 */
static void
siginfo(sig)
	int sig;
{
	int tty;
	FILE *fp;

	if ((tty = open(_PATH_TTY, O_WRONLY)) >= 0) {
		fp = fdopen(tty, "w");
		print_stats(fp);
		answer_info(fp);
		fclose(fp);
	}
}

/*
 * clear_scores:
 *	Clear the Scores list.
 */
static void
clear_scores()
{
	IDENT	*ip, *nextip;

	/* Release the list of scores: */
	for (ip = Scores; ip != NULL; ip = nextip) {
		nextip = ip->i_next;
		free((char *) ip);
	}
	Scores = NULL;
}

/*
 * announce_game:
 *	Publically announce the game
 */
static void
announce_game()
{

	/* TODO: could use system() to do something user-configurable */
}

/*
 * Handle a UDP packet sent to the well known port.
 */
static void
handle_wkport(fd)
	int fd;
{
	struct sockaddr		fromaddr;
	socklen_t		fromlen;
	uint16_t		query;
	uint16_t		response;

#ifdef HAVE_LIBWRAP
	{
	    struct request_info	ri;
	    request_init(&ri, RQ_DAEMON, "huntd", RQ_FILE, fd, 0);
	    fromhost(&ri);
	    /* Do we allow access? */
	    if (hosts_access(&ri) == 0) {
		logx(LOG_INFO, "rejected connection from %s", eval_client(&ri));
		return;
	    }
	}
#endif

	fromlen = sizeof fromaddr;
	if (recvfrom(fd, &query, sizeof query, 0, &fromaddr, &fromlen) == -1)
	{
		logit(LOG_WARNING, "recvfrom");
		return;
	}

#ifdef DEBUG
	fprintf(stderr, "query %d (%s) from %s:%d\n", query,
		query == C_MESSAGE ? "C_MESSAGE" :
		query == C_SCORES ? "C_SCORES" :
		query == C_PLAYER ? "C_PLAYER" :
		query == C_MONITOR ? "C_MONITOR" : "?",
		inet_ntoa(((struct sockaddr_in *)&fromaddr)->sin_addr),
		ntohs(((struct sockaddr_in *)&fromaddr)->sin_port));
#endif


	query = ntohs(query);

	switch (query) {
	  case C_MESSAGE:
		if (Nplayer <= 0)
			/* Don't bother replying if nobody to talk to: */
			return;
		/* Return the number of people playing: */
		response = Nplayer;
		break;
	  case C_SCORES:
		/* Someone wants the statistics port: */
		response = stat_port;
		break;
	  case C_PLAYER:
	  case C_MONITOR:
		/* Someone wants to play or watch: */
		if (query == C_MONITOR && Nplayer <= 0)
			/* Don't bother replying if there's nothing to watch: */
			return;
		/* Otherwise, tell them how to get to the game: */
		response = sock_port;
		break;
	  default:
		logit(LOG_INFO, "unknown udp query %d", query);
		return;
	}

	response = ntohs(response);
	if (sendto(fd, &response, sizeof response, 0,
	    &fromaddr, sizeof fromaddr) == -1)
		logit(LOG_WARNING, "sendto");
}
