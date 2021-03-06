/*	$OpenBSD: list.c,v 1.4 2001/06/04 02:41:34 d Exp $	*/
/*
 * Copyright 2001, David Leonard. All rights reserved.
 * Redistribution and use in source and binary forms with or without
 * modification are permitted provided that this notice is preserved.
 * This software is provided ``as is'' without express or implied warranty.
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include <sys/ioctl.h>

#include <netinet/in.h>
#include <net/if.h>

#include <arpa/inet.h>

#include "hunt.h"
#include "list.h"

/* Wait at most 5 seconds for a reply */
#define LIST_DELAY	5

struct driver *drivers = NULL;
int numdrivers = 0;
int maxdrivers = 0;

uint16_t Server_port;

static int numprobes = 0;
static int probe_sock[64];
static struct timeval probe_timeout;

struct driver *
next_driver()
{

	return next_driver_fd(-1);
}

struct driver *
next_driver_fd(fd)
	int	fd;
{
	fd_set	r;
	int	maxfd = -1;
	int	i, s, ret, len;
	struct driver *driver;
	uint16_t resp;

	if (fd == -1 && numprobes == 0)
		return NULL;

    again:
	FD_ZERO(&r);
	if (fd != -1) {
		FD_SET(fd, &r);
		maxfd = fd;
	}
	for (i = 0; i < numprobes; i++) {
		FD_SET(probe_sock[i], &r);
		if (probe_sock[i] > maxfd)
			maxfd = probe_sock[i];
	}

	probe_timeout.tv_sec = LIST_DELAY;
	probe_timeout.tv_usec = 0;
	ret = select(maxfd + 1, &r, NULL, NULL, &probe_timeout);

	if (ret == -1) {
		if (errno == EINTR)
			goto again;
		err(1, "select");
	}

	if (ret == 0) {
		/* Timeout - close all sockets */
		for (i = 0; i < numprobes; i++)
			close(probe_sock[i]);
		numprobes = 0;
		return NULL;
	}

	if (fd != -1 && FD_ISSET(fd, &r)) 
		/* Keypress. Return magic number */
		return (struct driver *)-1;

	for (i = 0; i < numprobes; i++)
		/* Find the first ready socket */
		if (FD_ISSET(probe_sock[i], &r))
			break;

	s = probe_sock[i];

	if (numdrivers >= maxdrivers) {
		if (maxdrivers) {
			maxdrivers *= 2;
			drivers = realloc(drivers, sizeof *driver * maxdrivers);
		} else {
			maxdrivers = 16;
			drivers = malloc(sizeof *driver * maxdrivers);
		}
		if (drivers == NULL)
			err(1, "malloc");
	}
	driver = &drivers[numdrivers];
	len = sizeof driver->addr;
	ret = recvfrom(s, &resp, sizeof resp, 0, &driver->addr, &len);
	if (ret == -1)
		goto again;
	driver->response = ntohs(resp);

	switch (driver->addr.sa_family) {
	case AF_INET:
	case AF_INET6:
		((struct sockaddr_in *)&driver->addr)->sin_port =
		    htons(driver->response);
		break;
	}
	numdrivers++;
	return driver;
}

/* Return the hostname for a driver. */
const char *
driver_name(driver)
	struct driver *driver;
{
	const char *name;
	static char buf[80];
	struct hostent *hp;
	struct sockaddr_in *sin;

	name = NULL;

	if (driver->addr.sa_family == AF_INET) {
		sin = (struct sockaddr_in *)&driver->addr;
		hp = gethostbyaddr((char *)&sin->sin_addr, 
		    sizeof sin->sin_addr, AF_INET);
		if (hp != NULL)
			name = hp->h_name;
		else {
			name = inet_ntop(AF_INET, &sin->sin_addr, 
			    buf, sizeof buf);
		}
	}

	return name;
}

static int
start_probe(addr, req)
	struct sockaddr *addr;
	uint16_t req;
{
	uint16_t msg;
	int s;
	int enable;

	if (numprobes >= (sizeof probe_sock / sizeof probe_sock[0])) {
		/* Just ridiculous */
		return -1;
	}

	s = socket(addr->sa_family, SOCK_DGRAM, 0);
	if (s < 0) {
		warn("socket");
		return -1;
	}

	enable = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof enable);

	switch (addr->sa_family) {
	case AF_INET:
	case AF_INET6:
		((struct sockaddr_in *)addr)->sin_port = 
		    htons(Server_port);
		break;
	}

	msg = htons(req);
	if (sendto(s, &msg, sizeof msg, 0, addr, SA_LEN(addr)) == -1)
		warn("sendto");
	probe_sock[numprobes++] = s;

	return 0;
}

void
probe_cleanup()
{
	int i;

	for (i = 0; i < numprobes; i++)
		close(probe_sock[i]);
	numprobes = 0;
}

/*
 * If we have no preferred host then send a broadcast message to everyone.
 * Otherwise, send the request message only to the preferred host.
 */
void
probe_drivers(req, preferred)
	uint16_t	req;
	char 		*preferred;
{
	struct sockaddr_in *target;
	struct sockaddr_in localhost;
	struct hostent *he;
        char *inbuf = NULL, *ninbuf;
        struct ifconf ifc;
        struct ifreq *ifr;
        int fd, inlen = 8192;
        int i, len;

	numdrivers = 0;

	probe_cleanup();

	/* Send exclusively to a preferred host. */
	if (preferred) {
		struct sockaddr_in sin;

		target = NULL;

		if (!target) {
			sin.sin_family = AF_INET;
#ifdef SIN_HAS_SIN_LEN
			sin.sin_len = sizeof sin;
#endif
			if (inet_pton(AF_INET, preferred, &sin.sin_addr) == 1)
				target = &sin;
		}

		if (!target && (he = gethostbyname(preferred)) != NULL) {
			sin.sin_family = he->h_addrtype;
#ifdef SIN_HAS_SIN_LEN
			sin.sin_len = sizeof sin;
#endif
			memcpy(&sin.sin_addr, he->h_addr, he->h_length);
			target = &sin;
		}

		if (!target)
			errx(1, "Bad hostname: %s", preferred);

		start_probe(target, req);
		return;
	}

	/* Send a query to the local machine: */
	localhost.sin_family = AF_INET;
#ifdef SIN_HAS_SIN_LEN
	localhost.sin_len = sizeof localhost;
#endif
	localhost.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	start_probe((struct sockaddr *)&localhost, req);

        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
                err(1, "socket");

	/* Find all attached networks: */
        while (1) {
                ifc.ifc_len = inlen;
                if ((ninbuf = realloc(inbuf, inlen)) == NULL)
			err(1, "malloc");
                ifc.ifc_buf = inbuf = ninbuf;
                if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) 
                        err(1, "SIOCGIFCONF");
                if (ifc.ifc_len + sizeof(*ifr) < inlen)
                        break;
                inlen *= 2;
        }

	/* Send a request to every attached broadcast address: */
        ifr = ifc.ifc_req;
        for (i = 0; i < ifc.ifc_len;
             i += len, ifr = (struct ifreq *)((caddr_t)ifr + len)) {
                len = sizeof(ifr->ifr_name) +
                      (SA_LEN(&(ifr->ifr_addr)) > sizeof(struct sockaddr) ?
                       SA_LEN(&(ifr->ifr_addr)) : sizeof(struct sockaddr));

		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;

                if (ioctl(fd, SIOCGIFFLAGS, (caddr_t)ifr) < 0) {
                        warn("%s: SIOCGIFFLAGS", ifr->ifr_name);
			continue;
		}
                if ((ifr->ifr_flags & IFF_UP) == 0)
			continue;
		if ((ifr->ifr_flags & IFF_BROADCAST) != 0) {
			if (ioctl(fd, SIOCGIFBRDADDR, (caddr_t)ifr) < 0)  {
				warn("%s: SIOCGIFBRDADDR", ifr->ifr_name);
				continue;
			}
			target = (struct sockaddr_in *)&ifr->ifr_dstaddr;
		} else if ((ifr->ifr_flags & IFF_POINTOPOINT) != 0) {
			if (ioctl(fd, SIOCGIFDSTADDR, (caddr_t)ifr) < 0)  {
				warn("%s: SIOCGIFDSTADDR", ifr->ifr_name);
				continue;
			}
			target = (struct sockaddr_in *)&ifr->ifr_broadaddr;
		} else
			continue;

		start_probe(target, req);
        }
        free(inbuf);
        (void) close(fd);
}
