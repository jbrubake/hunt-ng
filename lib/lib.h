#include <stdio.h>

size_t strlcat(char *, const char *, size_t);
size_t strlcpy(char *, const char *, size_t);
#ifdef LINUX
char *fgetln(FILE *, size_t *);
#endif
