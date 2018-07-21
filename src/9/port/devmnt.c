//%%%%%%%%%%% devmnt.c %%%%%%%%%%%%%%%%%%%%%%%%
/*  (C)  Bell Lab.
 *  (C2) KM
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

//%begin---------------------------------------------------
#include   <l4all.h>     //Used in dbg_time() 
#include   <l4/l4io.h>

#define   _DBGFLG   0 
#include   <l4/_dbgpr.h>

#include  "../errhandler-l4.h"

#define   print  print
#define   PRN    print
#define DXX(n) {print("%s  %s:%d ", (n==0)?"\n":"", __FUNCTION__, n);}

extern void dump_mem(char *, char *, int);  //% 
extern void dbg_time(int which, char *msg);
#define dbg_time(x, y)  //%

void pr_rpc(int type);
//%end----------------------------------------------------


/*
 * References are managed as follows:
 * The channel to the server - a network connection or pipe - has one
 * reference for every Chan open on the server.  The server channel has
 * c->mux set to the Mnt used for muxing control to that server.  Mnts
 * have no reference count; they go away when c goes away.
 * Each channel derived from the mount point has mchan set to c,
 * and increfs/decrefs mchan to manage references on the server
 * connection.
 */

#define MAXRPC (IOHDRSZ+8192)

struct Mntrpc
{
	Chan*	c;		/* Channel for whom we are working */
	Mntrpc*	list;		/* Free/pending list */
	Fcall	request;	/* Outgoing file system protocol message */
	Fcall 	reply;		/* Incoming reply */
	Mnt*	m;		/* Mount device during rpc */
	Rendez	r;		/* Place to hang out */
	uchar*	rpc;		/* I/O Data buffer */
	uint	rpclen;		/* len of buffer */
	Block	*b;		/* reply blocks */
	char	done;		/* Rpc completed */
	uvlong	stime;		/* start time for mnt statistics */
	ulong	reqlen;		/* request length for mnt statistics */
	ulong	replen;		/* reply length for mnt statistics */
	Mntrpc*	flushed;	/* message this one flushes */
};


enum
{
	TAGSHIFT = 5,			/* ulong has to be 32 bits */
	TAGMASK = (1<<TAGSHIFT)-1,
	NMASK = (64*1024)>>TAGSHIFT,
};


struct Mntalloc
{
        Lock    _lock;    //% 
	Mnt*	list;		/* Mount devices in use */
	Mnt*	mntfree;	/* Free list */
	Mntrpc*	rpcfree;
	int	nrpcfree;
	int	nrpcused;
	ulong	id;
	ulong	tagmask[NMASK];
}mntalloc;


/*  portdat.h
 *  struct Mnt
 *  {
 *    Lock    _lock; //%
 *    Chan    *c;     // Channel to file service 
 *    Proc    *rip;   // Reader in progress 
 *    Mntrpc  *queue; // Queue of pending requests on this channel 
 *    ulong   id;     // Multiplexer id for channel check 
 *    Mnt     *list;  // Free list 
 *    int     flags;  // cache 
 *    int     msize;  // data + IOHDRSZ 
 *    char    *version; // 9P version 
 *    Queue   *q;       // input queue 
 *  };
*/


Mnt*	mntchk(Chan*);
void	mntdirfix(uchar*, Chan*);
Mntrpc*	mntflushalloc(Mntrpc*, ulong);
void	mntflushfree(Mnt*, Mntrpc*);
void	mntfree(Mntrpc*);
void	mntgate(Mnt*);
void	mntpntfree(Mnt*);
void	mntqrm(Mnt*, Mntrpc*);
Mntrpc*	mntralloc(Chan*, ulong);
long	mntrdwr(int, Chan*, void*, long, vlong);
int	mntrpcread(Mnt*, Mntrpc*);
int	mountio(Mnt*, Mntrpc*);    //% void
void	mountmux(Mnt*, Mntrpc*);
int	mountrpc(Mnt*, Mntrpc*);   //% void
int	rpcattn(void*);
Chan*	mntchan(void);

char	Esbadstat[] = "invalid directory entry received from server";
char	Enoversion[] = "version not established for mount channel";


void (*mntstats)(int, Chan*, uvlong, ulong);


static void mntreset(void)
{
        mntalloc.id = 1;    //% set Mnt.id, Mnt-chan.dev 
	mntalloc.tagmask[0] = 1;			/* don't allow 0 as a tag */
	mntalloc.tagmask[NMASK-1] = 0x80000000UL;	/* don't allow NOTAG */
	fmtinstall('F', fcallfmt);
	fmtinstall('D', dirfmt);
/* We can't install %M since eipfmt does and is used in the kernel [sape] */

	cinit();
}


/*
 * Version is not multiplexed: message sent only once per connection.
 *   - Start session
 *   - c argument: server-connection
 */
long mntversion(Chan *c, char *version, int msize, int returnlen)
{
	Fcall f;
	uchar *msg;
	Mnt *m;
	char *v;
	long k, l;
	uvlong oo;
	char buf[128];

DBGPRN(">%s(\"%s\" \"%s\" %d %d) ", 
    __FUNCTION__, c->path->s, version, msize, returnlen);

	qlock(&c->umqlock);	/* make sure no one else does this until we've established ourselves */

	if(WASERROR()){
	_ERR_1:
		qunlock(&c->umqlock);
		NEXTERROR_RETURN(ONERR);   //%
	}

	/* defaults */
	if(msize == 0)
		msize = MAXRPC;
	if(msize > c->iounit && c->iounit != 0)
		msize = c->iounit;
	v = version;
	if(v == nil || v[0] == '\0')
		v = VERSION9P;

	/* validity */
	if(msize < 0)
	        ERROR_GOTO("bad iounit in version call", _ERR_1);  //%

	if(strncmp(v, VERSION9P, strlen(VERSION9P)) != 0)
	        ERROR_GOTO("bad 9P version specification", _ERR_1); //%

	m = c->mux;

	/*  c->+- Chan -+   m->+- Mnt --+   
	 *     |        |      |        |
	 *     |        |      |        |
	 *     |  mux -------->|        |
	 *     |        |      |        |
	 *     +--------+      +--------+
	 *    ServerConnection
	 */

	if(m != nil){  // Mnt-table is already setup
		qunlock(&c->umqlock);
		POPERROR();

		strecpy(buf, buf+sizeof buf, m->version);
		k = strlen(buf);
		if(strncmp(buf, v, k) != 0){
			snprint(buf, sizeof buf, "incompatible 9P versions %s %s", m->version, v);
			ERROR_RETURN(buf, ONERR); //% 
		}
		if(returnlen > 0){
			if(returnlen < k)
			        ERROR_RETURN(Eshort, ONERR); //% 
			
			memmove(version, buf, k);
		}
		return k;
	}

	f.type = Tversion;
	f.tag = NOTAG;
	f.msize = msize;
	f.version = v;

	msg = malloc(8192+IOHDRSZ);
	if(msg == nil)
	        EXHAUSTED_GOTO("version memory", _ERR_1);  //%

	if(WASERROR()){
	_ERR_2:
		free(msg);
		NEXTERROR_GOTO(_ERR_1);   //% 
	}

	k = convS2M(&f, msg, 8192+IOHDRSZ);
	if(k == 0)
	  ERROR_GOTO("bad fversion conversion on send", _ERR_2);//% error("bad fversion conversion on send");

	lock(&c->_ref._lock);   //% lock(c);
	oo = c->offset;
	c->offset += k;
	unlock(&c->_ref._lock);  
	
//DXX(10);// pr_chan(c);
	l = devtab[c->type]->write(c, msg, k, oo);  // --> Server
	if (_DBGFLG) dump_mem(__FUNCTION__, msg, l);  //%
//DXX(l);



	if(l < k){
		lock(&c->_ref._lock);  //% c
		c->offset -= k - l;
		unlock(&c->_ref._lock);  //% c
		ERROR_GOTO("short write in fversion", _ERR_2);
		//% error("short write in fversion");
	}

	/* message sent; receive and decode reply */
	//  <--- Server

//DXX(20);
	k = devtab[c->type]->read(c, msg, 8192+IOHDRSZ, c->offset);
//DXX(k);

	if (_DBGFLG) dump_mem(__FUNCTION__, msg, k);  //% 

	if(k <= 0)
	        ERROR_GOTO("EOF receiving fversion reply", _ERR_2); //% 

	lock(&c->_ref._lock);  //% c
	c->offset += k;
	unlock(&c->_ref._lock);  //% c

	l = convM2S(msg, k, &f);

	if(l != k)
	        ERROR_GOTO("bad fversion conversion on reply", _ERR_2); //%

	if(f.type != Rversion){
		if(f.type == Rerror)
		        ERROR_GOTO(f.ename, _ERR_2);    //% 
		ERROR_GOTO("unexpected reply type in fversion", _ERR_2);//%
	}

	if(f.msize > msize)
	        ERROR_GOTO("server tries to increase msize in fversion", _ERR_2); //%

	if(f.msize<256 || f.msize>1024*1024)
	        ERROR_GOTO("nonsense value of msize in fversion", _ERR_2); //%

	k = strlen(f.version);
	if(strncmp(f.version, v, k) != 0)
	        ERROR_GOTO("bad 9P version returned from server", _ERR_2); //%


	/* now build Mnt associated with this connection */
	lock(&mntalloc._lock);   //%  (&mntalloc)
	m = mntalloc.mntfree;
	if(m != 0)
		mntalloc.mntfree = m->list;
	else {
		m = malloc(sizeof(Mnt));
		if(m == 0) {
		  unlock(&mntalloc._lock);  //% (&mntalloc)
		  EXHAUSTED_GOTO("mount devices", _ERR_2);    //%
		}
	}
	m->list = mntalloc.list;
	mntalloc.list = m;
	m->version = nil;
	kstrdup(&m->version, f.version);
	m->id = mntalloc.id++;
	m->q = qopen(10*MAXRPC, 0, nil, nil);
	m->msize = f.msize;
	unlock(&mntalloc._lock);   //%  (&mntalloc)

	if(returnlen > 0){
		if(returnlen < k)
			error(Eshort);
		memmove(version, f.version, k);
	}

	POPERROR();	/* msg */
	free(msg);

	lock(&m->_lock);   //% (m)
	m->queue = 0;
	m->rip = 0;

	c->flag |= CMSG; // Message channel for a mount
	c->mux = m;
	m->c = c;
	unlock(&m->_lock);   //% (m)

	POPERROR();	/* c */
	qunlock(&c->umqlock);


	DBGPRN("<%s()->%d\n", __FUNCTION__, k);	
	return k;
}


Chan* mntauth(Chan *c, char *spec)
{
	Mnt *m;
	Mntrpc *r;
	int     rc;

	// Here, c is the Server-connection.
	DBGPRN(">%s() ", __FUNCTION__);
	m = c->mux;

	if(m == nil){
		mntversion(c, VERSION9P, MAXRPC, 0);
		m = c->mux;
		if(m == nil)
		        ERROR_RETURN(Enoversion, nil); //%
	}

	//--- From now on, c is Mnt-channel. ---
	c = mntchan();
	if(WASERROR()) {
	_ERR_1:
		/* Close must not be called since it will
		 * call mnt recursively
		 */
		chanfree(c);
		NEXTERROR_RETURN(nil);  //%
	}

	r = mntralloc(0, m->msize);

	if(WASERROR()) {
	_ERR_2:
		mntfree(r);
		NEXTERROR_GOTO(_ERR_1);  //%
	}

	r->request.type = Tauth;
	r->request.afid = c->fid;
	r->request.uname = up->user;
	r->request.aname = spec;

	rc = mountrpc(m, r); //%
	IF_ERR_GOTO(rc, ONERR, _ERR_2); //%

	c->qid = r->reply.aqid;
	c->mchan = m->c;
	incref(&m->c->_ref);   //% (m->c)
	c->mqid = c->qid;
	c->mode = ORDWR;

	POPERROR();	/* r */
	mntfree(r);

	POPERROR();	/* c */

	return c;
}

/*   Returned           Input c->
 *    c--> +-- Chan  --+       +-- Chan --+    m-> +-- Mnt ---+
 *         | type: 'M' |       |          |        |          |
 *         | dev       |       |          |<-------|--c       |
 *         | qid:aqid  |       | qid      |        |          |
 *	   | mchan ----|-----> |          |        |          |
 * 	   | mqid:aqid |       | mux -----|------> |          |
 *	   |           |       |          |        +----------+
 *	   +-----------+       +----------+
 *                           Server-connection
 */



/*                          bogus.chan
 *   c --> +-- Chan  --+       +-- Chan --+   m -> +-- Mnt ---+
 *         | type: 'M' |       |          |        |          |
 *         | dev       |       |          |<-------|--c       |
 *         | qid       |       | qid      |        |          |
 *	   | mchan ----|-----> |          |        |          |
 * 	   | mqid      |       | mux -----|------> |          |
 *	   |           |       |          |        +----------+
 *	   +-----------+       +----------+
 *                           pipe or com-link
 */
static Chan* mntattach(char *muxattach) //% ONERR
{
	Mnt *m;
	Chan *c;
	Mntrpc *r;
	struct bogus{
	        Chan	*chan;  // Server-connection
		Chan	*authchan;
		char	*spec;
		int	flags;
	}bogus;
	int  rc;

	bogus = *((struct bogus *)muxattach);
	c = bogus.chan;  // Server-connection

DBGPRN(">%s(chan:'%s' spec:'%s' flag:%x) \n", 
    __FUNCTION__, c->path->s, bogus.spec, bogus.flags);

	m = c->mux;

	if(m == nil){
//  DXX(10);
		mntversion(c, nil, 0, 0);
//  DXX(20);
		m = c->mux;
		if(m == nil) {

		        ERROR_RETURN(Enoversion, nil); //%
		}
	}

	// From now on, c is Mnt-cnannel -------
 
	c = mntchan();
	if(WASERROR()) {
	_ERR_1:
		/* Close must not be called since it will
		 * call mnt recursively
		 */
		chanfree(c);

		NEXTERROR_RETURN(nil);  //% nexterror();
	}

	// DBGPRN(" %s 30 \n", __FUNCTION__);
	r = mntralloc(0, m->msize);

	if(WASERROR()) {
	_ERR_2:
		mntfree(r);
		NEXTERROR_GOTO(_ERR_1);  //% nexterror();
	}

	r->request.type = Tattach;
	r->request.fid = c->fid;

	if(bogus.authchan == nil)
		r->request.afid = NOFID;
	else
		r->request.afid = bogus.authchan->fid;

	r->request.uname = up->user;
	r->request.aname = bogus.spec;
//DXX(50);
	rc = mountrpc(m, r);
//DXX(60);
	IF_ERR_GOTO(rc, ONERR, _ERR_2);

	c->qid = r->reply.qid;
	c->mchan = m->c;
	incref(&m->c->_ref);  //% m->c
	c->mqid = c->qid;

	POPERROR();	/* r */
	mntfree(r);

	POPERROR();	/* c */

	if(bogus.flags&MCACHE)
		c->flag |= CCACHE;

	DBGPRN("<%s()->\"%s\" \n", __FUNCTION__, c->path->s);
        return c;
}


Chan* mntchan(void)
{
	Chan *c;
	DBGPRN(">%s() ", __FUNCTION__);

	c = devattach('M', 0);
	lock(&mntalloc._lock);   //%  (&mntalloc)
	c->dev = mntalloc.id++;
	unlock(&mntalloc._lock);   //%  (&mntalloc)

	if(c->mchan)
		panic("mntchan non-zero %p", c->mchan);
	return c;
}


static Walkqid* mntwalk(Chan *c, Chan *nc, char **name, int nname)
{
	int i, alloc;
	Mnt *m;
	Mntrpc *r;
	Walkqid *wq;
	int  rc;

int  ZZZ = 0;
if((nc==nil) & (name==nil) & (nname==0)) ZZZ=1; 


	DBGPRN(">%s() ", __FUNCTION__);
	if(nc != nil)
		print("mntwalk: nc != nil\n");
	if(nname > MAXWELEM)
	        ERROR_RETURN("devmnt: too many name elements", nil); //%

	alloc = 0;
	wq = smalloc(sizeof(Walkqid)+(nname-1)*sizeof(Qid));

	if(WASERROR()){
	_ERR_1:
		if(alloc && wq->clone!=nil)
			cclose(wq->clone);
		free(wq);
		return nil;
	}

	alloc = 0;
	m = mntchk(c);
	r = mntralloc(c, m->msize); // Each Mntrpc has assigned unique <tag>

	if(nc == nil){
		nc = devclone(c);
		/*
		 * Until the other side accepts this fid, we can't mntclose it.
		 * Therefore set type to 0 for now; rootclose is known to be safe.
		 */
		nc->type = 0;
		alloc = 1;
	}
	wq->clone = nc;

	if(WASERROR()) {
	_ERR_2:
		mntfree(r);
		NEXTERROR_GOTO(_ERR_1);  //%
	}

	r->request.type = Twalk;
	r->request.fid = c->fid;
	r->request.newfid = nc->fid;
	r->request.nwname = nname;
	memmove(r->request.wname, name, nname*sizeof(char*));

//if(ZZZ) PRN("mntwalk1() "); 	

	rc = mountrpc(m, r);
//if(ZZZ) PRN("mntwalk2() "); 	

	IF_ERR_GOTO(rc, ONERR, _ERR_2);

	if(r->reply.nwqid > nname)
	        ERROR_GOTO("too many QIDs returned by walk", _ERR_2); //%

	if(r->reply.nwqid < nname){
		if(alloc)
			cclose(nc);
		wq->clone = nil;
		if(r->reply.nwqid == 0){
			free(wq);
			wq = nil;
			goto Return;
		}
	}

	/* move new fid onto mnt device and update its qid */
	if(wq->clone != nil){
		if(wq->clone != c){
			wq->clone->type = c->type;
			wq->clone->mchan = c->mchan;
			incref(&m->c->_ref);  //% m->c
		}
		if(r->reply.nwqid > 0)
			wq->clone->qid = r->reply.wqid[r->reply.nwqid-1];
	}
	wq->nqid = r->reply.nwqid;
	for(i=0; i<wq->nqid; i++)
		wq->qid[i] = r->reply.wqid[i];

    Return:
	POPERROR();
	mntfree(r);

	POPERROR();
	return wq;
}

/*               
 *    c--> +-- Chan  --+  nc-->+-- Chan --+   wq-> +-Walkqid--+ 
 *         |           |       |          |<-------+--clone   | 
 *         |           |       |          |        |  nqid    |
 *	   | mchan -+  |       | mchan    |        |  qid[]   | 
 *	   |        |  |       |   |      |        |          | 
 *	   +--------|--+       +---|------+        +----------+ 
 *                  |<-------------+
 *                  |
 *         +-- Chan ---+       +--- Mnt --+ 
 *         |           |       |          |
 *         |   mux ----|------>|          |
 *         |           |       |          |
 *         +-----------+       +----------+                
 */


/*                          bogus.chan
 *     --> +-- Chan  --+       +-- Chan --+    m-> +-- Mnt ---+    r-> +- Mntrpc -+
 *         | type: 'M' |       |          |        |          |        |          |
 *         |           |       |          |<-------|--c       |        |  c       |
 *	   | mchan ----|-----> |          |        |          |        |  |       |
 *	   | mqid      |       | mux -----|------> |          |        |  |       |
 *	   |           |       |          |        +----------+        +--|-------+
 *	   +-----------+ <-+   +----------+                               |
 *                         +----------------------------------------------+
 */
static int mntstat(Chan *c, uchar *dp, int n)
{
	Mnt *m;
	Mntrpc *r;
	int     rc;  //%

	DBGPRN(">%s() ", __FUNCTION__);
	if(n < BIT16SZ)
		error(Eshortstat);
	m = mntchk(c);   // m: c->mchan->mux
	r = mntralloc(c, m->msize);

	if(WASERROR()) {
	_ERR_1:
		mntfree(r);
		NEXTERROR_RETURN(ONERR);  //% nexterror();
	}

	r->request.type = Tstat;
	r->request.fid = c->fid;

	rc = mountrpc(m, r);
	IF_ERR_GOTO(rc, ONERR, _ERR_1);

	if(r->reply.nstat > n){
		n = BIT16SZ;
		PBIT16((uchar*)dp, r->reply.nstat-2);
	}else{
		n = r->reply.nstat;
		memmove(dp, r->reply.stat, n);
		validstat(dp, n);
		mntdirfix(dp, c);
	}

	POPERROR();
	mntfree(r);
	return n;
}


static Chan* mntopencreate(int type, Chan *c, char *name, int omode, ulong perm)
{
	Mnt *m;
	Mntrpc *r;
	int     rc;

	DBGPRN(">%s() ", __FUNCTION__);

	dbg_time(2, 0);  //%
	m = mntchk(c);   // m: c->mchan->mux
	r = mntralloc(c, m->msize);

	if(WASERROR()) {
	_ERR_1:
		mntfree(r);
		NEXTERROR_RETURN(nil);  //% nexterror();
	}

	r->request.type = type;
	r->request.fid = c->fid;
	r->request.mode = omode;
	if(type == Tcreate){
		r->request.perm = perm;
		r->request.name = name;
	}

	rc = mountrpc(m, r);  //%
	IF_ERR_GOTO(rc, ONERR, _ERR_1);  //%

	c->qid = r->reply.qid;
	c->offset = 0;
	c->mode = openmode(omode);
	c->iounit = r->reply.iounit;
	if(c->iounit == 0 || c->iounit > m->msize-IOHDRSZ)
		c->iounit = m->msize-IOHDRSZ;
	c->flag |= COPEN;

	POPERROR();
	mntfree(r);

	if(c->flag & CCACHE)
		copen(c);

	dbg_time(2, "devmnt:opencreate"); //%
	return c;
}


static Chan* mntopen(Chan *c, int omode)
{
	return mntopencreate(Topen, c, nil, omode, 0);
}


static Chan* mntcreate(Chan *c, char *name, int omode, ulong perm) //% ONERR <- void
{
        //%  mntopencreate(Tcreate, c, name, omode, perm);
	return mntopencreate(Tcreate, c, name, omode, perm);
}


static int mntclunk(Chan *c, int t)  //% ONERR <- void
{
	Mnt *m;
	Mntrpc *r;
	int     rc;

	m = mntchk(c);    // m: c->mchan->mux
	r = mntralloc(c, m->msize);

	if(WASERROR()){
	_ERR_1:
		mntfree(r);
		NEXTERROR_RETURN(ONERR);    //% nexterror();
	}

	r->request.type = t;
	r->request.fid = c->fid;

	rc = mountrpc(m, r);         //%
	IF_ERR_GOTO(rc, ONERR, _ERR_1);  //%

	mntfree(r);

	POPERROR();
	return  0; //%
}


void muxclose(Mnt *m)
{
	Mntrpc *q, *r;

	for(q = m->queue; q; q = r) {
		r = q->list;
		mntfree(q);
	}
	m->id = 0;
	free(m->version);
	m->version = nil;
	mntpntfree(m);
}


void mntpntfree(Mnt *m)
{
	Mnt *f, **l;
	Queue *q;

	lock(&mntalloc._lock);   //%  (&mntalloc)
	l = &mntalloc.list;
	for(f = *l; f; f = f->list) {
		if(f == m) {
			*l = m->list;
			break;
		}
		l = &f->list;
	}
	m->list = mntalloc.mntfree;
	mntalloc.mntfree = m;
	q = m->q;
	unlock(&mntalloc._lock);   //%  (&mntalloc)

	qfree(q);
}


static int mntclose(Chan *c)  //% int <- void
{
	mntclunk(c, Tclunk);
	return  1;
}


static int mntremove(Chan *c)   //% int <- void
{
	mntclunk(c, Tremove);
	return  1;
}


static int mntwstat(Chan *c, uchar *dp, int n)
{
	Mnt *m;
	Mntrpc *r;
	int     rc;

	DBGPRN(">%s() ", __FUNCTION__);
	m = mntchk(c);
	r = mntralloc(c, m->msize);
	if(WASERROR()) {
	_ERR_1:
		mntfree(r);
		NEXTERROR_RETURN(ONERR);  //% nexterror();
	}
	r->request.type = Twstat;
	r->request.fid = c->fid;
	r->request.nstat = n;
	r->request.stat = dp;

	rc = mountrpc(m, r); //%
	IF_ERR_GOTO(rc, ONERR, _ERR_1);  //%

	POPERROR();
	mntfree(r);
	return n;
}


static long mntread(Chan *c, void *buf, long n, vlong off) //% ONERR
{
	uchar *p, *e;
	int nc, cache, isdir, dirlen;

	DBGPRN(">%s() ", __FUNCTION__);

	dbg_time(2, 0); //%
	isdir = 0;
	cache = c->flag & CCACHE;
	if(c->qid.type & QTDIR) {
		cache = 0;
		isdir = 1;
	}

	p = buf;
	if(cache) {
		nc = cread(c, buf, n, off);
		if(nc > 0) {
			n -= nc;
			if(n == 0)
				return nc;
			p += nc;
			off += nc;
		}
		n = mntrdwr(Tread, c, p, n, off);
		cupdate(c, p, n, off);
		return n + nc;
	}

	n = mntrdwr(Tread, c, buf, n, off);  // ONERR
	if(isdir) {
		for(e = &p[n]; p+BIT16SZ < e; p += dirlen){
			dirlen = BIT16SZ+GBIT16(p);
			if(p+dirlen > e)
				break;
			validstat(p, dirlen);
			mntdirfix(p, c);
		}
		if(p != e)
		  ERROR_RETURN(Esbadstat, ONERR);  //%  error(...)
	}
	dbg_time(2, "devmnt:read"); //%
	return n;
}


static long mntwrite(Chan *c, void *buf, long n, vlong off) //% ONERR
{
        long  rc; 
	dbg_time(2, 0); //%
	rc = mntrdwr(Twrite, c, buf, n, off);
	dbg_time(2, "devmnt:write"); //%
	return  rc;
}



long mntrdwr(int type, Chan *c, void *buf, long n, vlong off) //% ONERR
{
	Mnt *m;
 	Mntrpc *r;
	char *uba;
	int cache;
	ulong cnt, nr, nreq;
	int   rc; //%

	m = mntchk(c);
	uba = buf;
	cnt = 0;
	cache = c->flag & CCACHE;
	if(c->qid.type & QTDIR)
		cache = 0;

	for(;;) {
		r = mntralloc(c, m->msize);

		if(WASERROR()) {
		_ERR_1:
			mntfree(r);
			NEXTERROR_RETURN(ONERR);   //%
		}

		r->request.type = type;
		r->request.fid = c->fid;
		r->request.offset = off;
		r->request.data = uba;
		nr = n;
		if(nr > m->msize-IOHDRSZ)
			nr = m->msize-IOHDRSZ;
		r->request.count = nr;

		// l4printf_c(255, " >mntrpc"); //%%
		rc = mountrpc(m, r);
		// l4printf_c(255, " <mntrpc:%d\n", rc); //%% 

		IF_ERR_GOTO(rc, ONERR, _ERR_1);

		nreq = r->request.count;
		nr = r->reply.count;
		if(nr > nreq)
			nr = nreq;

		if(type == Tread)
			r->b = bl2mem((uchar*)uba, r->b, nr);
		else if(cache)
			cwrite(c, (uchar*)uba, nr, off);

		POPERROR();
		mntfree(r);

		off += nr;
		uba += nr;
		cnt += nr;
		n -= nr;
		if(nr != nreq || n == 0 || up->nnote)
			break;
	}

	// l4printf_g(" mntrdwr(%d %s):%d \n", type, c->path->s, cnt); //%%%
	return cnt;
}


int mountrpc(Mnt *m, Mntrpc *r)  //% ONERR <- void
{
	char *sn, *cn;
	int t;
	int rc; //% 

	r->reply.tag = 0;
	r->reply.type = Tmax;	/* can't ever be a valid message type */

//%	pr_rpc(r->request.type);  //%

	rc = mountio(m, r);  //%

	IF_ERR_RETURN(rc, ONERR, ONERR);  //%
//%	pr_rpc(r->reply.type);  //%

	t = r->reply.type;

	switch(t) {
	case Rerror:
	        //PRN("Xmountrpc:%s  \n", r->reply.ename); _backtrace_(); //%
	        ERROR_RETURN(r->reply.ename, ONERR);  //%

	case Rflush:
	        ERROR_RETURN(Eintr, ONERR);  //%

	default:
		if(t == r->request.type+1)
			break;
		sn = "?";
		if(m->c->path != nil)
			sn = m->c->path->s;
		cn = "?";
		if(r->c != nil && r->c->path != nil)
			cn = r->c->path->s;
		print("mnt: proc %s %lud: mismatch from %s %s rep 0x%lux tag %d fid %d T%d R%d rp %d\n",
			up->text, up->pid, sn, cn,
			r, r->request.tag, r->request.fid, r->request.type,
			r->reply.type, r->reply.tag);
		ERROR_RETURN(Emountrpc, ONERR);      //% 
	}
	return 0;
}



/*                          
 *     --> +-- Chan --+  m -> +-- Mnt ---+   r -> +- Mntrpc--+        +- Mntrpc -+
 *         |          | <-----|-- c      |        |          |        |          |
 *         |          |       |          |        | list ----|------> | list     |
 *	   | mux   ---|-----> | queue ---|------> | r        |        | r        |
 *	   |          |       | rip      |        | rpc      |        | rpc      |
 *	   |          |       | q        |        +----------+        +----------+
 *	   +----------+       +----------+                               
 *                       
 */
int mountio(Mnt *m, Mntrpc *r)  //% ONERR <- void
{
	int n;
	int rc;
	DBGPRN(" >mountio(%d) ", r->request.type); //%%

	while(WASERROR()) {
	_ERR_1:
		if(m->rip == up)
			mntgate(m);
		if(strcmp(up->errstr, Eintr) != 0){
			mntflushfree(m, r);
			l4printf_c(255, " <mountio:Er1 "); //%%

			NEXTERROR_RETURN(ONERR);  //% ?
		}
		r = mntflushalloc(r, m->msize);
	}

	lock(&m->_lock);   //% (m)
	r->m = m;
	r->list = m->queue;
	m->queue = r;
	unlock(&m->_lock);   //% (m)

	/* Transmit a file system rpc */
	if(m->msize == 0)
		panic("msize");

	n = convS2M(&r->request, r->rpc, m->msize);
	if(n < 0)
		panic("bad message type in mountio");

	//---- Send 9P message to the server. ----

	if(rc = (devtab[m->c->type]->write(m->c, r->rpc, n, 0)) != n){
	        PRN(" mountio:dev[]->write():0x%x\n", rc); //%% 
	        ERROR_GOTO(Emountrpc, _ERR_1);               //% 
	}

	r->stime = fastticks(nil);
	r->reqlen = n;

	/* Gate readers onto the mount point one at a time */
	for(;;) {
		lock(&m->_lock);   //% (m)

		if(m->rip == 0)
			break;

		unlock(&m->_lock);   //% (m)
		sleep(&r->r, rpcattn, r);  // (r->done||r->m->rip==0)

		if(r->done){
			POPERROR();

			mntflushfree(m, r);
			l4printf_c(255, " <mountio "); //%%
			return  0;  //%
		}
	}

	m->rip = up;
	unlock(&m->_lock);   //% (m)

	while(r->done == 0) {
	        if(mntrpcread(m, r) < 0){
		        l4printf_c(255, "mountio:Er3"); //%%
		        ERROR_GOTO(Emountrpc, _ERR_1);   //% 
		}

		mountmux(m, r);
	}

	mntgate(m);

	POPERROR();
	mntflushfree(m, r);

	DBGPRN(" <mountio()\n"); //%%
	return  0;
}



static int doread(Mnt *m, int len)
{
	Block *b;

	// l4printf_c(10, ">doread ", m->c->path->s); //%
	while(qlen(m->q) < len){
		b = devtab[m->c->type]->bread(m->c, m->msize, 0);
		if(b == nil){
		        l4printf_c(10, "<doread:Er1 "); //%
			return -1;
		}
		if(blocklen(b) == 0){
			freeblist(b);
			l4printf_c(10, "<doread:Er2 "); //%
			return -1;
		}
		qaddlist(m->q, b);
	}
	// l4printf_c(10, "<doread "); //%
	return 0;
}



int mntrpcread(Mnt *m, Mntrpc *r)
{
	int i, t, len, hlen;
	Block *b, **l, *nb;

	DBGPRN(">%s() ", __FUNCTION__);
	// l4printf_c(254, ">mntrpcread() "); //%%

	r->reply.type = 0;
	r->reply.tag = 0;

	// read at least length, type, and tag and pullup to a single block 
	//  9Pmsg:: { totalsize[4] type[1] tag[2] ...  }

	if(doread(m, BIT32SZ+BIT8SZ+BIT16SZ) < 0){
	        l4printf_c(11, "mntrpcread:Er1 "); //%%
		return -1;
	}

	// make sure the first block has at least n bytes
	nb = pullupqueue(m->q, BIT32SZ+BIT8SZ+BIT16SZ);

	// read in the rest of the message, avoid ridiculous (for now) message sizes
	// 9Pmsg: { len[4] type[1] tag[2] ...}. 'len' is total size.
	len = GBIT32(nb->rp);  

	if(len > m->msize){
		qdiscard(m->q, qlen(m->q));
	        l4printf_c(11, "mntrpcread:Er2<len:%d,msize:%d> ", len, m->msize); //%%
		return -1;
	}

	if(doread(m, len) < 0){
	        l4printf_c(11, "mntrpcread:Er3 "); //%%
		return -1;
	}

	/* pullup the header (i.e. everything except data) 
	 *   { len[4] type[1] tag[2] ........... }
	 *   :<---- len ------------------------>:
	 *   { len[4] Rread   tag[2] count[4] data[count] }
	 *   :<---- hlen ------------------->:
	 */

	t = nb->rp[BIT32SZ];
	switch(t){
	case Rread:
		hlen = BIT32SZ+BIT8SZ+BIT16SZ+BIT32SZ;
		break;
	default:
		hlen = len;
		break;
	}

	// make sure the first block has at least hlen bytes
	nb = pullupqueue(m->q, hlen);

	if(convM2S(nb->rp, len, &r->reply) <= 0){
		/* bad message, dump it */
		print("mntrpcread: convM2S failed\n");
		qdiscard(m->q, len);
		l4printf_c(11, "mntrpcread:Er4 "); //%%
		return -1;
	}

	/* hang the data off of the fcall struct */
	l = &r->b;
	*l = nil;

	do {
		b = qremove(m->q);
		if(hlen > 0){
			b->rp += hlen;
			len -= hlen;
			hlen = 0;
		}

		i = BLEN(b);
		if(i <= len){
			len -= i;
			*l = b;
			l = &(b->next);
		} 
		else {
			/* split block and put unused bit back */
			nb = allocb(i-len);
			memmove(nb->wp, b->rp+len, i-len);

			b->wp = b->rp+len;
			nb->wp += i-len;
			qputback(m->q, nb);

			*l = b;
	                l4printf_c(11, "<mntrpcread:OK1 "); //%%
			return 0;
		}
	}while(len > 0);

	// l4printf_c(11, "<mntrpcread:OK2 "); //%%
	return 0;
}



void mntgate(Mnt *m)
{
	Mntrpc *q;

	DBGPRN(">%s() ", __FUNCTION__);
	lock(&m->_lock);   //% (m)
	m->rip = 0;
	for(q = m->queue; q; q = q->list) {
		if(q->done == 0)
		    if(wakeup(&q->r)) {
		        // l4printf_r("wakup-1(%x) ", q->r);  //%
		        break;
		    }
	}
	unlock(&m->_lock);   //% (m)
}



/*                  q -> . . . 
 *  m -> +-- Mnt --+      +- Mntrpc-+      +- Mntrpc-+
 *       |         |      |         |      |         |
 *       | queue --|----> |  list --|----->| list  --|-->
 *       |         |      |  r      |      | r       |
 *       +---------+      |  done   |      | done    |
 *                        +---------+      +---------+
 */

void mountmux(Mnt *m, Mntrpc *r)
{
	Mntrpc **l, *q;

	DBGPRN(">%s() ", __FUNCTION__);
	lock(&m->_lock);   //% (m)
	l = &m->queue;

	for(q = *l; q; q = q->list) {
		/* look for a reply to a message */
		if(q->request.tag == r->reply.tag) {
		        *l = q->list;  // Remove q-> from the list 

			if(q != r) {
				/*
				 * Completed someone else.
				 * Trade pointers to receive buffer.
				 */
				q->reply = r->reply;
				q->b = r->b;
				r->b = nil;
			}

			q->done = 1;
			unlock(&m->_lock);   //% (m)
			if(mntstats != nil)
				(*mntstats)(q->request.type,
					m->c, q->stime,
					q->reqlen + r->replen);
			if(q != r) {
			        // l4printf_r("wakup-2(%x) ", q->r);  //%
				wakeup(&q->r);
			}
			return;
		}

		l = &q->list;
	}
	unlock(&m->_lock);   //% (m)
	print("unexpected reply tag %ud; type %d\n", r->reply.tag, r->reply.type);
}


/*
 * Create a new flush request and chain the previous
 * requests from it
 */
Mntrpc* mntflushalloc(Mntrpc *r, ulong iounit)
{
	Mntrpc *fr;

	fr = mntralloc(0, iounit);

	fr->request.type = Tflush;
	if(r->request.type == Tflush)
		fr->request.oldtag = r->request.oldtag;
	else
		fr->request.oldtag = r->request.tag;
	fr->flushed = r;

	return fr;
}


/*
 *  Free a chain of flushes.  Remove each unanswered
 *  flush and the original message from the unanswered
 *  request queue.  Mark the original message as done
 *  and if it hasn't been answered set the reply to to
 *  Rflush.
 */
void mntflushfree(Mnt *m, Mntrpc *r)
{
	Mntrpc *fr;

	while(r){
		fr = r->flushed;
		if(!r->done){
			r->reply.type = Rflush;
			mntqrm(m, r);
		}
		if(fr)
			mntfree(r);
		r = fr;
	}
}


int alloctag(void)
{
	int i, j;
	ulong v;

	for(i = 0; i < NMASK; i++){
		v = mntalloc.tagmask[i];
		if(v == ~0UL)
			continue;
		for(j = 0; j < 1<<TAGSHIFT; j++)
			if((v & (1<<j)) == 0){
				mntalloc.tagmask[i] |= 1<<j;
				return (i<<TAGSHIFT) + j;
			}
	}
	panic("no friggin tags left");
	return NOTAG;
}


void freetag(int t)
{
	mntalloc.tagmask[t>>TAGSHIFT] &= ~(1<<(t&TAGMASK));
}


Mntrpc* mntralloc(Chan *c, ulong msize)  //% ONERR
{
	Mntrpc *new;

	lock(&mntalloc._lock);   //%  (&mntalloc)
	new = mntalloc.rpcfree;
	if(new == nil){
		new = malloc(sizeof(Mntrpc));
		if(new == nil) {
			unlock(&mntalloc._lock);   //%  (&mntalloc)
			EXHAUSTED_RETURN("mount rpc header", nil); //%
		}
		/*
		 * The header is split from the data buffer as
		 * mountmux may swap the buffer with another header.
		 */
		new->rpc = mallocz(msize, 0);
		if(new->rpc == nil){
			free(new);
			unlock(&mntalloc._lock);   //%  (&mntalloc)
			EXHAUSTED_RETURN("mount rpc buffer", nil);  //%
		}
		new->rpclen = msize;
		new->request.tag = alloctag(); // 9P <tag> is assigned
	}
	else {
		mntalloc.rpcfree = new->list;
		mntalloc.nrpcfree--;
		if(new->rpclen < msize){
			free(new->rpc);
			new->rpc = mallocz(msize, 0);
			if(new->rpc == nil){
				free(new);
				mntalloc.nrpcused--;
				unlock(&mntalloc._lock);   //%  (&mntalloc)
				EXHAUSTED_RETURN("mount rpc buffer", nil); //%
			}
			new->rpclen = msize;
		}
	}
	mntalloc.nrpcused++;
	unlock(&mntalloc._lock);   //%  (&mntalloc)
	new->c = c;
	new->done = 0;
	new->flushed = nil;
	new->b = nil;
	return new;
}


void mntfree(Mntrpc *r)
{
	if(r->b != nil)
		freeblist(r->b);
	lock(&mntalloc._lock);   //%  (&mntalloc)
	if(mntalloc.nrpcfree >= 10){
		free(r->rpc);
		free(r);
		freetag(r->request.tag);
	}
	else{
		r->list = mntalloc.rpcfree;
		mntalloc.rpcfree = r;
		mntalloc.nrpcfree++;
	}
	mntalloc.nrpcused--;
	unlock(&mntalloc._lock);   //%  (&mntalloc)
}


void mntqrm(Mnt *m, Mntrpc *r)
{
	Mntrpc **l, *f;

	lock(&m->_lock);   //% (m)
	r->done = 1;

	l = &m->queue;
	for(f = *l; f; f = f->list) {
		if(f == r) {
			*l = r->list;
			break;
		}
		l = &f->list;
	}
	unlock(&m->_lock);   //% (m)
}


Mnt* mntchk(Chan *c)
{
	Mnt *m;

	/* This routine is mostly vestiges of prior lives; now it's just sanity checking */

	if(c->mchan == nil)
		panic("mntchk 1: nil mchan c %s\n", chanpath(c));

	m = c->mchan->mux;

	if(m == nil)
		print("mntchk 2: nil mux c %s c->mchan %s \n", chanpath(c), chanpath(c->mchan));

	/*
	 * Was it closed and reused (was error(Eshutdown); now, it cannot happen)
	 */
	if(m->id == 0 || m->id >= c->dev)
		panic("mntchk 3: can't happen");

	return m;
}


/*
 * Rewrite channel type and dev for in-flight data to
 * reflect local values.  These entries are known to be
 * the first two in the Dir encoding after the count.
 */
void mntdirfix(uchar *dirbuf, Chan *c)
{
	uint r;

	r = devtab[c->type]->dc;
	dirbuf += BIT16SZ;	/* skip count */
	PBIT16(dirbuf, r);
	dirbuf += BIT16SZ;
	PBIT32(dirbuf, c->dev);
}


int rpcattn(void *v)
{
	Mntrpc *r;

	r = v;
	return r->done || r->m->rip == 0;
}


Dev mntdevtab = {
	'M',
	"mnt",

	mntreset,
	devinit,
	devshutdown,
	mntattach,
	mntwalk,
	mntstat,
	mntopen,
	mntcreate,
	mntclose,
	mntread,
	devbread,
	mntwrite,
	devbwrite,
	mntremove,
	mntwstat,
};


#if 1 //--------------------------------
void pr_rpc(int type)
{
    char *p;
    switch(type) {
    case Tversion: p = "Tversion"; break;
    case Rversion: p = "Rversion"; break;
    case Tauth :   p = "Tauth";    break;
    case Rauth:    p = "Rauth";    break;
    case Tattach:  p = "Tattach";  break;
    case Rattach:  p = "Rattach";  break;
    case Terror :  p = "Terror";   break;
    case Rerror:   p = "Rerror";   break;
    case Tflush :  p = "Tflush";   break;
    case Rflush:   p =  "Rflush";  break;
    case Twalk :   p = "Twalk";    break;
    case Rwalk:    p = "Rwalk";    break;
    case Topen :   p = "Topen";    break;
    case Ropen:    p = "Ropen";    break;
    case Tcreate : p = "Tcreate";  break;
    case Rcreate:  p = "Rcreate";  break;
    case Tread :   p =  "Tread" ;  break;
    case Rread:    p = "Rread";    break;
    case Twrite :  p = "Twrite";   break;
    case Rwrite:   p = "Rwrite";   break;
    case Tclunk :  p = "Tclunk";   break;
    case Rclunk:   p = "Rclunk";   break;
    case Tremove : p = "Tremove";  break;
    case Rremove:  p = "Rremove";  break;
    case Tstat :   p = "Tstat";    break;
    case Rstat:    p = "Rstat";    break; 
    case Twstat :  p = "Twstat";   break;
    case Rwstat:   p = "Rwstat";   break;
    default:  p ="ErCmnd";
    }
    l4printf_c(3, "%s ", p);
} 
#endif //%
