/*	$OpenBSD: conf.c,v 1.6 2004/01/16 00:13:19 espie Exp $	*/
/* David Leonard <d@openbsd.org>, 1999. Public domain. */

#include "../config.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>

#include "hunt.h"
#include "server.h"
#include "conf.h"
#include "lib.h"

/* Configuration option variables for the server: */

int conf_random =	1;
int conf_reflect =	1;
int conf_monitor =	1;
int conf_ooze =		1;
int conf_fly =		1;
int conf_volcano =	1;
int conf_drone =	1;
int conf_boots =	1;
int conf_dropboots =	1;
int conf_scan =		1;
int conf_cloak =	1;
int conf_rounds =	1;
int conf_show_pain =	0;
int conf_pain_message =	1;
int conf_msg_self =	1;
int conf_color =	1;
int conf_share_vision =	1;
int conf_share_scan =	0;
int conf_logerr =	1;
int conf_syslog =	0;
int conf_debug =	0;

int conf_scoredecay =	15;
int conf_maxremove =	40;
int conf_linger =	90;

int conf_flytime =	20;
int conf_flystep =	5;
int conf_volcano_max =	50;
int conf_ptrip_face =	2;
int conf_ptrip_back =	95;
int conf_ptrip_side =	50;
int conf_prandom =	1;
int conf_preflect =	1;
int conf_pshot_coll =	5;
int conf_pgren_coll =	10;
int conf_pgren_catch =	10;
int conf_pmiss =	5;
int conf_pdroneabsorb =	1;
int conf_fall_frac =	5;
int conf_pmine_cost =	2;

int conf_bulspd =	5;
int conf_ishots =	15;
int conf_nshots =	5;
int conf_maxncshot =	2;
int conf_cool_time =	5;
int conf_maxdam =	10;
int conf_mindam =	5;
int conf_stabdam =	2;
int conf_killgain =	2;
int conf_slimefactor =	2;
int conf_slimespeed =	3;
int conf_lavaspeed =	1;
int conf_cloaklen =	20;
int conf_scanlen =	20;
int conf_mindshot =	2;
int conf_simstep =	150000;
int conf_message_time = 1500000;


struct kwvar {
	char *	kw;
	void *	var;
	enum vartype { Vint, Vchar, Vstring, Vdouble } type;
};

static struct kwvar keywords[] = {
	{ "random",		&conf_random,		Vint },
	{ "reflect",		&conf_reflect,		Vint },
	{ "monitor",		&conf_monitor,		Vint },
	{ "ooze",		&conf_ooze,		Vint },
	{ "fly",		&conf_fly,		Vint },
	{ "volcano",		&conf_volcano,		Vint },
	{ "drone",		&conf_drone,		Vint },
	{ "boots",		&conf_boots,		Vint },
	{ "dropboots",		&conf_dropboots,	Vint },
	{ "scan",		&conf_scan,		Vint },
	{ "cloak",		&conf_cloak,		Vint },
	{ "rounds",		&conf_rounds,		Vint },
	{ "show_pain",		&conf_show_pain,	Vint },
	{ "pain_message",	&conf_pain_message,	Vint },
	{ "msg_self",		&conf_msg_self,		Vint },
	{ "color",		&conf_color,		Vint },
	{ "share_vision",	&conf_share_vision,	Vint },
	{ "share_scan",		&conf_share_scan,	Vint },
	{ "logerr",		&conf_logerr,		Vint },
	{ "syslog",		&conf_syslog,		Vint },
	{ "debug",		&conf_debug,		Vint },
	{ "scoredecay",		&conf_scoredecay,	Vint },
	{ "maxremove",		&conf_maxremove,	Vint },
	{ "linger",		&conf_linger,		Vint },

	{ "flytime",		&conf_flytime,		Vint },
	{ "flystep",		&conf_flystep,		Vint },
	{ "volcano_max",	&conf_volcano_max,	Vint },
	{ "ptrip_face",		&conf_ptrip_face,	Vint },
	{ "ptrip_back",		&conf_ptrip_back,	Vint },
	{ "ptrip_side",		&conf_ptrip_side,	Vint },
	{ "prandom",		&conf_prandom,		Vint },
	{ "preflect",		&conf_preflect,		Vint },
	{ "pshot_coll",		&conf_pshot_coll,	Vint },
	{ "pgren_coll",		&conf_pgren_coll,	Vint },
	{ "pgren_catch",	&conf_pgren_catch,	Vint },
	{ "pmiss",		&conf_pmiss,		Vint },
	{ "pdroneabsorb",	&conf_pdroneabsorb,	Vint },
	{ "fall_frac",		&conf_fall_frac,	Vint },
	{ "pmine_cost",		&conf_pmine_cost,	Vint },

	{ "bulspd",		&conf_bulspd,		Vint },
	{ "ishots",		&conf_ishots,		Vint },
	{ "nshots",		&conf_nshots,		Vint },
	{ "maxncshot",		&conf_maxncshot,	Vint },
	{ "cool_time",		&conf_cool_time,	Vint },
	{ "maxdam",		&conf_maxdam,		Vint },
	{ "mindam",		&conf_mindam,		Vint },
	{ "stabdam",		&conf_stabdam,		Vint },
	{ "killgain",		&conf_killgain,		Vint },
	{ "slimefactor",	&conf_slimefactor,	Vint },
	{ "slimespeed",		&conf_slimespeed,	Vint },
	{ "lavaspeed",		&conf_lavaspeed,	Vint },
	{ "cloaklen",		&conf_cloaklen,		Vint },
	{ "scanlen",		&conf_scanlen,		Vint },
	{ "mindshot",		&conf_mindshot,		Vint },
	{ "simstep",		&conf_simstep,		Vint },
	{ "message_time",	&conf_message_time,	Vint },

	{ NULL, NULL, Vint }
};

static char *
parse_int(p, kvp, fnm, linep)
	char *p;
	struct kwvar *kvp;
	const char *fnm;
	int *linep;
{
	char *valuestart, *digitstart;
	char savec;
	int newval;

	/* expect a number */
	valuestart = p;
	if (*p == '-') 
		p++;
	digitstart = p;
	while (*p && isdigit(*p))
		p++;
	if ((*p == '\0' || isspace(*p) || *p == '#') && digitstart != p) {
		savec = *p;
		*p = '\0';
		newval = atoi(valuestart);
		*p = savec;
		logx(LOG_INFO, "%s:%d: %s: %d -> %d", 
			fnm, *linep, kvp->kw, *(int *)kvp->var, newval);
		*(int *)kvp->var = newval;
		return p;
	} else {
		logx(LOG_ERR, "%s:%d: invalid integer value \"%s\"", 
		    fnm, *linep, valuestart);
		return NULL;
	}
}

static char *
parse_value(p, kvp, fnm, linep)
	char *p;
	struct kwvar *kvp;
	const char *fnm;
	int *linep;
{

	switch (kvp->type) {
	case Vint:
		return parse_int(p, kvp, fnm, linep);
	case Vchar:
	case Vstring:
	case Vdouble:
		/* tbd */
	default:
		abort();
	}
}

static void
parse_line(buf, fnm, line)
	char *buf;
	char *fnm;
	int *line;
{
	char *p;
	char *word;
	char *endword;
	struct kwvar *kvp;
	char savec;

	p = buf;

	/* skip leading white */
	while (*p && isspace(*p))
		p++;
	/* allow blank lines and comment lines */
	if (*p == '\0' || *p == '#')
		return;

	/* walk to the end of the word: */
	word = p;
	if (*p && (isalpha(*p) || *p == '_')) {
		p++;
		while (*p && (isalpha(*p) || isdigit(*p) || *p == '_'))
			p++;
	}
	endword = p;

	if (endword == word) {
		logx(LOG_ERR, "%s:%d: expected variable name",
			fnm, *line);
		return;
	}

	/* match the configuration variable name */
	savec = *endword;
	*endword = '\0';
	for (kvp = keywords; kvp->kw; kvp++) 
		if (strcmp(kvp->kw, word) == 0)
			break;
	*endword = savec;

	if (kvp->kw == NULL) {
		logx(LOG_ERR, 
		    "%s:%d: unrecognised variable \"%.*s\"", 
		    fnm, *line, endword - word, word);
		return;
	}

	/* skip whitespace */
	while (*p && isspace(*p))
		p++;

	if (*p++ != '=') {
		logx(LOG_ERR, "%s:%d: expected `=' after %s", fnm, *line, word);
		return;
	}

	/* skip whitespace */
	while (*p && isspace(*p))
		p++;

	/* parse the value */
	p = parse_value(p, kvp, fnm, line);
	if (!p) 
		return;

	/* skip trailing whitespace */
	while (*p && isspace(*p))
		p++;

	if (*p && *p != '#') {
		logx(LOG_WARNING, "%s:%d: trailing garbage ignored",
			fnm, *line);
	}
}


static void
load_config(f, fnm)
	FILE *	f;
	char *	fnm;
{
	char buf[BUFSIZ];
	size_t len;
	int line;
	char *p;

	line = 0;
	
while ((p = fgetln(f, &len)) != NULL) {
		line++;
		if (p[len-1] == '\n')
			len--;
		if (len >= sizeof(buf)) {
			logx(LOG_ERR, "%s:%d: line too long", fnm, line);
			continue;
		}
		(void)memcpy(buf, p, len);
		buf[len] = '\0';
		parse_line(buf, fnm, &line);
	}
}

/*
 * load various config file, allowing later ones to 
 * overwrite earlier values
 */
void
config()
{
	char *home;
	char nm[MAXNAMLEN + 1];
	static char *fnms[] = { 
		"/etc/hunt.conf",
		"%s/.hunt.conf", 
		".hunt.conf", 
		NULL
	};
	int fn;
	FILE *f;

	/* All the %s's get converted to $HOME */
	if ((home = getenv("HOME")) == NULL)
		home = "";

	for (fn = 0; fnms[fn]; fn++) {
		snprintf(nm, sizeof nm, fnms[fn], home);
		if ((f = fopen(nm, "r")) != NULL) {
			load_config(f, nm);
			fclose(f);
		} 
		else if (errno != ENOENT)
			logit(LOG_WARNING, "%s", nm);
	}
}

/*
 * Parse a single configuration argument given on the command line
 */
void
config_arg(arg)
	char *arg;
{
	int line = 0;

	parse_line(arg, "*Initialisation*", &line);
}
