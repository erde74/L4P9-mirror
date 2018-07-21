/*
 * (m) YS	NII
 */
#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"			//% ../pc/
#include	"../pc/dat.h"			//% ../pc/
#include	"../pc/fns.h"			//% ../pc/
#include	"../port/error.h"
#include	"../ip/ip.h"

enum {
	Nlog		= 4*1024,
};

/*
 *  action log
 */
struct Netlog {
	Lock  _lock;			//% _lock
	int	opens;
	char*	buf;
	char	*end;
	char	*rptr;
	int	len;

	int	logmask;		/* mask of things to debug */
	uchar	iponly[IPaddrlen];	/* ip address to print debugging for */
	int	iponlyset;

	QLock _qlock;			//% _lock
	Rendez _rendez;			//% _
};

typedef struct Netlogflag {
	char*	name;
	int	mask;
} Netlogflag;

static Netlogflag flags[] =
{
	{ "ppp",	Logppp, },
	{ "ip",		Logip, },
	{ "fs",		Logfs, },
	{ "tcp",	Logtcp, },
	{ "il",		Logil, },
	{ "icmp",	Logicmp, },
	{ "udp",	Logudp, },
	{ "compress",	Logcompress, },
	{ "ilmsg",	Logil|Logilmsg, },
	{ "gre",	Loggre, },
	{ "tcpwin",	Logtcp|Logtcpwin, },
	{ "tcprxmt",	Logtcp|Logtcprxmt, },
	{ "udpmsg",	Logudp|Logudpmsg, },
	{ "ipmsg",	Logip|Logipmsg, },
	{ "esp",	Logesp, },
	{ nil,		0, },
};

char Ebadnetctl[] = "too few arguments for netlog control message";

enum
{
	CMset,
	CMclear,
	CMonly,
};

static
Cmdtab routecmd[] = {
	{CMset,		"set",		0},		//% { ... }
	{CMclear,	"clear",	0},		//% { ... }
	{CMonly,	"only",		0},		//% { ... }
};

void
netloginit(Fs *f)
{
	f->alog = smalloc(sizeof(Netlog));
}

void
netlogopen(Fs *f)
{
	lock(&f->alog->_lock);			//% _lock
	if(waserror()){
		unlock(&f->alog->_lock);	//% _lock
		nexterror();
	}
	if(f->alog->opens == 0){
		if(f->alog->buf == nil)
			f->alog->buf = malloc(Nlog);
		f->alog->rptr = f->alog->buf;
		f->alog->end = f->alog->buf + Nlog;
	}
	f->alog->opens++;
	unlock(&f->alog->_lock);		//% _lock
	poperror();
}

void
netlogclose(Fs *f)
{
	lock(&f->alog->_lock);			//% _lock
	if(waserror()){
		unlock(&f->alog->_lock);	//% _lock
		nexterror();
	}
	f->alog->opens--;
	if(f->alog->opens == 0){
		free(f->alog->buf);
		f->alog->buf = nil;
	}
	unlock(&f->alog->_lock);		//% _lock
	poperror();
}

static int
netlogready(void *a)
{
	Fs *f = a;

	return f->alog->len;
}

long
netlogread(Fs *f, void *a, ulong _x, long n)
{
	int i, d;
	char *p, *rptr;

	qlock(&f->alog->_qlock);		//% _qlock
	if(waserror()){
		qunlock(&f->alog->_qlock);	//% _qlock
		nexterror();
	}

	for(;;){
		lock(&f->alog->_lock);		//% _lock
		if(f->alog->len){
			if(n > f->alog->len)
				n = f->alog->len;
			d = 0;
			rptr = f->alog->rptr;
			f->alog->rptr += n;
			if(f->alog->rptr >= f->alog->end){
				d = f->alog->rptr - f->alog->end;
				f->alog->rptr = f->alog->buf + d;
			}
			f->alog->len -= n;
			unlock(&f->alog->_lock);	//% _lock

			i = n-d;
			p = a;
			memmove(p, rptr, i);
			memmove(p+i, f->alog->buf, d);
			break;
		}
		else
			unlock(&f->alog->_lock);		//% _lock

		sleep(&f->alog->_rendez, netlogready, f);	//% _rendez
	}

	qunlock(&f->alog->_qlock);				//% _qlock
	poperror();

	return n;
}

void
netlogctl(Fs *f, char* s, int n)
{
	int i, set;
	Netlogflag *fp;
	Cmdbuf *cb;
	Cmdtab *ct;

	cb = parsecmd(s, n);
	if(waserror()){
		free(cb);
		nexterror();
	}

	if(cb->nf < 2)
		error(Ebadnetctl);

	ct = lookupcmd(cb, routecmd, nelem(routecmd));

	set = 0;	//%	instead of SET(set);

	switch(ct->index){
	case CMset:
		set = 1;
		break;

	case CMclear:
		set = 0;
		break;

	case CMonly:
		parseip(f->alog->iponly, cb->f[1]);
		if(ipcmp(f->alog->iponly, IPnoaddr) == 0)
			f->alog->iponlyset = 0;
		else
			f->alog->iponlyset = 1;
		free(cb);
		return;

	default:
		cmderror(cb, "unknown ip control message");
	}

	for(i = 1; i < cb->nf; i++){
		for(fp = flags; fp->name; fp++)
			if(strcmp(fp->name, cb->f[i]) == 0)
				break;
		if(fp->name == nil)
			continue;
		if(set)
			f->alog->logmask |= fp->mask;
		else
			f->alog->logmask &= ~fp->mask;
	}

	free(cb);
	poperror();
}

void
netlog(Fs *f, int mask, char *fmt, ...)
{
	char buf[128], *t, *fp;
	int i, n;
	va_list arg;

	if(!(f->alog->logmask & mask))
		return;

	if(f->alog->opens == 0)
		return;

	va_start(arg, fmt);
	n = vseprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);

	lock(&f->alog->_lock);				//% _lock
	i = f->alog->len + n - Nlog;
	if(i > 0){
		f->alog->len -= i;
		f->alog->rptr += i;
		if(f->alog->rptr >= f->alog->end)
			f->alog->rptr = f->alog->buf + (f->alog->rptr - f->alog->end);
	}
	t = f->alog->rptr + f->alog->len;
	fp = buf;
	f->alog->len += n;
	while(n-- > 0){
		if(t >= f->alog->end)
			t = f->alog->buf + (t - f->alog->end);
		*t++ = *fp++;
	}
	unlock(&f->alog->_lock);			//% _lock

	wakeup(&f->alog->_rendez);			//% _rendez
}
