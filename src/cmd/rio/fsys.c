//%
#include  <l4all.h>           //%
#include  <lp49/l_actobj.h>   //%

#include <u.h>
#include <libc.h>
#include <draw.h>
//#include <thread.h>	// HK 20100131
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>

#include "dat.h"
#include "fns.h"

#define  TESTCODE  1

L_thcb  *wctl_aobj,  *filsys_aobj;    //%
L_thcb  *winclose_aobj;  //%


char Eperm[] = "permission denied";
char Eexist[] = "file does not exist";
char Enotdir[] = "not a directory";
char Ebadfcall[] = "bad fcall type";
char Eoffset[] = "illegal offset";

int	messagesize = 8192+IOHDRSZ;	/* good start */

enum{
	DEBUG = 0
};

Dirtab dirtab[]=
{
	{ ".",		QTDIR,	Qdir,		0500|DMDIR },
	{ "cons",	QTFILE,	Qcons,		0600 },
	{ "cursor",	QTFILE,	Qcursor,	0600 },
	{ "consctl",	QTFILE,	Qconsctl,	0200 },
	{ "winid",	QTFILE,	Qwinid,		0400 },
	{ "winname",	QTFILE,	Qwinname,	0400 },
	{ "kbdin",	QTFILE,	Qkbdin,		0200 },
	{ "label",	QTFILE,	Qlabel,		0600 },
	{ "mouse",	QTFILE,	Qmouse,		0600 },
	{ "screen",	QTFILE,	Qscreen,	0400 },
	{ "snarf",	QTFILE,	Qsnarf,		0600 },
	{ "text",	QTFILE,	Qtext,		0400 },
	{ "wdir",	QTFILE,	Qwdir,		0600 },
	{ "wctl",	QTFILE,	Qwctl,		0600 },
	{ "window",	QTFILE,	Qwindow,	0400 },
	{ "wsys",	QTDIR,	Qwsys,		0500|DMDIR },
	{ nil, }
};

static uint	getclock(void);
static void	filsysproc(void*);
static Fid*	newfid(Filsys*, int);
static int	dostat(Filsys*, int, Dirtab*, uchar*, int, uint);

int	clockfd;
int	firstmessage = 1;

char	srvpipe[64];
char	srvwctl[64];

static	Xfid*	filsysflush(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysversion(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysauth(Filsys*, Xfid*, Fid*);
//% static	Xfid*	filsysnop(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysattach(Filsys*, Xfid*, Fid*);
static	Xfid*	filsyswalk(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysopen(Filsys*, Xfid*, Fid*);
static	Xfid*	filsyscreate(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysread(Filsys*, Xfid*, Fid*);
static	Xfid*	filsyswrite(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysclunk(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysremove(Filsys*, Xfid*, Fid*);
static	Xfid*	filsysstat(Filsys*, Xfid*, Fid*);
static	Xfid*	filsyswstat(Filsys*, Xfid*, Fid*);

Xfid* 	(*fcall[Tmax])(Filsys*, Xfid*, Fid*) =
{
	[Tflush]	= filsysflush,
	[Tversion]	= filsysversion,
	[Tauth]	= filsysauth,
	[Tattach]	= filsysattach,
	[Twalk]	= filsyswalk,
	[Topen]	= filsysopen,
	[Tcreate]	= filsyscreate,
	[Tread]	= filsysread,
	[Twrite]	= filsyswrite,
	[Tclunk]	= filsysclunk,
	[Tremove]= filsysremove,
	[Tstat]	= filsysstat,
	[Twstat]	= filsyswstat,
};


void
post(char *name, char *envname, int srvfd)
{
	int fd;
	char buf[32];

	fd = create(name, OWRITE|ORCLOSE|OCEXEC, 0600);
	if(fd < 0)
		error(name);
	sprint(buf, "%d",srvfd);
	if(write(fd, buf, strlen(buf)) != strlen(buf))
		error("srv write");
	putenv(envname, name);
}

/*
 * Build pipe with OCEXEC set on second fd.
 * Can't put it on both because we want to post one in /srv.
 */
int
cexecpipe(int *p0, int *p1)
{
	/* pipe the hard way to get close on exec */
        if(bind("#|", "/mnt/temp", MREPL) < 0){
		return -1;
	}
	*p0 = open("/mnt/temp/data", ORDWR);
	*p1 = open("/mnt/temp/data1", ORDWR|OCEXEC);

	unmount(nil, "/mnt/temp");
	if(*p0<0 || *p1<0)
		return -1;
	return 0;
}


Filsys*
filsysinit( )  //% filsysinit(Channel *cxfidalloc) 
{DXX(0);
	int n, fd, pid, p0;
	Filsys *fs;
	//%	Channel *c;
	char buf[128];

	fs = emalloc(sizeof(Filsys));
	if(cexecpipe(&fs->cfd, &fs->sfd) < 0)
		goto Rescue;
	fmtinstall('F', fcallfmt);

	clockfd = open("/dev/time", OREAD|OCEXEC);
	fd = open("/dev/user", OREAD);
	strcpy(buf, "Jean-Paul_Belmondo");
	if(fd >= 0){
		n = read(fd, buf, sizeof buf-1);
		if(n > 0)
			buf[n] = 0;
		close(fd);
	}
	fs->user = estrdup(buf);
	//%	fs->cxfidalloc = cxfidalloc;
	pid = getpid();

	/*
	 * Create and post wctl pipe
	 */
	if(cexecpipe(&p0, &wctlfd) < 0)
		goto Rescue;
	sprint(srvwctl, "/srv/riowctl.%s.%d", fs->user, pid);
	post(srvwctl, "wctl", p0);
	close(p0);

	/** Start server processes **/
	//%  c = chancreate(sizeof(char*), 0); //%
	//% if(c == nil)  error("wctl channel");

	wctl_aobj = l_thread_create(wctlthread_, 4096, nil);  //%
	//%   proccreate(wctlproc, c, 4096); //% No use
	//%   threadcreate(wctlthread, c, 4096); //%

	sleep(50);
	filsys_aobj = l_thread_create(filsysproc, 8192, fs); //%
	l_yield(filsys_aobj);
	l_wait_ready(filsys_aobj); //%
	//% proccreate(filsysproc, fs, 10000); //%
sleep(50); //??

	/*
	 * Post srv pipe
	 */
	sprint(srvpipe, "/srv/rio.%s.%d", fs->user, pid);
	post(srvpipe, "wsys", fs->cfd);
DXX(99);
	return fs;

Rescue:
	free(fs);
DXX(-99);
	return nil;
}


/**********************************************************
 *  Filsys active object / single                         *
 *       Data:     Filsys type                            *
 *       Action:   filsysproc()  in fsys.c                *
 **********************************************************/
static
void
filsysproc(void *arg)   //*  arg: self  Filsys structure
{
	int n;
	Xfid *x;
	Fid *f;
	Fcall t;
	uchar *buf;
	Filsys *fs;  //% denotes 'self'
//DXX(0);
	l_thread_setname("FILSYSPROC"); //%
	fs = arg;
	fs->pid = getpid();
	x = nil;
	l_post_ready(0); //%

	for(;;){
		buf = emalloc(messagesize+UTFmax);	
		/* UTFmax for appending partial rune in xfidwrite */
		if (buf == nil){
		   error("\n  Out of memory in filsysproc \n");
		   exits(0);
		}

		n = read9pmsg(fs->sfd, buf, messagesize);
		if(n <= 0){
		        l_yield(nil); //% if threadexitsall'ing, will not return
			fprint(2, "\n<><><> ?? rio[%x]:read9Pmsg fd=%d pid=%d n=%d %r <><><>\n", 
			       L4_Myself(), fs->sfd, getpid(), n);
			errorshouldabort = 0;
			error("eof or I/O error on server channel");
		}

		if(x == nil){
		        //% send(fs->cxfidalloc, nil); //%
			//% recv(fs->cxfidalloc, &x); //%
		        x = allocxfid();
			x->fs = fs;
		}
		x->buf = buf;

		if(convM2S(buf, n, &x->_fcall) != n)  //% x
			error("convert error in convM2S");
		if(DEBUG)
		        fprint(2, "rio:<-%F\n", &x->_fcall); //% Fcall

		if(fcall[x->_fcall.type] == nil) //%
			x = filsysrespond(fs, x, &t, Ebadfcall);
		else{
		       if( x->_fcall.type==Tversion || x->_fcall.type==Tauth) //%
				f = nil;
			else
			        f = newfid(fs, x->_fcall.fid);  //%
			x->f = f;
			x  = (*fcall[ x->_fcall.type])(fs, x, f);  //%
sleep(50);
		}
		firstmessage = 0;
	}
}

/*
 * Called only from a different FD group
 */
int
filsysmount(Filsys *fs, int id)
{
//  DXX(0);
	char buf[32];

	close(fs->sfd);	/* close server end so mount won't hang if exiting */
	sprint(buf, "%d", id);
print("filsysmount:mount(%d, -1, /mnt/wsys, %d, '%s')\n", fs->cfd, MREPL, buf);
	if(mount(fs->cfd, -1, "/mnt/wsys", MREPL, buf) < 0){
		fprint(2, "mount failed: %r\n");
		return -1;
	}
//DXX(10);
	if(bind("/mnt/wsys", "/dev", MBEFORE) < 0){
		fprint(2, "bind failed: %r\n");
		return -1;
	}
//DXX(99);
	return 0;
}

Xfid*
filsysrespond(Filsys *fs, Xfid *x, Fcall *t, char *err)
{
	int n;

	if(err){
		t->type = Rerror;
		t->ename = err;
	}else
	        t->type =  x->_fcall.type+1;  //%
	t->fid = x->_fcall.fid;  //%
	t->tag = x->_fcall.tag;  //%
	if(x->buf == nil){
		x->buf = malloc(messagesize);
		if (x->buf == nil){
		   print("\n  Out of memory in filsysrespond \n");
		   exits(0);
		}
	}
	n = convS2M(t, x->buf, messagesize);
	if(n <= 0)
		error("convert error in convS2M");
	if(write(fs->sfd, x->buf, n) != n)
		error("write error in respond");
	if(DEBUG)
		fprint(2, "rio:->%F\n", t);
	free(x->buf);
	x->buf = nil;
	return x;
}

void
filsyscancel(Xfid *x)
{
	if(x->buf){
		free(x->buf);
		x->buf = nil;
	}
}

static
Xfid*
filsysversion(Filsys *fs, Xfid *x, Fid* _z) //%
{
	Fcall t;

	if(!firstmessage)
		return filsysrespond(x->fs, x, &t, "version request not first message");
	if(x->_fcall.msize < 256)  //%
		return filsysrespond(x->fs, x, &t, "version: message size too small");
	messagesize = x->_fcall.msize;  //%
	t.msize = messagesize;
	if(strncmp(x->_fcall.version, "9P2000", 6) != 0)  //%
		return filsysrespond(x->fs, x, &t, "unrecognized 9P version");
	t.version = "9P2000";
	return filsysrespond(fs, x, &t, nil);
}

static
Xfid*
filsysauth(Filsys *fs, Xfid *x, Fid* _z)  //%
{
	Fcall t;
	return filsysrespond(fs, x, &t, "rio: authentication not required");
}

static
Xfid*
filsysflush(Filsys* _y, Xfid *x, Fid* _z)  //%
{
        L_msgtag  msgtag;
#if TESTCODE
	xfidflush(x);
	return  x;
#else
	l_putarg(nil, Xfunc, "i1", xfidflush);
	msgtag = l_send0((L_thcb*)x, INF, nil);
        //% sendp(x->c, xfidflush);
	if (L4_IpcFailed(msgtag)) IPCERR("silsysflush");
	return nil;
#endif
}

static
Xfid*
filsysattach(Filsys * _z, Xfid *x, Fid *f)  //%
{
	Fcall t;
	L_msgtag  msgtag;

	if(strcmp(x->_fcall.uname, x->fs->user) != 0)  //%
		return filsysrespond(x->fs, x, &t, Eperm);
	f->busy = TRUE;
	f->open = FALSE;
	f->qid.path = Qdir;
	f->qid.type = QTDIR;
	f->qid.vers = 0;
	f->dir = dirtab;
	f->nrpart = 0;
#if TESTCODE
	xfidattach(x);
	return  x;
#else
	l_putarg(nil, Xfunc, "i1", xfidattach);
	msgtag = l_send0((L_thcb*)x, INF, nil);
	//% sendp(x->c, xfidattach); 
	if (L4_IpcFailed(msgtag)) IPCERR("filsysattach");
	return nil;
#endif 
}

static
int
numeric(char *s)
{
	for(; *s!='\0'; s++)
		if(*s<'0' || '9'<*s)
			return 0;
	return 1;
}

static
Xfid*
filsyswalk(Filsys *fs, Xfid *x, Fid *f)
{
	Fcall t;
	Fid *nf;
	int i, id;
	uchar type;
	ulong path;
	Dirtab *d, *dir;
	Window *w;
	char *err;
	Qid qid;
	L_msgtag msgtag; //%

	if(f->open)
		return filsysrespond(fs, x, &t, "walk of open file");
	nf = nil;
	if(x->_fcall.fid  != x->_fcall.newfid){  //%
		/* BUG: check exists */
	  nf = newfid(fs, x->_fcall.newfid);  //%
		if(nf->busy)
			return filsysrespond(fs, x, &t, "clone to busy fid");
		nf->busy = TRUE;
		nf->open = FALSE;
		nf->dir = f->dir;
		nf->qid = f->qid;
		nf->w = f->w;
		incref(&f->w->_ref);  //% f->w
		nf->nrpart = 0;	/* not open, so must be zero */
		f = nf;	/* walk f */
	}

	t.nwqid = 0;
	err = nil;

	/* update f->qid, f->dir only if walk completes */
	qid = f->qid;
	dir = f->dir;

	if(x->_fcall.nwname > 0){  //%
	  for(i=0; i<x->_fcall.nwname; i++){  //%
			if((qid.type & QTDIR) == 0){
				err = Enotdir;
				break;
			}
			if(strcmp(x->_fcall.wname[i], "..") == 0){  //%
				type = QTDIR;
				path = Qdir;
				dir = dirtab;
				if(FILE(qid) == Qwsysdir)
					path = Qwsys;
				id = 0;
    Accept:
				if(i == MAXWELEM){
					err = "name too long";
					break;
				}
				qid.type = type;
				qid.vers = 0;
				qid.path = QID(id, path);
				t.wqid[t.nwqid++] = qid;
				continue;
			}

			if(qid.path == Qwsys){
				/* is it a numeric name? */
			  if(!numeric(x->_fcall.wname[i]))  //%
					break;
				/* yes: it's a directory */
			  id = atoi(x->_fcall.wname[i]);  //%
				qlock(&all);
				w = wlookid(id);
				if(w == nil){
					qunlock(&all);
					break;
				}
				path = Qwsysdir;
				type = QTDIR;
				qunlock(&all);
				incref(&w->_ref); //% w

				l_putarg(nil, 0, "i1", f->w);
				msgtag = l_send0(winclose_aobj, INF, nil);
				//% sendp(winclosechan, f->w);
				if (L4_IpcFailed(msgtag)) IPCERR("filsyswalk");
				f->w = w;
				dir = dirtab;
				goto Accept;
			}
		
			if(snarffd>=0 && strcmp(x->_fcall.wname[i], "snarf")==0) //%
				break;	/* don't serve /dev/snarf if it's provided in the environment */
			id = WIN(f->qid);
			d = dirtab;
			d++;	/* skip '.' */
			for(; d->name; d++)
			        if(strcmp(x->_fcall.wname[i], d->name) == 0){ //%
					path = d->qid;
					type = d->type;
					dir = d;
					goto Accept;
				}

			break;	/* file not found */
		}

		if(i==0 && err==nil)
			err = Eexist;
	}

	if(err!=nil || t.nwqid<x->_fcall.nwname){  //%
		if(nf){
		       if(nf->w){
			   l_putarg(nil, 0, "i1", nf->w);
			   msgtag = l_send0(winclose_aobj, INF, nil);
			   //%  sendp(winclosechan, nf->w); //%
			  if (L4_IpcFailed(msgtag)) IPCERR("filsyswalk");
		       }
			nf->open = FALSE;
			nf->busy = FALSE;
		}
	}else if(t.nwqid == x->_fcall.nwname){  //%
		f->dir = dir;
		f->qid = qid;
	}

	return filsysrespond(fs, x, &t, err);
}

static
Xfid*
filsysopen(Filsys *fs, Xfid *x, Fid *f)
{
	Fcall t;
	int m;
	L_msgtag  msgtag;

	/* can't truncate anything, so just disregard */
	x->_fcall.mode &= ~(OTRUNC|OCEXEC);  //%
	/* can't execute or remove anything */
	if(x->_fcall.mode==OEXEC || (x->_fcall.mode&ORCLOSE)) //%
		goto Deny;
	switch(x->_fcall.mode){  //%
	default:
		goto Deny;
	case OREAD:
		m = 0400;
		break;
	case OWRITE:
		m = 0200;
		break;
	case ORDWR:
		m = 0600;
		break;
	}
	if(((f->dir->perm&~(DMDIR|DMAPPEND))&m) != m)
		goto Deny;
#if TESTCODE
	xfidopen(x);
	return  x;
#else
	l_putarg(nil, Xfunc, "i1", xfidopen);
	msgtag = l_send0((L_thcb*)x, INF, nil);
	//% sendp(x->c, xfidopen);
	if (L4_IpcFailed(msgtag)) IPCERR("filsysopen");
	return nil;
#endif

    Deny:
	return filsysrespond(fs, x, &t, Eperm);
}

static
Xfid*
filsyscreate(Filsys *fs, Xfid *x, Fid* _z)  //%
{
	Fcall t;

	return filsysrespond(fs, x, &t, Eperm);
}

static
int
idcmp(void *a, void *b)
{
	return *(int*)a - *(int*)b;
}

static
Xfid*
filsysread(Filsys *fs, Xfid *x, Fid *f)
{
	Fcall t;
	uchar *b;
	int i, n, o, e, len, j, k, *ids;
	Dirtab *d, dt;
	uint clock;
	char buf[16];
	L_msgtag  msgtag;

	if((f->qid.type & QTDIR) == 0){
	        l_putarg(nil, Xfunc, "i1", xfidread);
		msgtag = l_send0((L_thcb*)x, INF, nil);
	        //% sendp(x->c, xfidread);
		if (L4_IpcFailed(msgtag)) IPCERR("filsysread");
		return nil;
	}
	o = x->_fcall.offset;   //%
	e = x->_fcall.offset+x->_fcall.count;  //%
	clock = getclock();
	b = malloc(messagesize-IOHDRSZ);	/* avoid memset of emalloc */
	if(b == nil)
		return filsysrespond(fs, x, &t, "out of memory");
	n = 0;
	switch(FILE(f->qid)){
	case Qdir:
	case Qwsysdir:
		d = dirtab;
		d++;	/* first entry is '.' */
		for(i=0; d->name!=nil && i<e; i+=len){
		        len = dostat(fs, WIN(x->f->qid), d, b+n, x->_fcall.count-n, clock); //%
			if(len <= BIT16SZ)
				break;
			if(i >= o)
				n += len;
			d++;
		}
		break;
	case Qwsys:
		qlock(&all);
		ids = emalloc(nwindow*sizeof(int));
		for(j=0; j<nwindow; j++)
			ids[j] = window[j]->id;
		qunlock(&all);
		qsort(ids, nwindow, sizeof ids[0], idcmp);
		dt.name = buf;
		for(i=0, j=0; j<nwindow && i<e; i+=len){
			k = ids[j];
			sprint(dt.name, "%d", k);
			dt.qid = QID(k, Qdir);
			dt.type = QTDIR;
			dt.perm = DMDIR|0700;
			len = dostat(fs, k, &dt, b+n, x->_fcall.count-n, clock);  //%
			if(len == 0)
				break;
			if(i >= o)
				n += len;
			j++;
		}
		free(ids);
		break;
	}
	t.data = (char*)b;
	t.count = n;
	filsysrespond(fs, x, &t, nil);
	free(b);
	return x;
}

static
Xfid*
filsyswrite(Filsys* _y, Xfid *x, Fid* _z)  //%
{
        L_msgtag  msgtag;

#if TESTCODE
	xfidwrite(x);
	return  x;
#else		
	l_putarg(nil, Xfunc, "i1", xfidwrite);
	msgtag = l_send0((L_thcb*)x, INF, nil);
        //%  sendp(x->c, xfidwrite); //%
	if (L4_IpcFailed(msgtag)) IPCERR("filsyswrite");
	return nil;
#endif
}

static
Xfid*
filsysclunk(Filsys *fs, Xfid *x, Fid *f)
{
	Fcall t;
	L_msgtag  msgtag;

	if(f->open){
		f->busy = FALSE;
		f->open = FALSE;

#if TESTCODE
		xfidclose(x);
		return  x;
#else		
		l_putarg(nil, Xfunc, "i1", xfidclose);
		msgtag = l_send0((L_thcb*)x, INF, nil);
		//%  sendp(x->c, xfidclose); //%
		if (L4_IpcFailed(msgtag)) IPCERR("filsysclunk");
		return nil;
#endif
	}
	if(f->w){
		l_putarg(nil, 0, "i1", f->w);
		msgtag = l_send0((L_thcb*)winclose_aobj, INF, nil);
	        //%   sendp(winclosechan, f->w); //%
		if (L4_IpcFailed(msgtag)) IPCERR("filsysclunk");
	}
	f->busy = FALSE;
	f->open = FALSE;
	return filsysrespond(fs, x, &t, nil);
}

static
Xfid*
filsysremove(Filsys *fs, Xfid *x, Fid* _z)
{
	Fcall t;

	return filsysrespond(fs, x, &t, Eperm);
}

static
Xfid*
filsysstat(Filsys *fs, Xfid *x, Fid *f)
{
	Fcall t;

	t.stat = emalloc(messagesize-IOHDRSZ);
	t.nstat = dostat(fs, WIN(x->f->qid), f->dir, t.stat, messagesize-IOHDRSZ, getclock());
	x = filsysrespond(fs, x, &t, nil);
	free(t.stat);
	return x;
}

static
Xfid*
filsyswstat(Filsys *fs, Xfid *x, Fid* _z)
{
	Fcall t;

	return filsysrespond(fs, x, &t, Eperm);
}

static
Fid*
newfid(Filsys *fs, int fid)
{
	Fid *f, *ff, **fh;

	ff = nil;
	fh = &fs->fids[fid&(Nhash-1)];
	for(f=*fh; f; f=f->next)
		if(f->fid == fid)
			return f;
		else if(ff==nil && f->busy==FALSE)
			ff = f;
	if(ff){
		ff->fid = fid;
		return ff;
	}
	f = emalloc(sizeof *f);
	f->fid = fid;
	f->next = *fh;
	*fh = f;
	return f;
}

static
uint
getclock(void)
{
	char buf[32];

	seek(clockfd, 0, 0);
	read(clockfd, buf, sizeof buf);
	return atoi(buf);
}

static
int
dostat(Filsys *fs, int id, Dirtab *dir, uchar *buf, int nbuf, uint clock)
{
	Dir d;

	d.qid.path = QID(id, dir->qid);
	if(dir->qid == Qsnarf)
		d.qid.vers = snarfversion;
	else
		d.qid.vers = 0;
	d.qid.type = dir->type;
	d.mode = dir->perm;
	d.length = 0;	/* would be nice to do better */
	d.name = dir->name;
	d.uid = fs->user;
	d.gid = fs->user;
	d.muid = fs->user;
	d.atime = clock;
	d.mtime = clock;
	return convD2M(&d, buf, nbuf);
}
