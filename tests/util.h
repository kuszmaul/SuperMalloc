// stuff from jemalloc util.h
#include <errno.h>
#include <string.h>

/* Sets error code */
static inline void
set_errno(int errnum)
{

#ifdef _WIN32
	SetLastError(errnum);
#else
	errno = errnum;
#endif
}

/* Get last error code */
static inline int
get_errno(void)
{

#ifdef _WIN32
	return (GetLastError());
#else
	return (errno);
#endif
}

// from util.c
/*
 * glibc provides a non-standard strerror_r() when _GNU_SOURCE is defined, so
 * provide a wrapper.
 */
int
buferror(char *buf, size_t buflen)
{

#ifdef _WIN32
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
	    (LPSTR)buf, buflen, NULL);
	return (0);
#elif defined(_GNU_SOURCE)
	char *b = strerror_r(errno, buf, buflen);
	if (b != buf) {
		strncpy(buf, b, buflen);
		buf[buflen-1] = '\0';
	}
	return (0);
#else
	return (strerror_r(errno, buf, buflen));
#endif
}
