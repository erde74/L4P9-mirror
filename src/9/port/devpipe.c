//%%%%%%%%%% devpipe.c %%%%%%%%%%%%%%%%%
#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

#include	"../port/netif.h"

//%begin------------------------------------------
#include        "../errhandler-l4.h"

#define    _DBGFLG   0
#include   <l4/_dbgpr.h>
#define    print   l4printf

extern int l4printf(const char *fmt, ...);
extern int l4printf_r(const char *fmt, ...);
extern int l4printf_c(int color, const char *fmt, ...);

extern void dump_mem(char *title, unsigned char *start, unsigned size);
extern void dbg_dump_mem(char *title, unsigned char *start, unsigned size);
//%end------------------------------------------- 

typedef struct Pipe	Pipe;
struct Pipe
{
        QLock   _qlock;    //%
	Pipe	*next;
	int	ref;
	ulong	path;
	Queue	*q[2];
	int	qref[2];
};

struct
{
        Lock    _lock;  //%
	ulong	path;
} pipealloc;

enum
{
	Qdir,
	Qdata0,
	Qdata1,
};

Dirtab pipedir[] =
{
        {".",		{Qdir,0,QTDIR},	0,		DMDIR|0500},
	{"data",	{Qdata0},	0,		0600},
	{"data1",	{Qdata1},	0,		0600},
};
#define NPIPEDIR 3

static void pipeinit(void)
{
	if(conf.pipeqsize == 0){
		if(conf.nmach > 1)
			conf.pipeqsize = 256*1024;
		else
			conf.pipeqsize = 32*1024;
	}
}

/*
 *  create a pipe, no streams are created until an open
 */
static Chan* pipeattach(char *spec)
{
	Pipe *p;
	Chan *c;

	DBGPRN(">%s(%s) ", __FUNCTION__, spec); //_backtrace_();

	c = devattach('|', spec);

	p = malloc(sizeof(Pipe));
	if(p == 0)
		exhausted("memory");
	p->ref = 1;

	p->q[0] = qopen(conf.pipeqsize, 0, 0, 0);
	if(p->q[0] == 0){
		free(p);
		exhausted("memory");
	}
	p->q[1] = qopen(conf.pipeqsize, 0, 0, 0);
	if(p->q[1] == 0){
		free(p->q[0]);
		free(p);
		exhausted("memory");
	}

	lock(&pipealloc._lock);        //% &pipealloc
	p->path = ++pipealloc.path;
	unlock(&pipealloc._lock);      //% &pipealloc

	mkqid(&c->qid, NETQID(2*p->path, Qdir), 0, QTDIR);
	c->_u.aux = p;    //% _u.
	c->dev = 0;

	DBGPRN("<%s( ):%s ", __FUNCTION__, c->path->s);
	return c;
}

static int pipegen(Chan *c, char* _x, Dirtab *tab, int ntab, int i, Dir *dp)
{
	Qid q;
	int len;
	Pipe *p;

	if(i == DEVDOTDOT){
		devdir(c, c->qid, "#|", 0, eve, DMDIR|0555, dp);
		return 1;
	}
	i++;	/* skip . */
	if(tab==0 || i>=ntab)
		return -1;

	tab += i;
	p = c->_u.aux;   //%  _u.
	switch((ulong)tab->qid.path){
	case Qdata0:
		len = qlen(p->q[0]);
		break;
	case Qdata1:
		len = qlen(p->q[1]);
		break;
	default:
		len = tab->length;
		break;
	}
	mkqid(&q, NETQID(NETID(c->qid.path), tab->qid.path), 0, QTFILE);
	devdir(c, q, tab->name, len, eve, tab->perm, dp);
	return 1;
}


static Walkqid* pipewalk(Chan *c, Chan *nc, char **name, int nname)
{
	Walkqid *wq;
	Pipe *p;

	DBGPRN(">%s() ", __FUNCTION__);
	wq = devwalk(c, nc, name, nname, pipedir, NPIPEDIR, pipegen);
	if(wq != nil && wq->clone != nil && wq->clone != c){
	        p = c->_u.aux;     //% \u.
		qlock(&p->_qlock);   //% p
		p->ref++;
		if(c->flag & COPEN){
			print("channel open in pipewalk\n");
			switch(NETTYPE(c->qid.path)){
			case Qdata0:
				p->qref[0]++;
				break;
			case Qdata1:
				p->qref[1]++;
				break;
			}
		}
		qunlock(&p->_qlock);  //% p
	}
	return wq;
}

static int pipestat(Chan *c, uchar *db, int n)  //% ONERR
{
	Pipe *p;
	Dir dir;

	DBGPRN(">%s() ", __FUNCTION__);
	p = c->_u.aux;   //% \u.

	switch(NETTYPE(c->qid.path)){
	case Qdir:
		devdir(c, c->qid, ".", 0, eve, DMDIR|0555, &dir);
		break;
	case Qdata0:
		devdir(c, c->qid, "data", qlen(p->q[0]), eve, 0600, &dir);
		break;
	case Qdata1:
		devdir(c, c->qid, "data1", qlen(p->q[1]), eve, 0600, &dir);
		break;
	default:
		panic("pipestat");
	}
	n = convD2M(&dir, db, n);
	if(n < BIT16SZ)
	        ERROR_RETURN(Eshortstat, ONERR); //%
	return n;
}

/*
 *  if the stream doesn't exist, create it
 */
static Chan* pipeopen(Chan *c, int omode)  //% ONERR 
{
	Pipe *p;

	DBGPRN(">%s(%s %x) ", __FUNCTION__, c->path->s, omode);
	if(c->qid.type & QTDIR){
		if(omode != OREAD)
		        ERROR_RETURN(Ebadarg, nil);  //%
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	p = c->_u.aux;       //% _u.
	qlock(&p->_qlock);   //% p
	switch(NETTYPE(c->qid.path)){
	case Qdata0:
		p->qref[0]++;
		break;
	case Qdata1:
		p->qref[1]++;
		break;
	}
	qunlock(&p->_qlock);  //% p

	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	c->iounit = qiomaxatomic;
	return c;
}

static void pipeclose(Chan *c)
{
	Pipe *p;

	DBGPRN(">%s() ", __FUNCTION__);
	p = c->_u.aux;       //% _u.
	qlock(&p->_qlock);   //% p

	if(c->flag & COPEN){
		/*
		 *  closing either side hangs up the stream
		 */
		switch(NETTYPE(c->qid.path)){
		case Qdata0:
			p->qref[0]--;
			if(p->qref[0] == 0){
				qhangup(p->q[1], 0);
				qclose(p->q[0]);
			}
			break;
		case Qdata1:
			p->qref[1]--;
			if(p->qref[1] == 0){
				qhangup(p->q[0], 0);
				qclose(p->q[1]);
			}
			break;
		}
	}


	/*
	 *  if both sides are closed, they are reusable
	 */
	if(p->qref[0] == 0 && p->qref[1] == 0){
		qreopen(p->q[0]);
		qreopen(p->q[1]);
	}

	/*
	 *  free the structure on last close
	 */
	p->ref--;
	if(p->ref == 0){
	        qunlock(&p->_qlock);  //% p
		free(p->q[0]);
		free(p->q[1]);
		free(p);
	} else
		qunlock(&p->_qlock);  //% p
}

static void _DYY_(char *ss, int rc)
{  int i, j; char ch;
  if (rc <= 0) 
    { l4printf_r(" piperead():%d\n", rc); return; }

  dump_mem("T", ss, (rc<32)?rc:32);
  for (i=0; i<rc; i++){
    if (ss[i] == 'T') {
      l4printf_r(" piperead() ss[%d]==T <- %x :", i, up->thread.raw);
      for (j=0; j<rc && j <80; j++)  
	if ((ch=ss[j]) >= ' ' && ch <= '}' ) 
	  l4printf_r("%c",ch);
      l4printf_r("\n");
      return;
    }
  }
}

static long piperead(Chan *c, void *va, long n, vlong _x) //% ONERR
{
	Pipe *p;

	DBGPRN(">%s(%s %x %d) ", __FUNCTION__, c->path->s, va, n);

	p = c->_u.aux;    //% _u.



	switch(NETTYPE(c->qid.path)){
	case Qdir:
	        // l4printf_c(5, "%d|D(%x,%d)? ", p->path, va, n); //% %s c->path->s 

		return devdirread(c, va, n, pipedir, NPIPEDIR, pipegen);
	case Qdata0:
	  {     //%
	        int nn;
	        // l4printf_c(5, "%d|0(%x,%d)? ", p->path, va, n); //%  %s c->path->s 

		nn = qread(p->q[0], va, n);  // ONERR
		//  _DYY_(va, nn); //%
		DBGPRN("< %s(%s %x %d)-> %d \n", 
		       __FUNCTION__, c->path->s, va, n, nn);
		return  nn; 
	  }
	case Qdata1:
	  {     //%
	        int  nn;
	        // l4printf_c(5, "%d|1(%x,%d)? ", p->path, va, n); //%  %s c->path->s 

		nn = qread(p->q[1], va, n);  // ONERR
		//  _DYY_(va, nn); //%
		DBGPRN("< %s(%s %x %d)-> %d \n", 
		       __FUNCTION__, c->path->s, va, n, nn);
		return nn;
	  }
	default:
		panic("piperead");
	}
	return -1;	/* not reached */
}

static Block* pipebread(Chan *c, long n, ulong offset) //% ONERR
{
	Pipe *p;

	DBGPRN(">%s(%s %d %d) ", __FUNCTION__, c->path->s, n, offset);
	p = c->_u.aux;    //% _u.

	switch(NETTYPE(c->qid.path)){
	case Qdata0:
	        // l4printf_c(5, "%d|0(%d)? ", p->path, n); //%  %s c->path->s 

	        return qbread(p->q[0], n); // NIL ON ERR
	case Qdata1:
	        // l4printf_c(5, "%d|1(%d)? ", p->path, n); //%  %s c->path->s 

		return qbread(p->q[1], n); // NIL ON ERR
	}

	return devbread(c, n, offset);
}

/*
 *  a write to a closed pipe causes a note to be sent to
 *  the process.
 */
static long  pipewrite(Chan *c, void *va, long n, vlong _x) //% ONERR
{
	Pipe *p;
	
	uchar *vb = (uchar*)va;         //%
	uint   tsz = GBIT32(vb);        //%
	if(0 & (tsz>8216)){                   //%
	  dbg_dump_mem("pipe", va, 32); //%
	  L4_KDB_Enter("pipewrite");    //%
	}                               //%

	DBGPRN(">%s(%s %x %d) ", __FUNCTION__, c->path->s, va, n);
	//  l4printf_b("> %s(%s %x %d)\n", __FUNCTION__, c->path->s, va, n);
	//% if(!islo())
	//%	print("pipewrite hi %lux\n", getcallerpc(&c));

	if(WASERROR()) {   //%
	_ERR_1:
		/* avoid notes when pipe is a mounted queue */
		if((c->flag & CMSG) == 0)
			postnote(up, 1, "sys: write on closed pipe", NUser);
		NEXTERROR_RETURN(ONERR);   //% 
	}

	p = c->_u.aux;    //% _u.


	switch(NETTYPE(c->qid.path)){
	case Qdata0:
	            if(0 & (tsz>0x2000))
		    {  l4printf_c(6, "%d|0(%x:%x,%x)! ", p->path, tsz, va, n); //%
		        // va = va - 0x1000;
		    }
	        n = qwrite(p->q[1], va, n); //% ONERR
		break;

	case Qdata1:
	            if(0 & (tsz>0x2000))
		    {  l4printf_c(6, "%d|0(%x:%x,%x)! ", p->path, tsz, va, n); //%
		        // va = va - 0x1000;
		    }

	        n = qwrite(p->q[0], va, n); //% ONERR
		break;

	default:
		panic("pipewrite");
	}

	POPERROR();  //% 
	return n;
}

static long  pipebwrite(Chan *c, Block *bp, ulong _x) //% ONERR
{
	long n;
	Pipe *p;

	DBGPRN(">%s() ", __FUNCTION__);
	if(WASERROR()) {
	_ERR_1:
		/* avoid notes when pipe is a mounted queue */
		if((c->flag & CMSG) == 0)
			postnote(up, 1, "sys: write on closed pipe", NUser);
		NEXTERROR_RETURN(ONERR);  //% 
	}

	p = c->_u.aux;    //% _u.

	switch(NETTYPE(c->qid.path)){
	case Qdata0:
	        // l4printf_c(6, "%d|0(%d:blk)! ", p->path, BLEN(bp)); //%

	        n = qbwrite(p->q[1], bp); //% ONERR
		break;

	case Qdata1:
	        // l4printf_c(6, "%d|1(%d:blk)! ", p->path, BLEN(bp)); //%

	        n = qbwrite(p->q[0], bp); //% ONERR
		break;

	default:
		n = 0;
		panic("pipebwrite");
	}

	POPERROR();  //%
	return n;
}

Dev pipedevtab = {
	'|',
	"pipe",

	devreset,
	pipeinit,
	devshutdown,
	pipeattach,
	pipewalk,
	pipestat,
	pipeopen,
	devcreate,
	pipeclose,
	piperead,
	pipebread,
	pipewrite,
	pipebwrite,
	devremove,
	devwstat,
};
