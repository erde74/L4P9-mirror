//%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include "fmtdef.h"

extern void req2printsrv(char* s, int len, int color); //%

/*
 * generic routine for flushing a formatting buffer
 * to a file descriptor
 */
int
_fmtFdFlush(Fmt *f)
{
	int n;

	n = (char*)f->to - (char*)f->start;

#if 0 //%------------------------------------
	if  (n == 0)
	    return  1;
	if ((int)(uintptr)f->farg <= 2) {  // STDOUT, STDERR
	    ((char*)f->start)[n] = 0;
	    req2printsrv(f->start, n, 0);
	    f->to = f->start;
	    return 1;
	}
	return  1;
#else //%original------------------------------------

	if(n && write((int)(uintptr)f->farg, f->start, n) != n)
		return 0;
	f->to = f->start;
	return 1;
#endif //%----------------------------------------
}

int
vfprint(int fd, char *fmt, va_list args)
{
	Fmt f;
	char buf[256];
	int n;

	fmtfdinit(&f, fd, buf, sizeof(buf));
	f.args = args;
	n = dofmt(&f, fmt);
	if(n > 0 && _fmtFdFlush(&f) == 0)
		return -1;
	return n;
}
