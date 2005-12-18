/* TODO: This is all my (MDB) code, so can I put it under the GPL? Or would
 * that lead to horrible legal ugliness? For now at least it's BSD:
 *
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "hunt.h"
#include "conf.h"
#include "server.h"

static void	commands_help(PLAYER *);
static PLAYER	*player_by_name(char *);
static void	msg(PLAYER *, PLAYER *, char *);
static int	repeat_mangle(PLAYER *, char *, char *);
static int	at_least_since(struct timeval, struct timeval);

/* 
 * start_command:
 *	set up ready for typing a command
 */
void
start_command(PLAYER *pp)
{
	pp->p_inputmode = INPUTMODE_COM;
	pp->p_com_len = 0;
	outyx(pp, HEIGHT, 0, COMMAND_PROMPT);
	ce(pp);
}

/*
 * process_command:
 *	handle in-game IRC-style command (often just a message to all) from
 *	player
 */
void
process_command(PLAYER *pp, char *command)
{
	if (*command == '\0')
		return;

	/* Check for actual commands */
	if (*command == '/') {
		if (strncmp(command, "/msg ", 5) == 0) {
			/* send msg to target */
			char    target[strlen(command)];
			int	chars_read = 0;

			command += 4;

			/* %n in sscanf doesn't increment the return value in
			 * all implementations, so we check chars_read too: */
			if (sscanf(command, " %s %n", target, &chars_read) < 1 || !chars_read)
				message(pp, TRUE, "Usage: /msg [player|team_number] [message]");
			else {
				/* Try to match the target to be a team
				 * number, else to be a player name.
				 * XXX: Players named after numbers don't get
				 *  any messages. Hard cheese.
				 * */

				char	*message = command+chars_read;
				int	team_number;
				char	*endptr;
				PLAYER	*to_pp;

				team_number = strtol(target, &endptr, 0);

				if (1 <= team_number && team_number <= 9 && *endptr == '\0') {
					/* Matched as a team number */
					for (to_pp = Player; to_pp < End_player; to_pp++)
						if (to_pp->p_ident->i_team == '0'+team_number)
							msg(pp, to_pp, message);
				}
				else {
					/* Not a team number - try as a name */
					to_pp = player_by_name(target);
					if (to_pp) {
						msg(pp, to_pp, message);
					}
					else
						message_formatted(pp, TRUE, "There's no %s here!", target);
				}
			}
		}
		else if (strncmp(command, "/inv", 4) == 0) {
			char	*inv_string;

			if (pp->p_nboots)
				if (pp->p_nboots == 2)
					inv_string = "a pair of boots";
				else
					inv_string = "a boot";
			else
				inv_string = "nothing";

			message_formatted(pp, TRUE, "You have: %s", inv_string);
		}
		else if (strncmp(command, "/dropboots", 10) == 0) {

			if (!pp->p_nboots) {
				message(pp, TRUE, "But you aren't wearing any boots!");
				return;
			}

			if (pp->p_over != SPACE) {
				message(pp, TRUE, "No room");
				return;
			}

			message_formatted(pp, TRUE, "You drop your boot%s",
					(pp->p_nboots==2 ? "s" : ""));

			pp->p_over = (pp->p_nboots==2 ? BOOT_PAIR : BOOT);

			pp->p_nboots = 0;
		}
		else if (strncmp(command, "/xyzzy", 6) == 0
				&& conf_debug) {
			/* wizard (debugging) commands */
			char    xyzzy_command[strlen(command)];

			command += 6;
			if (sscanf(command, " %s", xyzzy_command) < 1)
				return;
			else {
				if (strncmp(xyzzy_command, "ammo", 4) == 0) {
					pp->p_ammo += 25;
					ammo_update(pp);
				}
				/* These break the boot code:
				if (strncmp(xyzzy_command, "boots", 5) == 0)
					pp->p_nboots=2;
				else if (strncmp(xyzzy_command, "boot", 4) == 0)
					pp->p_nboots=1;
				*/
				else return;

				message_formatted(ALL_PLAYERS, FALSE, "%s cheats!", pp->p_ident->i_name);
			}

		}
		else if (strncmp(command, "/help", 5) == 0) {
			char    help_command[strlen(command)];

			command += 5;
			if (sscanf(command, " %s", help_command) < 1)
				commands_help(pp);
			else {
				if (strncmp(help_command, "msg", 3) == 0 ||
						strncmp(help_command, "/msg", 4) == 0)
					message(pp, TRUE, "/msg [player|team_number] [message]: send message to player/team");
				else if (strncmp(help_command, "help", 4) == 0 ||
						strncmp(help_command, "/help", 5) == 0)
					message(pp, TRUE, "/help [command]: get help on command");
				else if (strncmp(help_command, "inv", 3) == 0 ||
						strncmp(help_command, "/inv", 4) == 0)
					message(pp, TRUE, "/inv: take an inventory");
				else if (strncmp(help_command, "dropboots", 9) == 0 ||
						strncmp(help_command, "/dropboots", 10) == 0)
					message(pp, TRUE, "/dropboots: drop any boots you're wearing");
				else
					commands_help(pp);
			}
		}
		else
			message(pp, TRUE, "No such command ('/help' for help)");
	}
	else {
		/* Not a command - send as a message to all other players */
		PLAYER *to_pp;

		FOR_EACH_PLAYER(to_pp)
			if (to_pp != pp || conf_msg_self)
				msg(pp, to_pp, command);
	}
}

static void
commands_help(PLAYER *pp)
{
	message(pp, TRUE, "Commands: /msg /help /inv /dropboots");
}

/*
 * player_by_name:
 *	return the player with a name, or NULL if no player has the name
 */
static PLAYER
*player_by_name(char *name)
{
	PLAYER *pp;

	/* FIXME: Can two players have the same name? */
	FOR_EACH_PLAYER(pp)
		if (strcmp(name, pp->p_ident->i_name) == 0)
			return pp;
	return NULL;
}

static void
msg(PLAYER *from_pp, PLAYER *to_pp, char *msg)
{
    char	teamstr[] = "[x]";

    teamstr[1] = from_pp->p_ident->i_team;

    message_formatted(to_pp, FALSE, "%s%s: %.*s",
	    from_pp->p_ident->i_name,
	    from_pp->p_ident->i_team == ' ' ? "": teamstr,
	    strlen(msg),
	    msg);
}

/*
 * message:
 *	Send player a message, adding it to the player's message queue.
 *	If urgent, message goes to the front of the queue displacing old ones
 *	    if necessary. 
 *	Else, it goes to the rear if there's room.
 */
void
message(pp, urgent, s)
	PLAYER	*pp;
	char	*s;
{
	char	*copy;
	int	len;
	if (pp == ALL_PLAYERS) {
		FOR_EACH_PLAYER(pp)
			message(pp, urgent, s);
		return;
	}
	/* Add to player's message buffer. 
	 * XXX: We malloc and strcpy for each player, which is wasteful of
	 * memory in the majority of cases but prevents awkwardness with
	 * reused memory. We're never talking more than a couple of K or so.
	 *
	 * XXX: This is a mixture of a FIFO and a FILO.
	 * Reasoning: We want urgent messages (e.g. "You've been stabbed!") to
	 * be viewed as soon as possible, even if that means viewing them out
	 * of order. Less urgent messages (e.g. "dave: hi bob") we can wait
	 * for, and we'd rather see in order.
	 */

	if (!urgent && pp->p_message_buf_num >= MAX_BUFFERED_MESSAGES)
		/* No room and non-urgent, so ignore */
		return;

	len = strlen(s);

	copy = malloc((len + 1) * sizeof(char));

	if (copy == NULL) {
	    logit(LOG_ERR, "malloc");
	    return;
	}

	strcpy(copy, s);


	if (urgent) {
#ifdef MESSAGE_DEBUG
		logit(LOG_INFO, ">! %s - %d %x %s", pp->p_ident->i_name, pp->p_message_buf_num, copy, copy);
#endif
		/* Urgent: goes straight to the front of the queue */

		if (pp->p_message_buf_num) {
			/* Check for duplication */
			char	*prev = pp->p_message_buf[pp->p_message_buf_num - 1];

			if (strncmp(copy, prev, len) == 0) {
				/* Looking like it's the same as the last message */
				if (repeat_mangle(pp, copy, prev)) {
					/* It is and we've edited the old
					 * message appropriately. Finish. */
					free(copy);
					return;
				}

			}
		}


		if (pp->p_message_buf_num >= MAX_BUFFERED_MESSAGES) {
			/* Shift back (lose earliest message) */
			int i;

			free(pp->p_message_buf[0]);
			for (i=0; i < MAX_BUFFERED_MESSAGES - 1; )
				pp->p_message_buf[i] = pp->p_message_buf[++i];

			pp->p_message_buf[MAX_BUFFERED_MESSAGES - 1] = copy;
		}
		else 
			pp->p_message_buf[pp->p_message_buf_num++] = copy;
	}
	else {
		/* Non-urgent - form an orderly queue please */

		/* Shift existing messages forward to make room */
		/* (Note that we've already checked for overflow) */
		int i;
#ifdef MESSAGE_DEBUG
		logit(LOG_INFO, "> %s - 0/%d %x %s", pp->p_ident->i_name, pp->p_message_buf_num, copy, copy);
#endif
		for (i = pp->p_message_buf_num; i > 0; )
			pp->p_message_buf[i] = pp->p_message_buf[--i];

		/* New message goes to rear of queue */
		pp->p_message_buf[0] = copy;
		pp->p_message_buf_num++;
	}

}

/*
 * Handle mangling of old messages when new identical ones come along.
 */

static int
repeat_mangle(PLAYER *pp, char *copy, char *prev)
{
	int	len = strlen(copy);
	int	prev_len = strlen(prev);
	char	*new;

	if (prev_len == len) {
		/* Is identical to previous. Add ' [x2]' to the end and we're
		 * done */
		new = realloc(prev, (len + 5 + 1) * sizeof(char));
		if (!new) {
			logit(LOG_ERR, "realloc");
			return TRUE;
		}

		prev = pp->p_message_buf[pp->p_message_buf_num - 1] = new;

		prev[len+0] = ' ';
		prev[len+1] = '[';
		prev[len+2] = 'x';
		prev[len+3] = '2';
		prev[len+4] = ']';
		prev[len+5] = '\0';

		return TRUE;
	}
	else {
		int	rep;
		if (sscanf(prev+len, " [x%d]", &rep) == 1) {
			/* increment the count */
			rep++;
			if (3 <= rep <= 9)
				*(prev+len+3) = '0' + rep;
			else
				/* one digit max */
				*(prev+len+3) = '*';

			return TRUE;
		}
	}
	return FALSE;
}
/* 
 * at_least_since:
 *	check if at least min_diff time has passed since time since
 */
static int
at_least_since(min_diff, since)
		struct timeval	min_diff, since;
{
		struct timeval  t, t2; 

		gettimeofday(&t, NULL);
		timersub(&t, &since, &t2);
		return timercmp(&t2, &min_diff, >=);
}

/*
 * message_formatted:
 *	send player a formatted message, with handling.
 */
void
message_formatted(pp, urgent, fmt)
	PLAYER	*pp;
	const char *fmt;
{
	va_list ap;
	char buf[BUFSIZ];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	message(pp, urgent, buf);
}

/*
 * send_next_message:
 *	display the next message player is due to view, if any such exist,
 *	else blank the message bar
 */
void
send_next_message(pp)
	PLAYER  *pp;
{
	if (pp->p_message_buf_num) {
		/* there is a message to send */
		pp->p_message_buf_num--;
#ifdef MESSAGE_DEBUG
		logit(LOG_INFO, "< %s - %d %x %s", pp->p_ident->i_name,
			pp->p_message_buf_num,
			pp->p_message_buf[pp->p_message_buf_num],
			pp->p_message_buf[pp->p_message_buf_num]);
#endif
		send_message(pp, pp->p_message_buf[pp->p_message_buf_num]);

		pp->p_message_displayed = TRUE;

		free(pp->p_message_buf[pp->p_message_buf_num]);
	}
	else if (pp->p_message_displayed) {
		cgoto(pp, HEIGHT, 0);
		ce(pp);

		pp->p_message_displayed = FALSE;
	}

}

/*
 * message_ready_for_next:
 *	true if player is ready to view a message
 */
int
message_ready_for_next(pp)
	PLAYER  *pp;
{
	struct timeval  t, t2; 

	if (pp->p_inputmode == INPUTMODE_COM) {
		return FALSE;
	}
	else 
		return at_least_since(min_message_time, pp->p_last_message_time);
}

/*
 * messages_pending:
 *	true if any player has a message yet to be viewed
 */
int
messages_pending()
{
	PLAYER *pp;

	/* Just check if any messages are in the queues
	 * would be a waste of cpu to run message_ready_for_next too */
	FOR_EACH_PLAYER(pp)
		if (pp->p_message_buf_num)
			return TRUE;
}
