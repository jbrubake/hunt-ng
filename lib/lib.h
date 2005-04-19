#include <stdio.h>

#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif
#ifndef HAVE_FGETLN
char *fgetln(FILE *, size_t *);
#endif
