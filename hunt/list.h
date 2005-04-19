/*	$OpenBSD: list.h,v 1.3 2003/06/17 00:36:36 pjanzen Exp $	*/
/*	David Leonard <d@openbsd.org>, 1999.  Public domain.	*/

/* linux doesn't have sa_len field in sockaddr struct */
#ifndef SA_LEN
#ifdef __GLIBC__
#define SA_LEN(x)      (((x)->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) \
                               : sizeof(struct sockaddr_in))
#else
#define SA_LEN(x)      ((x)->sa_len)
#endif
#endif

struct driver {
	struct sockaddr addr;
	u_int16_t	response;
	int		once;
};

extern struct driver *drivers;
extern int numdrivers;
extern u_int16_t Server_port;

struct  driver *next_driver(void);
struct  driver *next_driver_fd(int);
const char *	driver_name(struct driver *);
void	probe_drivers(u_int16_t, char *);
void	probe_cleanup(void);
