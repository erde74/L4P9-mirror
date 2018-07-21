//%%%%%%%%%%%% sysfile-l4.c %%%%%%%%%%%%%%%%%%%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

//%begin-----------------------------------------------
#include        <l4all.h>
#include        <l4/l4io.h>
#include        <lp49/lp49.h>

#include  "../errhandler-l4.h"

#define   _DBGFLG  0 
#include  <l4/_dbgpr.h>

/*  USE_MAPPING:  Argument transfer mode: 
 *  1: L4 fpage mapping,  0: process to process copyintg
 */
#define   USE_MAPPING      1   

#define  pprint print
#define  PRN    print
#define  BRK    l4printgetc

extern L4_ThreadId_t  SrvManager; 

extern Proc * tid2pcb(L4_ThreadId_t tid);
extern int proc2proc_copy(unsigned, unsigned, unsigned, unsigned, unsigned);
extern void dbg_time(int which, char *msg) ;
extern void dump_mem(char *title, unsigned char *start, unsigned size);


extern L4_ThreadId_t  client;
extern int core_process_nr;

#define  _BUFSIZE  10240
//%end------------------------------------------------

/*
 * The sys*() routines do not need POPERROR() as they return directly to syscall().
 */

static void unlockfgrp(Fgrp *f)
{
	int ex;

	ex = f->exceed;
	f->exceed = 0;
	unlock(& f->_ref._lock);  //%  <- (f)
	if(ex)
		pprint("warning: process exceeds %d file descriptors\n", ex);
}

int growfd(Fgrp *f, int fd)	/* fd is always >= 0 */
{
	Chan **newfd, **oldfd;

	if(fd < f->nfd)
		return 0;
	if(fd >= f->nfd+DELTAFD)
		return -1;	/* out of range */
	/*
	 * Unbounded allocation is unwise; besides, there are only 16 bits
	 * of fid in 9P
	 */
	if(f->nfd >= 5000){
    Exhausted:
		print("no free file descriptors\n");
		return -1;
	}
	newfd = malloc((f->nfd+DELTAFD)*sizeof(Chan*));
	if(newfd == 0)
		goto Exhausted;
	oldfd = f->fd;
	memmove(newfd, oldfd, f->nfd*sizeof(Chan*));
	f->fd = newfd;
	free(oldfd);
	f->nfd += DELTAFD;
	if(fd > f->maxfd){
		if(fd/100 > f->maxfd/100)
			f->exceed = (fd/100)*100;
		f->maxfd = fd;
	}
	return 1;
}

/*
 *  this assumes that the fgrp is locked
 */
int findfreefd(Fgrp *f, int start)
{
	int fd;

	for(fd=start; fd<f->nfd; fd++)
		if(f->fd[fd] == 0)
			break;
	if(fd >= f->nfd && growfd(f, fd) < 0)
		return -1;
	return fd;
}

int newfd(Chan *c)
{
	int fd;
	Fgrp *f;

	f = up->fgrp;
	lock(& f->_ref._lock);  //% <- (f) 
	fd = findfreefd(f, 0);
	if(fd < 0){
	  unlockfgrp(f);  
		return -1;
	}
	if(fd > f->maxfd)
		f->maxfd = fd;
	f->fd[fd] = c;
	unlockfgrp(f);
	return fd;
}

int newfd2(int fd[2], Chan *c[2])
{
	Fgrp *f;

	f = up->fgrp;
	lock(& f->_ref._lock);  //% <- (f)
	fd[0] = findfreefd(f, 0);
	if(fd[0] < 0){
		unlockfgrp(f);
		return -1;
	}
	fd[1] = findfreefd(f, fd[0]+1);
	if(fd[1] < 0){
		unlockfgrp(f);
		return -1;
	}
	if(fd[1] > f->maxfd)
		f->maxfd = fd[1];
	f->fd[fd[0]] = c[0];
	f->fd[fd[1]] = c[1];
	unlockfgrp(f);

	return 0;
}


Chan* fdtochan(int fd, int mode, int chkmnt, int iref)  //% ONERR NIL
{
	Chan *c;
	Fgrp *f;

DBGPRN(">%s(fd:%d mode:%x chkmnt:%x iref:%x)\n", __FUNCTION__, fd, mode, chkmnt, iref);

	c = 0;
	f = up->fgrp;

	lock(& f->_ref._lock);  //%
	if(fd<0 || f->nfd<=fd || (c = f->fd[fd])==0) {
	        unlock(& f->_ref._lock);  //%
	        ERROR_RETURN(Ebadfd, nil);  //%
	}

	if(iref)
	        incref(& c->_ref);  //% <- (c) 
	unlock(& f->_ref._lock);  //% <- (f)

	if(chkmnt && (c->flag&CMSG)) {
		if(iref)
			cclose(c);
		ERROR_RETURN(Ebadusefd, nil); //%
	}

	if(mode<0 || c->mode==ORDWR) {
	        DBGPRN("< %s()1 -> %s\n", __FUNCTION__, c->path->s);
		return c;
	}

	if((mode&OTRUNC) && c->mode==OREAD) {
		if(iref)
			cclose(c);
		ERROR_RETURN(Ebadusefd, nil);  //%
	}

	if((mode&~OTRUNC) != c->mode) {
		if(iref)
			cclose(c);
		PRN("fdtochan{mode=%x c->mode=%x}  ", mode, c->mode); //%%
		ERROR_RETURN(Ebadusefd, nil);  //% devramfs-create?
	}
DBGPRN("< %s()2 -> %s\n", __FUNCTION__, c->path->s);
	return c;
}

int openmode(ulong o)  //% ONERR
{
	o &= ~(OTRUNC|OCEXEC|ORCLOSE);
	if(o > OEXEC)
	        ERROR_RETURN(Ebadarg, ONERR); //%
	if(o == OEXEC)
		return OREAD;
	return o;
}


//%   int fd2path(int fd, char *buf, int nbuf);
long sysfd2path(ulong *arg, Clerkjob_t *myjob) //% ONERR  
{
	Chan *c;
	char  *cp;
	L4_ThreadId_t  client = myjob->client; //%
	uint   nbuf = arg[2];  //% 

#if USE_MAPPING //---------------------------------
	uint  adrs = (arg[1]-L4_Address(myjob->mapper)) + L4_Address(myjob->mappee); //%
#else //----------------------------------------
	uint   adrs = arg[1];  //%
#endif //-------------------------------------

	DBGPRN(">sysfd2path(%d,%x,%d) ", arg[0], arg[1], arg[2]);

	//%  validaddr(arg[1], arg[2], 1);

	c = fdtochan(arg[0], -1, 0, 1);  // pr_chan(c);//%

DBGPRN("sysfd2path(%d):'%s' ", arg[0], c->path->s);
	IF_ERR_RETURN(c, nil, ONERR);

#if USE_MAPPING //%----------------
	snprint((char*)adrs, nbuf, "%s", chanpath(c));  //% arg[1]
#else //original----------
	char  *_buf = malloc(nbuf);
	//snprint(_buf, nbuf, "%s", cp = chanpath(c));
	strncpy(_buf, chanpath(c), nbuf); _buf[nbuf-1] = 0;
	proc2proc_copy(core_process_nr, (unsigned)_buf, TID2PROCNR(client), adrs, nbuf);
	free(_buf);
#endif //-----------------

	cclose(c);
	DBGPRN("<%s(): 0\n", __FUNCTION__);
	return 0;
}


//%  int pipe(int fd[2]);
//%  result (fd[0,1]) is returned via myjob->args[0, 1]

long syspipe(ulong *arg, Clerkjob_t *myjob)  //ONERR
{
	int fd[2];
	Chan *c[2];
	Dev *d;
	static char *datastr[] = {"data", "data1"};
	L4_ThreadId_t  client = myjob->client; //%
	DBGPRN(">%s() ", __FUNCTION__);

	//%  validaddr(arg[0], 2*BY2WD, 1);
	//%	evenaddr(arg[0]);

	d = devtab[devno('|', 0)];

	c[0] = namec("#|", Atodir, 0, 0);
	c[1] = 0;
	fd[0] = -1;
	fd[1] = -1;

	if(WASERROR()){
	_ERR_1:
		cclose(c[0]);
		if(c[1])
			cclose(c[1]);
		NEXTERROR_RETURN(ONERR);  //%
	}

	c[1] = cclone(c[0]);
	if(walk(&c[0], datastr+0, 1, 1, nil) < 0)
	        ERROR_GOTO(Egreg, _ERR_1);   //%

	if(walk(&c[1], datastr+1, 1, 1, nil) < 0)
	        ERROR_GOTO(Egreg, _ERR_1);  //%

	c[0] = d->open(c[0], ORDWR);
	c[1] = d->open(c[1], ORDWR);

	if(newfd2(fd, c) < 0)
	        ERROR_GOTO(Enofd, _ERR_1);  //%

	POPERROR();

#if 1 //Result is returned via myjob->args[0,1] -----------------
	myjob->args[0] = fd[0];
	myjob->args[1] = fd[1];
	DBGBRK("syspipe;<%d %d>\n", myjob->args[0], myjob->args[1]);
#else //--------------------------------------------------------
    #if 1 //%------------------------
	proc2proc_copy(core_process_nr, (unsigned)fd, TID2PROCNR(client), (unsigned)arg[0], 8);
    #else //original
	((long*)arg[0])[0] = fd[0];
	((long*)arg[0])[1] = fd[1];
    #endif //%---------------------
#endif //------------------------------------------------------
	DBGPRN("<%s():0 ", __FUNCTION__);
	return 0;
}


//%  int dup(int oldfd, int newfd);
long sysdup(ulong *arg, Clerkjob_t *myjob)    //% ONERR 
{
	int fd;
	Chan *c, *oc;
	Fgrp *f = up->fgrp;
	DBGPRN("> %s \n", __FUNCTION__);

	/*
	 * Close after dup'ing, so date > #d/1 works
	 */
	c = fdtochan(arg[0], -1, 0, 1);
	IF_ERR_RETURN(c, nil, ONERR); //%

	fd = arg[1];
	if(fd != -1){
	        lock(& f->_ref._lock);  //%
		if(fd<0 || growfd(f, fd)<0) {
			unlockfgrp(f);
			cclose(c);
			ERROR_RETURN(Ebadfd, ONERR);  //%
		}
		if(fd > f->maxfd)
			f->maxfd = fd;

		oc = f->fd[fd];
		f->fd[fd] = c;
		unlockfgrp(f);
		if(oc)
			cclose(oc);
	}else{
		if(WASERROR()) {
		_ERR_1:
			cclose(c);
			NEXTERROR_RETURN(ONERR);  //%
		}
		fd = newfd(c);
		if(fd < 0)
		        ERROR_GOTO(Enofd, _ERR_1); //%

		POPERROR();
	}

	DBGPRN("< %s(): %d\n", __FUNCTION__, fd);
	return fd;
}


//%  int open(char *name, int omode);
//%  omode : {OREAD, OWRITE, ORDWR, OEXEC} | OTRUNC, OCEXEC, ORCLOSE
long sysopen(ulong *arg, Clerkjob_t *myjob)  //%  ONERR
{
	int fd;
	Chan *c = 0;
	int  rc; //%
	DBGPRN("> %s(\"%s\", %x) \n", __FUNCTION__, arg[0], arg[1]); 

	//	dbg_time(1, 0); //%
#if 1 //%----------------------------
	rc = openmode(arg[1]);	/* error check only */
	IF_ERR_RETURN(rc, ONERR, ONERR); //%
#else //original
	openmode(arg[1]);	/* error check only */
#endif //%----------------------------

	if(WASERROR()){
	_ERR_1:
		if(c)
			cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}
	//%  validaddr(arg[0], 1, 0);
	c = namec((char*)arg[0], Aopen, arg[1], 0);
	IF_ERR_GOTO(c, nil, _ERR_1);  //%

	fd = newfd(c);
	if(fd < 0)
	        ERROR_GOTO(Enofd, _ERR_1);  //%

	POPERROR();
	DBGPRN("< %s(\"%s\", %x)->%d \n", __FUNCTION__, arg[0], arg[1], fd);
	//	dbg_time(1, "sysfile:open"); //%
	return fd;
}


void fdclose(int fd, int flag)
{
	int i;
	Chan *c;
	//DBGPRN("> %s(%d, %x): %d\n", __FUNCTION__, fd, flag);
	Fgrp *f = up->fgrp;

	lock(& f->_ref._lock);  //%
	c = f->fd[fd];
	if(c == 0){
		/* can happen for users with shared fd tables */
	  unlock(& f->_ref._lock);  //% <- (f);
		return;
	}
	if(flag){
		if(c==0 || !(c->flag&flag)){
		  unlock(& f->_ref._lock); //%  (f);
			return;
		}
	}
	f->fd[fd] = 0;
	if(fd == f->maxfd)
		for(i=fd; --i>=0 && f->fd[i]==0; )
			f->maxfd = i;

	unlock(& f->_ref._lock); //%  (f);
	cclose(c);
}

//%  int close(int fd);
long sysclose(ulong *arg, Clerkjob_t *myjob) //% ONERR
{
        Chan *rc;  //%
        //  PRN("> %s(%d)  ", __FUNCTION__, arg[0]);
	rc = fdtochan(arg[0], -1, 0, 0); //%
	if (rc==nil) PRN("Err:fdtochan()==nil  \n"); //%
	IF_ERR_RETURN(rc, nil, ONERR); //%

	fdclose(arg[0], 0);
        //  PRN("< %s(%d)->%d \n", __FUNCTION__, arg[0] ,0);
	return 0;
}


long unionread(Chan *c, void *va, long n)
{
	int i;
	long nr;
	Mhead *m;
	Mount *mount;
	unsigned char c1 = 0; //%
	DBGPRN(">%s(c:%s %x %d)\n", __FUNCTION__, c->path->s, va, n); 

	qlock(&c->umqlock);
	m = c->umh;
	rlock(&m->lock);
	mount = m->mount;
	/* bring mount in sync with c->uri and c->umc */
	for(i = 0; mount != nil && i < c->uri; i++)
		mount = mount->next;

	nr = 0;
	while(mount != nil){
		/* Error causes component of union to be skipped */

		if(mount->to && !WASERROR()){
			if(c->umc == nil){
				c->umc = cclone(mount->to);
				c->umc = devtab[c->umc->type]->open(c->umc, OREAD);
				IF_ERR_GOTO(c->umc, nil, _ERR_1); //%
			}
	
			nr = devtab[c->umc->type]->read(c->umc, va, n, c->umc->offset);
			IF_ERR_GOTO(nr, ONERR, _ERR_1); //%
			IF_ERR_GOTO(nr, -1, _ERR_1);    //%

			//% dump_mem("unionread", va, nr); //%
			//% l4printgetc("chan=%s nr=%d offset=%d \n", 
			//%   c->umc->path->s, nr,  c->umc->offset); //%

			c->umc->offset += nr;
			POPERROR();
		}

	_ERR_1:
		if(nr > 0)
			break;

		/* Advance to next element */
		c->uri++;
		if(c->umc){
			cclose(c->umc);
			c->umc = nil;
		}
		mount = mount->next;
	}
	runlock(&m->lock);
	qunlock(&c->umqlock);

	//%  dump_mem("<unionread", va, nr); //%
	//%  L4_KDB_Enter("");
	return nr;
}

static void unionrewind(Chan *c)
{
	qlock(&c->umqlock);
	c->uri = 0;
	if(c->umc){
		cclose(c->umc);
		c->umc = nil;
	}
	qunlock(&c->umqlock);
}



/*
 *   struct Dir {
 *    ushort  type;  
 *    uint    dev;   
 *    Qid     qid;   
 *    ulong   mode, atime, mtime;
 *    vlong   length; 
 *    char    *name, *uid, *gid, *muid;
 *  } Dir;
 *
 * p->|            dir-all                           e->|
 *    +--+------------------+--+-----+--+----+--+-------+
 *    |sz|   dir-fixed      |sz|name |sz|uid |sz| ---   |
 *    +--+------------------+--+-----+--+----+--+-------+
 *    <------------ len = total size-------------------->
 */

/* dirfixed() returns: total length of dir-all
 *  p :  the start address of dir-all. 
 *  e :  limit
 *  d :  into *d, Dir is output
 *  returns: len (total size)
 */

static int dirfixed(uchar *p, uchar *e, Dir *d)
{
	int len;

	len = GBIT16(p)+BIT16SZ;
	if(p + len > e)
		return -1;

	p += BIT16SZ;	/* ignore size */
	d->type = devno(GBIT16(p), 1);
	p += BIT16SZ;
	d->dev = GBIT32(p);
	p += BIT32SZ;
	d->qid.type = GBIT8(p);
	p += BIT8SZ;
	d->qid.vers = GBIT32(p);
	p += BIT32SZ;
	d->qid.path = GBIT64(p);
	p += BIT64SZ;
	d->mode = GBIT32(p);
	p += BIT32SZ;
	d->atime = GBIT32(p);
	p += BIT32SZ;
	d->mtime = GBIT32(p);
	p += BIT32SZ;
	d->length = GBIT64(p);

	return len;
}


/*  dirname(): returns the NAME.
 *  p:  start addr of dir-serialized.
 *  *n: put the length of NAME  
 */
static char* dirname(uchar *p, int *n)  
{
        //DBGPRN("> %s(%x %x) \n", __FUNCTION__, p, n);
	p += BIT16SZ+BIT16SZ+BIT32SZ+BIT8SZ+BIT32SZ+BIT64SZ
		+ BIT32SZ+BIT32SZ+BIT32SZ+BIT64SZ;

	*n = GBIT16(p); 
	return (char*)p+BIT16SZ;
}

/*    p: input                 Outout val
 *    :                        :           
 *    +--+------------------+--+-----+--+----+--+-------+
 *    |sz|   dir-fixed      |sz|name |sz|uid |sz| ---   |
 *    +--+------------------+--+-----+--+----+--+-------+
 *                             <-*n-> 
 */



/* dirsetname() returns: --- length
 *  name: NAME to be put
 *  len : length of NAME
 *  p: start addr of dir-serialized.
 *  n: length of dir-serialized.
 */

static long dirsetname(char *name, int len, uchar *p, long n, long maxn)
{
	char *oname;
	int olen;
	long nn;

	DBGPRN(">%s(\"%s\" %x %d %x %d %d) \n",
		   __FUNCTION__,  name, len, p, n, maxn);

	if(n == BIT16SZ)
		return BIT16SZ;

	oname = dirname(p, &olen);

	nn = n+len-olen;
	PBIT16(p, nn-BIT16SZ);
	if(nn > maxn)
		return BIT16SZ;
	
	//DBGBRK("# %x %x %d \n", oname+len, oname+olen, p+n-(uchar*)(oname+olen)); 
	  
	if(len != olen)
		memmove(oname+len, oname+olen, p+n-(uchar*)(oname+olen));

	PBIT16((uchar*)(oname-2), len);
	memmove(oname, name, len);

	return nn;
}


/* <Input> 
 *    p: input                 oname  oname+olen         p+n          max
 *    :                        :      :                  :             :
 *    +--+------------------+--+------+--+----+--+-------+         (Buffer limit)
 *    |sz|   dir-fixed      |sz|oname |sz|uid |sz| ---   |
 *    +--+------------------+--+------+--+----+--+-------+
 *                             <-olen-> 
 *
 *
 * <Output>
 *    p: input                 Outout val
 *    :                        :           
 *    +--+------------------+--+--------+--+----+--+-------+
 *    |sz|   dir-fixed      |sz|  name  |sz|uid |sz| ---   |
 *    +--+------------------+--+--------+--+----+--+-------+
 *                             <-len---> 
 *    |<------- nn = return value ------------------------>|
 */ 



/*
 * Mountfix might have caused the fixed results of the directory read
 * to overflow the buffer.  Catch the overflow in c->dirrock.
 */

static void mountrock(Chan *c, uchar *p, uchar **pe)
{
	uchar *e, *r;
	int len, n;
	DBGPRN(">%s(%s %s)  ", c->path->s, &p[41+2]);

	e = *pe;

	/* find last directory entry */
	for(;;){
		len = BIT16SZ+GBIT16(p);
		if(p+len >= e)
			break;
		p += len;
	}

	/* save it away */
	qlock(&c->rockqlock);
	if(c->nrock+len > c->mrock){
		n = ROUND(c->nrock+len, 1024);
		r = smalloc(n);
		memmove(r, c->dirrock, c->nrock);
		free(c->dirrock);
		c->dirrock = r;
		c->mrock = n;
	}

	memmove(c->dirrock+c->nrock, p, len);
	c->nrock += len;
	qunlock(&c->rockqlock);

	/* drop it */
	*pe = p;
}

/*
 *                                       *pe <- - - -    *pe 
 *    p                              --> p                e 
 *    :                                  :<--- len ------>: 
 *    +----------------+-----------------+----------------+
 *    |  1st dir-all   |  ...            | last dir-all   |
 *    +----------------+-----------------+----------------+
 *                                               |
 * c->+-Chan---+                                 |
 *    |        |                                 |
 *    |dirrock-|------>+---------+ <- r          |
 *    | nrock  |       | (nrock) |               |
 *    | mrock  |       |         |               |
 *    |        |       | - - - - |               |
 *    +--------+       |  len    | <-------------+
 *                     |- - - - -|
 *                     +---------+
 *
 */



/*
 * Satisfy a directory read with the results saved in c->dirrock.
 *
 *  mountrockread() returns:
 *   c:
 *   op, n: addr and length of client buffer (output) 
 */

static int mountrockread(Chan *c, uchar *op, long n, long *nn)
{
	long dirlen;
	uchar *rp, *erp, *ep, *p;
	DBGPRN(">%s(c:%s %x %d %x) ", __FUNCTION__, c->path->s, op, n, nn);

	/* common case */
	if(c->nrock == 0){   DBGPRN("<%s():0 \n", __FUNCTION__);
		return 0;
	}

	/* copy out what we can */
	qlock(&c->rockqlock);
	rp = c->dirrock;
	erp = rp + c->nrock;
	p = op;
	ep = p+n;

	while(rp+BIT16SZ <= erp){
		dirlen = BIT16SZ+GBIT16(rp);
		if(p+dirlen > ep)
			break;

		memmove(p, rp, dirlen);
		p += dirlen;
		rp += dirlen;
	}

	if(p == op){
		qunlock(&c->rockqlock);  DBGPRN("<%s():0 \n", __FUNCTION__);
		return 0;
	}

	/* shift the rest */
	if(rp != erp)
		memmove(c->dirrock, rp, erp-rp);
	c->nrock = erp - rp;

	*nn = p - op;
	qunlock(&c->rockqlock); 
	DBGPRN("<%s():1 \n", __FUNCTION__);
	return 1;
}

static void mountrewind(Chan *c)
{
	c->nrock = 0;
}


static void NM(char *hd, char *p)
{
    char  nm[20] = {[0 ... 19] = 0};
    char    *cp;
    int     len;
    cp = dirname(p, &len);
    strncpy(nm, cp, len);
    nm[len] = 0;
    DBGPRN("%s:'%s'  ", hd, nm);
}

/*
 * Rewrite the results of a directory read to reflect current 
 * name space bindings and mounts.  Specifically, replace
 * directory entries for bind and mount points with the results
 * of statting what is mounted there.  Except leave the old names.
 *
 *  mountfix() returns:
 *   *c: directory channel
 *   *op: n = uniornread(c, op, len); multiple dir-all entries.
 *   n: size returned by unionread()
 *   maxn: sizo of buffer
 */
static long mountfix(Chan *c, uchar *op, long n, long maxn) //% ONERR
{
	char *name;
	int nbuf, nname;
	Chan *nc;
	Mhead *mh;
	Mount *m;
	uchar *p;
	int dirlen, rest;
	long l;
	uchar *buf, *e;
	Dir d;
	DBGPRN(">%s(%s %s %d %d);  ", __FUNCTION__, 
	    c->path->s, &op[41+2], n, maxn); //%
	
	p = op;
	buf = nil;
	nbuf = 0;

/*
 *    p<1>=op          p<2>                               e
 *    :                                                   :
 *    +----------------+-----------------+----------------+
 *    |  1st dir-all   |  ...            | last dir-all   |
 *    +----------------+-----------------+----------------+
 *     <- dirlen1 ----> <- dirlen2 -->    <- dirlen-n --->
 *
 */
	//%$  dump_mem("mountfix", op, 64); //%
	for(e=&p[n]; p+BIT16SZ<e; p+=dirlen){
		dirlen = dirfixed(p, e, &d);

		if(dirlen < 0)
			break;

	        //% NM("NM", p); //%
		nc = nil;
		mh = nil;

		if(findmount(&nc, &mh, d.type, d.dev, d.qid)){
		        // DBGPRN("findmount()%s ", nc->path->s);

			/*
			 * If it's a union directory and the original is
			 * in the union, don't rewrite anything.
			 */
			for(m=mh->mount; m; m=m->next)
				if(eqchantdqid(m->to, d.type, d.dev, d.qid, 1))
					goto Norewrite;

			name = dirname(p, &nname);

			/*    p               name
			 *    +--+---------+--+-----+------+
			 *    |sz|         |sz|name |      |
			 *    +--+---------+--+-----+------+
			 */

			/*
			 * Do the stat but fix the name.  If it fails, leave old entry.
			 * BUG: If it fails because there isn't room for the entry,
			 * what can we do?  Nothing, really.  Might as well skip it.
			 */
			if(buf == nil){
				buf = smalloc(4096);
				nbuf = 4096;
			}

			if(WASERROR()) {
			_ERR_1:
				goto Norewrite;
			}

			l = devtab[nc->type]->stat(nc, buf, nbuf);

			if (l == ONERR || l == -1) PRN("MNTFIX;%d\n", l);
			IF_ERR_GOTO(l, ONERR, _ERR_1); //%
			IF_ERR_GOTO(l, -1, _ERR_1); //%

			//%  dump_mem("stat", buf, l);  //%
			//%  NM("NM2", buf); //%

			l = dirsetname(name, nname, buf, l, nbuf);
			if(l == BIT16SZ)
			        ERROR_GOTO("dirsetname", _ERR_1); //%

			/*    p               name
			 *    +--+---------+--+-------+------+
			 *    |sz|         |sz|newname|      |
			 *    +--+---------+--+-------+------+
			 *    :<----------- l -------------->
			 *    buf
			 */

			POPERROR();

			/*
			 * Shift data in buffer to accomodate new entry,
			 * possibly overflowing into rock.
			 */
			rest = e - (p+dirlen);

			if(l > dirlen){
				while(p+l+rest > op+maxn){
					mountrock(c, p, &e);
					PRN("MNTROCK(%s)  ", c->path->s);

					if(e == p){
						dirlen = 0;
						goto Norewrite;
					}

					rest = e - (p+dirlen);
				}
			}

/*
 *    p                                                 e
 *    :                                                 :
 *    +----------------+---------------+----------------+
 *    |  1st dir-all   |  ...          | last dir-all   |
 *    +----------------+---------------+----------------+
 *     <- dirlen  ----> <- dirlen  ->   <- dirlen-n --->
 *
 */

			if(l != dirlen){
				memmove(p+l, p+dirlen, rest);
				dirlen = l;
				e = p+dirlen+rest;
			}

			/*
			 * Rewrite directory entry.
			 */
			memmove(p, buf, l);

		    Norewrite:
			cclose(nc);
			putmhead(mh);
		}
	}

	if(buf)
		free(buf);

	if(p != e) {
 	        PRN("mountfixX{%d}; ",  e - p);  //%
		dump_mem("", p, e-p); //%
	        //% ERROR_RETURN("oops in rockfix", ONERR); //%
	}

	DBGPRN("<mountfix():%d \n", e-op);
	return e-op;
}


static long read(ulong *arg, vlong *offp, Clerkjob_t *myjob)  //% ONERR
{
	int dir;
	long n, nn, nnn;
	uchar *p;
	Chan *c;
	vlong off;
	L4_ThreadId_t  client = myjob->client; //%

check_clock('s', ">read"); //%
#if USE_MAPPING //---------------------------
	uint  adrs = (arg[1]-L4_Address(myjob->mapper)) + L4_Address(myjob->mappee); //%
#else  //-------------------------------
	uint  adrs = arg[1];
#endif //--------------------------------

	//	dbg_time(1, 0); //%
	n = arg[2];
	//%  validaddr(arg[1], n, 1);
	p = (void*)adrs;   //% arg[1]
	c = fdtochan(arg[0], OREAD, 1, 1);

	if (c==nil){
	    l4printf_c(10, "sysfile:read:Er1"); //%
	    _backtrace_();
	}
	IF_ERR_RETURN(c, nil, ONERR); //%

	if(WASERROR()){
	_ERR_1:
		cclose(c);
		l4printf_c(10, "sysfile:read():Er2"); //%
		NEXTERROR_RETURN(ONERR);  //%
	}

	/*
	 * The offset is passed through on directories, normally.
	 * Sysseek complains, but pread is used by servers like exportfs,
	 * that shouldn't need to worry about this issue.
	 *
	 * Notice that c->devoffset is the offset that c's dev is seeing.
	 * The number of bytes read on this fd (c->offset) may be different
	 * due to rewritings in rockfix.
	 */

	if(offp == nil)	/* use and maintain channel's offset */
		off = c->offset;
	else
		off = *offp;

	if(off < 0)
	        ERROR_GOTO(Enegoff, _ERR_1);  //%

	if(off == 0){	/* rewind to the beginning of the directory */
		if(offp == nil){
			c->offset = 0;
			c->devoffset = 0;
		}
		mountrewind(c);
		unionrewind(c);
	}

#if USE_MAPPING  //%-----------------------------------------------
	dir = c->qid.type&QTDIR;
	if(dir && mountrockread(c, p, n, &nn)){
		/* do nothing: mountrockread filled buffer */
	}else{
		if(dir && c->umh)
			nn = unionread(c, p, n);
		else {
			nn = devtab[c->type]->read(c, p, n, off);
		}
	}
	if(dir)
		nnn = mountfix(c, p, nn, n);
	else
		nnn = nn;

	// if (nnn >= -1)PRN("{%s} ", p); //%%%% 
#else //%-------------------------------------------------
	dir = c->qid.type & QTDIR;

	if (n < _BUFSIZE)
	{
	    char  *_buf = malloc(n);

	    if(dir && mountrockread(c, _buf, n, &nn)){
		/* do nothing: mountrockread filled buffer */
	    }
	    else{
	        if(dir && c->umh){
		    nn = unionread(c, _buf, n);  //% p
		    //% dump_mem("read:unionread", _buf, 64);  //%
		}
		else {
		    nn = devtab[c->type]->read(c, _buf, n, off); //% p
		    //% dump_mem("devtab->read", _buf, 64);  //%
		}
		if (nn == ONERR) free(_buf);    //%
		IF_ERR_GOTO(nn, ONERR, _ERR_1); //%
	    }


	    if(dir) {
	        nnn = mountfix(c, _buf, nn, n);  //% p
		if (nnn == ONERR) free(_buf);    //%
		IF_ERR_GOTO(nnn, ONERR, _ERR_1);  //%
	    }
	    else
		nnn = nn;

	    proc2proc_copy(core_process_nr, (ulong)_buf, TID2PROCNR(client), (ulong)p, nnn);

	    if (_DBGFLG) dump_mem("read:", _buf, (n <= 16)? n : 16); //% 
	    DBGPRN("# read [%d] from \"%s\" <%d %d> bytes \n", 
		   c->type, devtab[c->type]->name, nn, nnn);

	    free(_buf);
	}
	else {
	    DBGBRK("sysfile:read (%d > _BUFSIZE) \n", n);
	    nnn = 0; 
	}
#endif //%-----------------------------------------------------------

	lock(& c->_ref._lock);  //% (c)
	c->devoffset += nn;
	c->offset += nnn;
	unlock(& c->_ref._lock);  //% (c);

	POPERROR();
	cclose(c);

	if(nnn<0) l4printf_c(10, "read:Er3"); //%

	//	dbg_time(1, "sysfile:read"); //%
	//if(nnn==0)print("\tsysfile:read(%s):%d\n", c->path->s, nnn); //% DBG
check_clock('s', "<read"); //%
	return nnn;
}


long sys_read(ulong *arg, Clerkjob_t *myjob)  //%  ONERR
{
      	DBGPRN("> %s \n", __FUNCTION__);
	return read(arg, nil, myjob);   //% myjob
}


long syspread(ulong *arg, Clerkjob_t *myjob)  //% myjob  ONERR
{
	vlong v;
	//	va_list list;

#if 1  //%-----------------------------
	DBGPRN("> %s(%d %x %x %x-%x)\n", 
	       __FUNCTION__, arg[0], arg[1], arg[2], arg[3], arg[4]);

	union {
	  vlong   v;
	  ulong   u[2];
	} uv;

	uv.u[0] = arg[3];
	uv.u[1] = arg[4];
	v = uv.v;
#else //original-------------------------
	/* use varargs to guarantee alignment of vlong */
	va_start(list, arg[2]);
	v = va_arg(list, vlong);
	va_end(list);
#endif //%--------------------------------

	DBGPRN("> %s \n", __FUNCTION__);
	if(v == ~0ULL)
	        return read(arg, nil, myjob);   //%

	return read(arg, &v, myjob);   //%
}


static long write(ulong *arg, vlong *offp, Clerkjob_t *myjob) //% myjob  ONERR
{
	Chan *c;
	long m, n;
	vlong off;
	L4_ThreadId_t  client = myjob->client; //%
#if USE_MAPPING //----------------------------------
	uint  adrs = (arg[1] - L4_Address(myjob->mapper)) + L4_Address(myjob->mappee);  //%
#else //-------------------------------------
	uint  adrs = arg[1];  //%
#endif //------------------------------


	// DBGPRN("> %s(%d \"%s\" %d %x)\n", __FUNCTION__, arg[0], arg[1], arg[2], offp);

	//	dbg_time(1, 0); //% 
	//%  validaddr(arg[1], arg[2], 0);
	n = 0;
	c = fdtochan(arg[0], OWRITE, 1, 1);   // pr_chan(c); //%
	IF_ERR_RETURN(c, nil, ONERR);  //%

	if(WASERROR()) {
	_ERR_1:
		if(offp == nil){
		        lock(& c->_ref._lock);  //% (c)
			c->offset -= n;
			unlock(& c->_ref._lock);  //%  (c);
		}
		cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}

	if(c->qid.type & QTDIR)
	        ERROR_GOTO(Eisdir, _ERR_1); //%

	n = arg[2];

	if(offp == nil){	/* use and maintain channel's offset */
	        lock(& c->_ref._lock);  //%  (c)
		off = c->offset;
		c->offset += n;
		unlock(& c->_ref._lock);  //%  (c)
	}else
		off = *offp;

	if(off < 0)
	        ERROR_GOTO(Enegoff, _ERR_1);  //%

	// pr_chan(c);  //%

#if USE_MAPPING //%------------------------------------------
	if(n>0x2000){                                  //% DBG
	    L4_Fpage_t fpage1 = myjob->mapper;       //%
	    L4_Fpage_t fpage2 = myjob->mappee;       //%
	    // print("wrt(%s, %x, %x, %x)", c->path->s, adrs, n, off); //%
	    // print("<%x %x>", L4_Address(fpage1), L4_Size(fpage1));
	    // print("<%x %x>", L4_Address(fpage2), L4_Size(fpage2)); 

	    //    adrs = adrs - 0x1000;//????

	    // dbg_dump_mem("sys", adrs, 32);          //%
	    // L4_KDB_Enter("sys-write");              //%
	}
	m = devtab[c->type]->write(c, (void*)adrs, n, off);   //% arg[1]
	IF_ERR_GOTO(m, ONERR, _ERR_1);  //%
#else //%-----------------------------------------------------
	if (n <= _BUFSIZE) {
	  	char  *_buf = malloc(n);

	        proc2proc_copy(TID2PROCNR(client), (ulong)adrs,
			       core_process_nr, (ulong)_buf, n);  

		m = devtab[c->type]->write(c, (void*)_buf, n, off);
		if (m == ONERR) free(_buf);     //%
		IF_ERR_GOTO(m, ONERR, _ERR_1);  //%

		DBGPRN("sysfile:write(\"%s\" \"%s\" n=%d off=%Ld)\n",  
		       devtab[c->type]->name, _buf, n, off); //%%%
		free(_buf);
	}
	else {
	  m = -1;
	  DBGBRK("sysfile:write %d > _BUFSIZE \n", n);
	}
#endif //%-----------------------------------------------

	if(offp == nil && m < n){
	  lock(& c->_ref._lock);  //% (c);
		c->offset -= n - m;
		unlock(& c->_ref._lock);  //% (c)
	}

	POPERROR();
	cclose(c);

	DBGPRN("< %s(%x %x %x %x)->%d \n", 
	       __FUNCTION__, arg[0], arg[1], arg[2], (ulong)offp, m);

	//	dbg_time(1, "sysfile:write");
	return m;
}


long sys_write(ulong *arg, Clerkjob_t *myjob)  //% myjob  ONERR
{
      	DBGPRN("> %s(%d %x %d) \n", __FUNCTION__, arg[0], arg[1], arg[2]);
	return write(arg, nil, myjob);  //% myjob
}


long syspwrite(ulong *arg, Clerkjob_t *myjob)  //% myjob  ONERR
{
	vlong v;
	//	va_list list;

#if 1  //%----------------------------
	DBGPRN("> %s(%d %x %x %x-%x)\n", 
	       __FUNCTION__, arg[0], arg[1], arg[2], arg[3], arg[4]);

	union {
	  vlong   v;
	  ulong   u[2];
	} uv;
	uv.u[0] = arg[3];
	uv.u[1] = arg[4];
	v = uv.v;
#else //original-----------------------
	DBGPRN("> %s \n", __FUNCTION__);
	/* use varargs to guarantee alignment of vlong */
	va_start(list, arg[2]);
	v = va_arg(list, vlong);
	va_end(list);
#endif  //%-------------------------------

	if(v == ~0ULL) 
	      return write(arg, nil, myjob);   //%

	return write(arg, &v, myjob);  //%
}

#if 1 //% 061211--------------------------------------
static long sseek_l4(ulong *arg, vlong *seekpos, Clerkjob_t *myjob) //% ONERR <- void
{
	Chan *c;
	uchar buf[sizeof(Dir)+100];
	Dir dir;
	int n;
	vlong off;
	union {
		vlong v;
		ulong u[2];
	} o;

       DBGPRN(">%s(%d %d %d %d %x) \n", 
	      __FUNCTION__, arg[0], arg[1], arg[2], arg[3], seekpos);

       c = fdtochan(arg[0], -1, 1, 1);  //% arg[1]
       IF_ERR_RETURN(c, nil, ONERR);  //%
       

	if(WASERROR()){
	_ERR_1:
		cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}
	if(devtab[c->type]->dc == '|')
	        ERROR_GOTO(Eisstream, _ERR_1);  //%

	off = 0;
	o.u[0] = arg[1];  //% arg[2]
	o.u[1] = arg[2];  //% arg[3]
	switch(arg[3]){   //% arg[4]
	case 0:
		off = o.v;
		if((c->qid.type & QTDIR) && off != 0)
		        ERROR_GOTO(Eisdir, _ERR_1);  //%

		if(off < 0)
		        ERROR_GOTO(Enegoff, _ERR_1);  //%

		c->offset = off;  // DBG_CH(c, "##"); 
		break;

	case 1:
		if(c->qid.type & QTDIR)
		        ERROR_GOTO(Eisdir, _ERR_1);  //%

		lock(& c->_ref._lock);	//% /* lock for read/write update */
		off = o.v + c->offset;
		if(off < 0){
		  unlock(& c->_ref._lock);  //% (c);
		  ERROR_GOTO(Enegoff, _ERR_1);  //%
		}
		c->offset = off;
		unlock(& c->_ref._lock);  //%  (c);
		break;

	case 2:
		if(c->qid.type & QTDIR)
		        ERROR_GOTO(Eisdir, _ERR_1);  //%
		n = devtab[c->type]->stat(c, buf, sizeof buf);

		if(convM2D(buf, n, &dir, nil) == 0)
		        ERROR_GOTO("internal error: stat error in seek", _ERR_1); //%
		off = dir.length + o.v;
		if(off < 0)
		        ERROR_GOTO(Enegoff, _ERR_1); //%
		c->offset = off;
		break;

	default:
	        ERROR_GOTO(Ebadarg, _ERR_1); //%
	}
	*seekpos = off;  //%  *(vlong*)arg[0] = off;
	c->uri = 0;
	c->dri = 0;
	cclose(c);
	POPERROR();
	return 0;
}
#endif //% -------------------------------------

static int sseek(ulong *arg)  // ONERR <- void
{
	Chan *c;
	uchar buf[sizeof(Dir)+100];
	Dir dir;
	int n;
	vlong off;
	union {
		vlong v;
		ulong u[2];
	} o;

       DBGPRN("> %s(%d %d %d) \n", __FUNCTION__, arg[0], arg[1], arg[2]);

	c = fdtochan(arg[1], -1, 1, 1);
	IF_ERR_RETURN(c, nil, ONERR);  //%

	if(WASERROR()){
	_ERR_1:
		cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}
	if(devtab[c->type]->dc == '|')
	        ERROR_GOTO(Eisstream, _ERR_1); //%

	off = 0;
	o.u[0] = arg[2];
	o.u[1] = arg[3];
	switch(arg[4]){
	case 0:
		off = o.v;
		if((c->qid.type & QTDIR) && off != 0)
		        ERROR_GOTO(Eisdir, _ERR_1); //%
		if(off < 0)
		        ERROR_GOTO(Enegoff, _ERR_1);  //%
		c->offset = off;
		break;

	case 1:
		if(c->qid.type & QTDIR)
		        ERROR_GOTO(Eisdir, _ERR_1); //%
		lock(& c->_ref._lock);	//% /* lock for read/write update */
		off = o.v + c->offset;
		if(off < 0){
		        unlock(& c->_ref._lock);  //% (c);
			ERROR_GOTO(Enegoff, _ERR_1);  //%
		}
		c->offset = off;
		unlock(& c->_ref._lock);  //%  (c);
		break;

	case 2:
		if(c->qid.type & QTDIR)
		        ERROR_GOTO(Eisdir, _ERR_1);  //%
		n = devtab[c->type]->stat(c, buf, sizeof buf);
		if(convM2D(buf, n, &dir, nil) == 0)
		        ERROR_GOTO("internal error: stat error in seek", _ERR_1); //%
		off = dir.length + o.v;
		if(off < 0)
		        ERROR_GOTO(Enegoff, _ERR_1); //%
		c->offset = off;
		break;

	default:
	        ERROR_GOTO(Ebadarg, _ERR_1); //%
	}
	*(vlong*)arg[0] = off;
	c->uri = 0;
	c->dri = 0;
	cclose(c);
	POPERROR();
	return  0; //%
}


#if 1 //% 061211 ----------------------------
long sysseek_l4(ulong *arg, vlong *seekpos, Clerkjob_t *myjob) //%  ONERR myjob
{
        DBGPRN(">%s(%d %d %d %d %x) \n", 
		   __FUNCTION__, arg[0], arg[1], arg[2], arg[3], seekpos);
	return  sseek_l4(arg, seekpos, myjob);  //%

}

#else //original ---------------------------
long sysseek(ulong *arg)
{
  DBGPRN(">%s(%d %d %d) \n", __FUNCTION__, arg[0], arg[1], arg[2]);
	//%  validaddr(arg[0], BY2V, 1);
	sseek(arg);
	return 0;
}
#endif //%----------------------------------


long sysoseek(ulong *arg, Clerkjob_t *myjob)
{
	union {
		vlong v;
		ulong u[2];
	} o;
	ulong a[5];
	DBGPRN(">%s() \n", __FUNCTION__);

	o.v = (long)arg[1];
	a[0] = (ulong)&o.v;
	a[1] = arg[0];
	a[2] = o.u[0];
	a[3] = o.u[1];
	a[4] = arg[2];
	sseek(a);
	return o.v;
}

long validstat(uchar *s, int n)   //% ONERR <- void
{
	int m;
	char buf[64];

	if(statcheck(s, n) < 0)
	  ERROR_RETURN(Ebadstat, ONERR);
	/* verify that name entry is acceptable */
	s += STATFIXLEN - 4*BIT16SZ;	/* location of first string */
	/*
	 * s now points at count for first string.
	 * if it's too long, let the server decide; this is
	 * only for his protection anyway. otherwise
	 * we'd have to allocate and WASERROR.
	 */
	m = GBIT16(s);
	s += BIT16SZ;
	if(m+1 > sizeof buf)
	        return 0;  //%
	memmove(buf, s, m);
	buf[m] = '\0';
	/* name could be '/' */
	if(strcmp(buf, "/") != 0)
	        validname(buf, 0);
	return  0; //%
}

static char* pathlast(Path *p)
{
	char *s;

	if(p == nil)
		return nil;
	if(p->len == 0)
		return nil;

	s = strrchr(p->s, '/'); 
	if(s)
		return s+1;
	return p->s;
}

//% int fstat(int fd, uchar *edir, int nedir);
long sysfstat(ulong *arg, Clerkjob_t *myjob)  // ONERR
{
	Chan *c;
	uint l;
#if USE_MAPPING //------------------------
	uint  adrs = (arg[1]-L4_Address(myjob->mapper)) + L4_Address(myjob->mappee); //%
#else //-----------------------
	uint adrs = arg[1]; //%
#endif //----------------------
	L4_ThreadId_t  client = myjob->client; //%
	DBGPRN("> %s \n", __FUNCTION__);

	l = arg[2];
	//%  validaddr(arg[1], l, 1);
	c = fdtochan(arg[0], -1, 0, 1);
	IF_ERR_RETURN(c, nil, ONERR);  //%

	if(WASERROR()) {
		cclose(c);
		nexterror();
	}

#if USE_MAPPING //%------------------------------------------
	l = devtab[c->type]->stat(c, (uchar*)adrs, l);   //%
#else //%----------------------------------------------------
	char  *_buf = malloc(l);
	l = devtab[c->type]->stat(c, _buf, l);
	if (l == ONERR) free(_buf);     //%
	IF_ERR_RETURN(l, ONERR, ONERR); //%

	proc2proc_copy(core_process_nr, (unsigned)_buf, TID2PROCNR(client), adrs, l);
	free(_buf);
#endif //%----------------------------------------------
	POPERROR();
	cclose(c);
	return l;
}


//%  int stat(char *name, uchar *edir, int nedir);
long sysstat(ulong *arg, Clerkjob_t *myjob)  //% ONERR
{
	char *name;
	Chan *c;
	uint l;
#if USE_MAPPING //--------------
	uint  adrs = (arg[1]-L4_Address(myjob->mapper)) + L4_Address(myjob->mappee); //%
#else //-----------------------
	uint adrs = arg[1];  //%
#endif //-----------------------
	L4_ThreadId_t  client = myjob->client; //%
	DBGPRN("> %s(\"%s\" %x %x) \n", __FUNCTION__, arg[0], arg[1], arg[2]);

	l = arg[2];
	//%  validaddr(arg[1], l, 1);   // edir: client-space
	//%  validaddr(arg[0], 1, 0);
	c = namec((char*)arg[0], Aaccess, 0, 0); 
	IF_ERR_RETURN(c, nil, ONERR);  //%

	if(WASERROR()){
		cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}

#if USE_MAPPING //%-------------------------------------
	l = devtab[c->type]->stat(c, (uchar*)adrs, l);  //% arg[1]

	name = pathlast(c->path);  

	if(name) {
 	        l = dirsetname(name, strlen(name), (uchar*)adrs, l, arg[2]); //% arg[1]
	}

	POPERROR();
	cclose(c);
	return l;
#else //%-----------------------------------------------
	char  *_buf = malloc(l);
	l = devtab[c->type]->stat(c, _buf, l);   //  pr_chan(c);
	if (l == ONERR) free(_buf);     //%
	IF_ERR_RETURN(l, ONERR, ONERR); //%

	//proc2proc_copy(core_process_nr, (unsigned)_buf, TID2PROCNR(client), adrs, l);

	name = pathlast(c->path);  

	if(name) { 
		l = dirsetname(name, strlen(name), _buf, l, arg[2]);
	}

	proc2proc_copy(core_process_nr, (unsigned)_buf, TID2PROCNR(client), adrs, l);
	
	free(_buf);
	POPERROR();
	cclose(c);
	return l;
#endif //%-----------------------------------
}

long syschdir(ulong *arg, Clerkjob_t *myjob) // ONERR
{
	Chan *c;
	DBGPRN("> %s('%s') \n", __FUNCTION__, arg[0]);

	//%  validaddr(arg[0], 1, 0);

	c = namec((char*)arg[0], Atodir, 0, 0);   //% pr_chan(c);
	//	PRN("syschdir('%s'):'%s' \n", arg[0], c->path->s); //%
	IF_ERR_RETURN(c, nil, ONERR); //%

	cclose(up->dot);  //% ?????

	up->dot = c;     //% pr_chan(up->dot); DBGBRK("");
	return 0;
}



long bindmount(int ismount, int fd, int afd, char* mname, char* mold, ulong flag, char* spec)  //% ONERR
{
	int ret;
	Chan *c0, *c1, *ac = nil, *bc = nil;  //%
	struct{
		Chan	*chan;
		Chan	*authchan;
		char	*spec;
		int	flags;
	}bogus;

DBGPRN(">%s(%s fd=%d afd=%d new='%s' old='%s' flg=%x spc='%s')\n", 
    __FUNCTION__, (ismount)?"mount":"bind", fd, afd, mname, mold, flag, spec);

	if((flag&~MMASK) || (flag&MORDER)==(MBEFORE|MAFTER))
	        ERROR_RETURN(Ebadarg, ONERR);  //%

	bogus.flags = flag & MCACHE;

	if(ismount){
		if(up->pgrp->noattach)
		        ERROR_RETURN(Enoattach, ONERR); //%

		ac = nil;
		bc = fdtochan(fd, ORDWR, 0, 1);
		IF_ERR_RETURN(bc, nil, ONERR); //%

		if(WASERROR()) {
		_ERR_1:
			if(ac)
				cclose(ac);
			cclose(bc);
			NEXTERROR_RETURN(ONERR);  //%
		}

		if(afd >= 0) {
			ac = fdtochan(afd, ORDWR, 0, 1);
			IF_ERR_GOTO(ac, nil, _ERR_1); //%
		}

		bogus.chan = bc;
		bogus.authchan = ac;

		//%  validaddr((ulong)spec, 1, 0);
		bogus.spec = spec;

		if(WASERROR()) {
		_ERR_2:
		        ERROR_GOTO(Ebadspec, _ERR_1);  //%
		}

		spec = validnamedup(spec, 1);
		POPERROR();  //% end of _ERR_2 -----

		if(WASERROR()){
		_ERR_3:
			free(spec);
			NEXTERROR_GOTO(_ERR_2);  //%
		}

		ret = devno('M', 0);  //DBGBRK("devno[M]=%d \n", ret);
		c0 = devtab[ret]->attach((char*)&bogus);  // pr_chan(c0);
		//-->  devmnt::attach()   c0: mnt('M')-channel

		IF_ERR_GOTO(c0, nil, _ERR_3);  //%%

		POPERROR();	/* spec */  //end of _ERR_3
		free(spec);

		POPERROR();	/* ac bc */ //end of _ERR_2
		if(ac)
			cclose(ac);
		cclose(bc);
	}
	else{
		bogus.spec = 0;
		//%  validaddr((ulong)mname, 1, 0);   
		c0 = namec(mname, Abind, 0, 0);  // pr_chan(c0);
		IF_ERR_RETURN(c0, nil, ONERR); //%
	}


	if(WASERROR()){    
	_ERR_4:
		cclose(c0);
		NEXTERROR_GOTO(_ERR_1);  //%%%  ?? return ??
	}

	//%  validaddr((ulong)mold, 1, 0);
	c1 = namec(mold, Amount, 0, 0);    // c1: mount point
	IF_ERR_GOTO(c1, nil, _ERR_4); //%%%

	if(WASERROR()){
	_ERR_5:
		cclose(c1);
		NEXTERROR_GOTO(_ERR_4);  //%%
	}

	ret = cmount(&c0, c1, flag, bogus.spec); 
	IF_ERR_GOTO(ret, ONERR, _ERR_5); //%%%

	POPERROR();  // end of _ERR_5
	cclose(c1);

	POPERROR();  // end of _ERR_4
	cclose(c0);
	if(ismount)
		fdclose(fd, 0);

	DBGPRN("<%s()-> %d\n", __FUNCTION__, ret);
	return ret;
}


//----- int bind(cahr *name, char *old, int flag) ----- 
long sysbind(ulong *arg, Clerkjob_t *myjob)  //% ONERR
{
        int  rc;

DBGPRN(">%s(name='%s' old='%s' flag=%x)\n", 
    __FUNCTION__, (char*)arg[0], (char*)arg[1], arg[2]);

	rc = bindmount(0, -1, -1, (char*)arg[0], (char*)arg[1], arg[2], nil);
	//---------------  --  ----- name  ---------- old ------ flag -----
	IF_ERR_RETURN(rc, ONERR, ONERR); //%

	DBGPRN("<%s('%s', '%s')->%d \n", __FUNCTION__, arg[0], arg[1], rc);
	return  rc;
}


//----- int mount(int fd, int afd, char* old, int flag, char *aname) -----
long sysmount(ulong *arg, Clerkjob_t *myjob) // ONERR
{
        int  rc;
DBGPRN(">%s(fd:%d afd:%d mntpt:'%s' flag:%x spec:'%s')\n", __FUNCTION__, 
    arg[0], arg[1], arg[2], arg[3], arg[4]);

	rc = bindmount(1, arg[0], arg[1], nil, (char*)arg[2], arg[3], (char*)arg[4]);
	//             ---  fd ---  afd --------  old ------  flag ---- aname ---
	if(rc<0) PRN("<sysmount():%x\n", rc);   //%
	IF_ERR_RETURN(rc, ONERR, ONERR); //%

	DBGPRN("<%s('%s', '%s')->%d \n", __FUNCTION__, arg[2], arg[4], rc);
	return  rc;
}


// mount(int fd, int afd, char *old, int flag, char *aname) 
long sys_mount(ulong *arg, Clerkjob_t *myjob) // ONERR
{
        int  rc;
	rc = bindmount(1, arg[0], -1, nil, (char*)arg[1], arg[2], (char*)arg[3]);
	IF_ERR_RETURN(rc, ONERR, ONERR); //%

        DBGPRN(">%s('%s', %x, '%s')->%d \n", __FUNCTION__, arg[1], arg[2], arg[3], rc);
	return   rc;
}


// umount(char *name, char *old)
long sysunmount(ulong *arg, Clerkjob_t *myjob)  // ONERR
{
	Chan *cmount, *cmounted;
	DBGPRN(">%s(%x:%s, %x:%s)\n", __FUNCTION__, 
	    arg[0], arg[0], arg[1], arg[1]);

#if 1 //% FIXME -- Temporary repair: String parameter passing needs repaire.
	if(arg[0] && ((*(char*)arg[0]) == 0))
	   arg[0] = 0;
#endif //-------------------------------

	cmounted = 0;

	//%  validaddr(arg[1], 1, 0);
	cmount = namec((char *)arg[1], Amount, 0, 0);
	IF_ERR_RETURN(cmount, nil, ONERR); //%

	if(arg[0]) {
		if(WASERROR()) {
		_ERR_1:
			cclose(cmount);
			NEXTERROR_RETURN(ONERR);  //%
		}
		//%  validaddr(arg[0], 1, 0);
		/*
		 * This has to be namec(..., Aopen, ...) because
		 * if arg[0] is something like /srv/cs or /fd/0,
		 * opening it is the only way to get at the real
		 * Chan underneath.
		 */
		cmounted = namec((char*)arg[0], Aopen, OREAD, 0);
		IF_ERR_GOTO(cmounted, nil, _ERR_1); //%

		POPERROR();
	}

	if(WASERROR()) {
	_ERR_2:
		cclose(cmount);
		if(cmounted)
			cclose(cmounted);
		NEXTERROR_GOTO(_ERR_1);  //%%
	}

	cunmount(cmount, cmounted);
	cclose(cmount);
	if(cmounted)
		cclose(cmounted);
	POPERROR();
	return 0;
}



//%  int create(char *file, int omode, ulong permission);
long syscreate(ulong *arg, Clerkjob_t *myjob)  //%   ONERR myjob
{
	int fd;
	Chan *c = 0;
	int rc;

DBGPRN(">%s(%s %x %x)\n", __FUNCTION__, arg[0], arg[1], arg[2]); 
	rc = openmode(arg[1]&~OEXCL);	/* error check only; OEXCL okay here */
	IF_ERR_RETURN(rc, ONERR, ONERR);  //%

	if(WASERROR()) {
	_ERR_1:
		if(c)
			cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}

	//%  validaddr(arg[0], 1, 0);
	DBGPRN(" syscreae:namec(%s Acreate %x %x)\n", arg[0], arg[1], arg[2]);
	c = namec((char*)arg[0], Acreate, arg[1], arg[2]);  
	DBGPRN(" %s -- chan=%s\n", __FUNCTION__, c->path->s);

	IF_ERR_GOTO(c, nil, _ERR_1); //%

	fd = newfd(c);
	if(fd < 0)
	        ERROR_GOTO(Enofd, _ERR_1); //% error(Enofd);
	POPERROR();

DBGPRN("<%s('%s', %x, %x)->%x \n", 
	       __FUNCTION__, arg[0], arg[1], arg[2], fd);
	return fd;
}


//%  int remove(char *file);
long sysremove(ulong *arg, Clerkjob_t *myjob)  //% ONERR
{
	Chan *c;
	DBGPRN("> %s \n", __FUNCTION__);

	//%  validaddr(arg[0], 1, 0);
	c = namec((char*)arg[0], Aremove, 0, 0);
	IF_ERR_RETURN(c, nil, ONERR); //%

	/*
	 * Removing mount points is disallowed to avoid surprises
	 * (which should be removed: the mount point or the mounted Chan?).
	 */
	if(c->ismtpt){
		cclose(c);
		ERROR_RETURN(Eismtpt, ONERR);  //%
	}

	if(WASERROR()){
	_ERR_1:
		c->type = 0;	/* see below */
		cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}
	devtab[c->type]->remove(c);
	/*
	 * Remove clunks the fid, but we need to recover the Chan
	 * so fake it up.  rootclose() is known to be a nop.
	 */
	c->type = 0;
	POPERROR();
	cclose(c);
	return 0;
}


static long wstat(Chan *c, uchar *d, int nd) // ONERR
{
	long l;
	int namelen;

	if(WASERROR()){
	_ERR_1:
		cclose(c);
		NEXTERROR_RETURN(ONERR);
	}

	if(c->ismtpt){
		/*
		 * Renaming mount points is disallowed to avoid surprises
		 * (which should be renamed? the mount point or the mounted Chan?).
		 */
		dirname(d, &namelen);
		if(namelen)
			nameerror(chanpath(c), Eismtpt);
	}

	l = devtab[c->type]->wstat(c, d, nd);
	IF_ERR_GOTO(l, ONERR, _ERR_1); //%

	POPERROR();
	cclose(c);
	return l;
}


//% wstat(char *name, uchar *edir, int nedir)
long syswstat(ulong *arg, Clerkjob_t *myjob)  //% ONERR
{
	Chan *c;
	uint l;
	int  rc;
#if USE_MAPPING //--------------
	uint  adrs = (arg[1]-L4_Address(myjob->mapper)) + L4_Address(myjob->mappee); //%
#else //-----------------
	uint adrs = arg[1];  //%
#endif //-------------------
	L4_ThreadId_t  client = myjob->client; //%
	DBGPRN("> %s \n", __FUNCTION__);

	l = arg[2];
	//%  validaddr(arg[1], l, 0);
	validstat((uchar*)arg[1], l);
	//%  validaddr(arg[0], 1, 0);

	c = namec((char*)arg[0], Aaccess, 0, 0);
	IF_ERR_RETURN(c, nil, ONERR); //%

#if USE_MAPPING //%-----------------------------------
	return wstat(c, (uchar*)adrs, l);  //% arg[1]
#else //%---------------------------------------------
	char  *_buf = malloc(l);
	rc =  wstat(c, _buf, l);
	proc2proc_copy(core_process_nr, (unsigned)_buf, TID2PROCNR(client), adrs, l);
	free(_buf);
	return  rc;
#endif //%--------------------------------------
}


long sysfwstat(ulong *arg, Clerkjob_t *myjob)  //% ONERR
{
	Chan *c;
	uint l;
	int  rc; //%
	L4_ThreadId_t  client = myjob->client; //%
#if USE_MAPPING //---------------
	uint  adrs = (arg[1]-L4_Address(myjob->mapper)) + L4_Address(myjob->mappee); //%
#else //-----------------
	uint adrs = arg[1];  //% 
#endif //--------------------
	l = arg[2];
	//%  validaddr(arg[1], l, 0);
	validstat((uchar*)arg[1], l);

	c = fdtochan(arg[0], -1, 1, 1);
	IF_ERR_RETURN(c, nil, ONERR); //%

#if USE_MAPPING //%--------------------------------------------
	return wstat(c, (uchar*)adrs, l);  //% arg[1]
#else //%-----------------------------------------------------
	char  *_buf = malloc(l);
	rc =  wstat(c, _buf, l);
	proc2proc_copy(core_process_nr, (unsigned)_buf, TID2PROCNR(client), arg[1], l);
	free(_buf);
	return  rc;
#endif //%------------------------------------------
}


static void packoldstat(uchar *buf, Dir *d)
{
	uchar *p;
	ulong q;

	/* lay down old stat buffer - grotty code but it's temporary */
	p = buf;
	strncpy((char*)p, d->name, 28);
	p += 28;
	strncpy((char*)p, d->uid, 28);
	p += 28;
	strncpy((char*)p, d->gid, 28);
	p += 28;
	q = d->qid.path & ~DMDIR;	/* make sure doesn't accidentally look like directory */
	if(d->qid.type & QTDIR)	/* this is the real test of a new directory */
		q |= DMDIR;
	PBIT32(p, q);
	p += BIT32SZ;
	PBIT32(p, d->qid.vers);
	p += BIT32SZ;
	PBIT32(p, d->mode);
	p += BIT32SZ;
	PBIT32(p, d->atime);
	p += BIT32SZ;
	PBIT32(p, d->mtime);
	p += BIT32SZ;
	PBIT64(p, d->length);
	p += BIT64SZ;
	PBIT16(p, d->type);
	p += BIT16SZ;
	PBIT16(p, d->dev);
}


long sys_stat(ulong *arg, Clerkjob_t *myjob) // ONERR
{
	Chan *c;
	uint l;
	uchar buf[128];	/* old DIRLEN plus a little should be plenty */
	char strs[128], *name;
	Dir d;
	char old[] = "old stat system call - recompile";
	DBGPRN("> %s \n", __FUNCTION__);

	//%  validaddr(arg[1], 116, 1);
	//%  validaddr(arg[0], 1, 0);
	c = namec((char*)arg[0], Aaccess, 0, 0);
	IF_ERR_RETURN(c, nil, ONERR); //%

	if(WASERROR()){
	_ERR_1:
		cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}
	l = devtab[c->type]->stat(c, buf, sizeof buf);
	IF_ERR_GOTO(l, ONERR, _ERR_1); //%

	/* buf contains a new stat buf; convert to old. yuck. */
	if(l <= BIT16SZ)	/* buffer too small; time to face reality */
	        ERROR_GOTO(old, _ERR_1);  //%

	name = pathlast(c->path);
	if(name)
		l = dirsetname(name, strlen(name), buf, l, sizeof buf);
	l = convM2D(buf, l, &d, strs);
	if(l == 0)
	        ERROR_GOTO(old, _ERR_1);  //%
	packoldstat((uchar*)arg[1], &d);
	
	POPERROR();
	cclose(c);
	return 0;
}

long sys_fstat(ulong *arg, Clerkjob_t *myjob) // ONERR
{
	Chan *c;
	char *name;
	uint l;
	uchar buf[128];	/* old DIRLEN plus a little should be plenty */
	char strs[128];
	Dir d;
	char old[] = "old fstat system call - recompile";
	DBGPRN("> %s \n", __FUNCTION__);

	//%  validaddr(arg[1], 116, 1);
	c = fdtochan(arg[0], -1, 0, 1);
	IF_ERR_RETURN(c, nil, ONERR); //%

	if(WASERROR()){
	_ERR_1:
		cclose(c);
		NEXTERROR_RETURN(ONERR);  //%
	}

	l = devtab[c->type]->stat(c, buf, sizeof buf);
	IF_ERR_GOTO(l, ONERR, _ERR_1); //%

	/* buf contains a new stat buf; convert to old. yuck. */
	if(l <= BIT16SZ)	/* buffer too small; time to face reality */
	        ERROR_GOTO(old, _ERR_1);  //%
	name = pathlast(c->path);
	if(name)
		l = dirsetname(name, strlen(name), buf, l, sizeof buf);
	l = convM2D(buf, l, &d, strs);
	if(l == 0)
	        ERROR_GOTO(old, _ERR_1);  //%
	packoldstat((uchar*)arg[1], &d);
	
	POPERROR();
	cclose(c);
	return 0;
}


long sys_wstat(ulong * _x, Clerkjob_t *myjob)  // ONERR
{
	DBGPRN("> %s \n", __FUNCTION__);
	ERROR_RETURN("old wstat system call - recompile", ONERR);
	return -1;
}

long sys_fwstat(ulong * _x)  // ONERR
{
	DBGPRN("> %s \n", __FUNCTION__);
	ERROR_RETURN("old fwstat system call - recompile", ONERR);
	return -1;
}

long syspassfd(ulong * _x, Clerkjob_t *myjob) // ONERR
{
	DBGPRN("> %s \n", __FUNCTION__);
	ERROR_RETURN("passfd unimplemented", ONERR);
	return -1;
}

