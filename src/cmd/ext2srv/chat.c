//%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "dat.h"
#include "fns.h"

#if 1 //----------------------
#include  <l4all.h>
#endif //----------------------

#define	SIZE	1024
#define	DOTDOT	(&fmt+1)

int	chatty;

void
chat(char *fmt, ...)
{
	char buf[SIZE], *out;
	va_list arg;

	if (!chatty)
		return;

	va_start(arg, fmt);
	out = vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
#if 0 //%-------
	*(out+1) = 0;
	l4printf_b("%s", buf);
#else //plan9
	write(2, buf, (long)(out-buf));
#endif //-------------
}

void
mchat(char *fmt, ...)
{
	char buf[SIZE], *out;
	va_list arg;

	va_start(arg, fmt);
	out = vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
#if 0 //%-------
	*(out+1) = 0;
	l4printf_b("%s\n", buf);
#else //plan9
	write(2, buf, (long)(out-buf));
#endif //-------------
}

void
panic(char *fmt, ...)
{
	char buf[SIZE];
	va_list arg;
	int n;

	n = sprint(buf, "%s %d: panic ", argv0, getpid());
	va_start(arg, fmt);
	vseprint(buf+n, buf+sizeof(buf)-n, fmt, arg);
	va_end(arg);
#if 0 //-------------------
	l4printf_g("Ext2srv panic\n");
	
#else //plan9 -------
	fprint(2, "%s: %r\n", buf);
	//% exits("panic");
	L4_Sleep(L4_Never);
#endif //-----------------
}
