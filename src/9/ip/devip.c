/*
 * (C) Bell Lab
 * (C2) YS, KM	@NII
 */
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"
#include	"../ip/ip.h"

#if 1 //%-----------------------------------------
#include   "../errhandler-l4.h"

#define   _DBGFLG 0
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b
extern  void _prncpy(char*, char*, int); 
extern  char *_dbg_names(char **names, int num);
extern  void check_clock(int which, char *msg);
#endif //%----------------------------------------

//% -- Move from pc/pcf-l4.c --------------------
extern void ilinit(Fs*);
extern void tcpinit(Fs*);
extern void udpinit(Fs*);
extern void ipifcinit(Fs*);
extern void icmpinit(Fs*);
extern void icmp6init(Fs*);
void (*ipprotoinit[])(Fs*) = {
	ilinit,
	tcpinit,
	udpinit,
	ipifcinit,
	icmpinit,
	icmp6init,
	nil,
};
//% ------------------------------------------------

enum
{
	Qtopdir=	1,		/* top level directory */
	Qtopbase,
	Qarp=		Qtopbase,
	Qbootp,
	Qndb,
	Qiproute,
	Qipselftab,
	Qlog,

	Qprotodir,			/* directory for a protocol */
	Qprotobase,
	Qclone=		Qprotobase,
	Qstats,

	Qconvdir,			/* directory for a conversation */
	Qconvbase,
	Qctl=		Qconvbase,
	Qdata,
	Qerr,
	Qlisten,
	Qlocal,
	Qremote,
	Qstatus,
	Qsnoop,

	Logtype=	5,
	Masktype=	(1<<Logtype)-1,
	Logconv=	12,
	Maskconv=	(1<<Logconv)-1,
	Shiftconv=	Logtype,
	Logproto=	8,
	Maskproto=	(1<<Logproto)-1,
	Shiftproto=	Logtype + Logconv,

	Nfs=		128,
};
/* QID(p,c,y)
 *           +--- 8 ----+--- 12 --------+-- 5 --+ 
 *           | protocol | conversation  | type  |
 *           +----------+---------------+-------+
 */
#define TYPE(x) 	( ((ulong)(x).path) & Masktype )
#define CONV(x) 	( (((ulong)(x).path) >> Shiftconv) & Maskconv )
#define PROTO(x) 	( (((ulong)(x).path) >> Shiftproto) & Maskproto )
#define QID(p, c, y) 	( ((p)<<(Shiftproto)) | ((c)<<Shiftconv) | (y) )

static char network[] = "network";

QLock	fslock;
Fs	*ipfs[Nfs];	/* attached fs's */
Queue	*qlog;

extern	void nullmediumlink(void);
extern	void pktmediumlink(void);
	long ndbwrite(Fs *f, char *a, ulong off, int n);

/*    ipfs         Fs          Proto                  Conv
 *    +------+    +------+    +------+               +------+
 *    |  :   |    |      |    |      |    +-----+    |      |
 *    |    --|--> |p[ ]--|--> | conv-|--->|  :--|--> |      |
 *    |  :   |    |      |    |      |    |  :  |    |      |
 *    +------+    |      |    |      |    +-----+    |      |
 *                +------+    +------+               +------+
 */

/*   /net/<protocl>/<n>
 *                 /<n>/data
 *                     /ctl
 *                     /local
 *                     /remote
 *                     /status
 *                     /listen
 *
 *  i(=type); {Qctl, Qdata, Qerr, Qlisten, Qlocal, Qremote, Qstatus,,}
 *  Dir: { "ctl"/etc , qid:{QTFILE, {proto, conv, Qctl/etc }},  }
 */

static int ip3gen(Chan *c, int i, Dir *dp) // i: type(Qctl, Qdata, Qerr, Qlisten,,,,)
{
	Qid q;
	Conv *cv;
	char *p;

	cv = ipfs[c->dev]->p[PROTO(c->qid)]->conv[CONV(c->qid)];
	if(cv->owner == nil)
		kstrdup(&cv->owner, eve);
	mkqid(&q, QID(PROTO(c->qid), CONV(c->qid), i), 0, QTFILE);

	switch(i) {
	default:
		return -1;
	case Qctl:
		devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qdata:
		devdir(c, q, "data", qlen(cv->rq), cv->owner, cv->perm, dp);
		return 1;
	case Qerr:
		devdir(c, q, "err", qlen(cv->eq), cv->owner, cv->perm, dp);
		return 1;
	case Qlisten:
		devdir(c, q, "listen", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qlocal:
		p = "local";
		break;
	case Qremote:
		p = "remote";
		break;
	case Qsnoop:
		if(strcmp(cv->p->name, "ipifc") != 0)
			return -1;
		devdir(c, q, "snoop", qlen(cv->sq), cv->owner, 0400, dp);
		return 1;
	case Qstatus:
		p = "status";
		break;
	}
	devdir(c, q, p, 0, cv->owner, 0444, dp);
	return 1;
}


/*   /net/<protocl>/clone
 *                 /stats
 *                 /<n> 
 *
 *  i (=type):  {Qclone, Qstats}
 *  Dir: {"clone"/"stats", qid:{QTFILE, {proto, 0, Qclone/Qstatus }},,, }
 */

static int ip2gen(Chan *c, int i, Dir *dp)   // i(=type): {Qclone, Qstats} 
{
	Qid q;

	switch(i) {
	case Qclone:
		mkqid(&q, QID(PROTO(c->qid), 0, Qclone), 0, QTFILE);
		devdir(c, q, "clone", 0, network, 0666, dp);
		return 1;
	case Qstats:
		mkqid(&q, QID(PROTO(c->qid), 0, Qstats), 0, QTFILE);
		devdir(c, q, "stats", 0, network, 0444, dp);
		return 1;
	}	
	return -1;
}


/*   /net/arp
 *       /log
 *       /ndb
 *       /iproute
 *       /ipselftab 
 *
 *  i(=type): {Qarp, Qbootp, Qndb, Qiproute, Qipselftab, Qlog}
 *  Dir: {"arp"/etc, qid:{QTFILE, {0, 0, Qarp/etc  }},,, }
 */

static int ip1gen(Chan *c, int i, Dir *dp) // i: {Qarp, Qbootp, Qndb, Qiproute,,,}
{
	Qid q;
	char *p;
	int prot;
	int len = 0;
	Fs *f;
	//%	extern ulong	kerndate;

	f = ipfs[c->dev];

	prot = 0666;
	mkqid(&q, QID(0, 0, i), 0, QTFILE);
	switch(i) {
	default:
		return -1;
	case Qarp:
		p = "arp";
		break;
	case Qbootp:
		p = "bootp";
		break;
	case Qndb:
		p = "ndb";
		len = strlen(f->ndb);
		q.vers = f->ndbvers;
		break;
	case Qiproute:
		p = "iproute";
		break;
	case Qipselftab:
		p = "ipselftab";
		prot = 0444;
		break;
	case Qlog:
		p = "log";
		break;
	}
	devdir(c, q, p, len, network, prot, dp);
//%	if(i == Qndb && f->ndbmtime > kerndate)	//% not implemented kerndate and seconds() yet
//%		dp->mtime = f->ndbmtime;	//% not implemented kerndate and seconds() yet
	return 1;
}


static int ipgen(Chan *c, char* _x, Dirtab* _y, int _z, int s, Dir *dp)  //%
{
	Qid q;
	Conv *cv;
	Fs *f;

	f = ipfs[c->dev];

	switch(TYPE(c->qid)) {
	case Qtopdir:
		if(s == DEVDOTDOT){
			mkqid(&q, QID(0, 0, Qtopdir), 0, QTDIR);
			sprint(up->genbuf, "#I%lud", c->dev);
			devdir(c, q, up->genbuf, 0, network, 0555, dp);
			// Dir:{ "#I?", qid:{QTDIR, {0, 0, Qtopdir }},,, }
			return 1;
		}

		if(s < f->np) {
			if(f->p[s]->connect == nil)
				return 0;	/* protocol with no user interface */
			mkqid(&q, QID(s, 0, Qprotodir), 0, QTDIR);
			devdir(c, q, f->p[s]->name, 0, network, 0555, dp);
			// Dir:{ f->p[s]->name, qid:{QTDIR, {s, 0, Qtopdir }},,, }
			return 1;
		}

		s -= f->np;
		return ip1gen(c, s+Qtopbase, dp);
		//Dir: { "arp" etc., qid:{QTFILE, {0, 0, s+Qtopbase}},,,}


	case Qarp:
	case Qbootp:
	case Qndb:
	case Qlog:
	case Qiproute:
	case Qipselftab:
		return ip1gen(c, TYPE(c->qid), dp);
		//Dir: { "arp" etc., qid:{QTFILE, {0, 0, TYPE(c->qid)}},,,}


	case Qprotodir:
		if(s == DEVDOTDOT){
			mkqid(&q, QID(0, 0, Qtopdir), 0, QTDIR);
			sprint(up->genbuf, "#I%lud", c->dev);
			devdir(c, q, up->genbuf, 0, network, 0555, dp);
			// Dir:{ "I?", qid:{QTDIR, {0, 0, Qtopdir }},,, }
			return 1;
		}

		if(s < f->p[PROTO(c->qid)]->ac) {
			cv = f->p[PROTO(c->qid)]->conv[s];
			sprint(up->genbuf, "%d", s);
			mkqid(&q, QID(PROTO(c->qid), s, Qconvdir), 0, QTDIR);
			devdir(c, q, up->genbuf, 0, cv->owner, 0555, dp);
			// Dir:{ N, qid:{QTDIR, {proto, s, Qconvdir }},,, }
			return 1;
		}

		s -= f->p[PROTO(c->qid)]->ac;
		return ip2gen(c, s+Qprotobase, dp);
		//Dir: {"clone" or "stats", qid:{QTFILE, {proto, 0, s+Qprotobase}},,,}


	case Qclone:
	case Qstats:
		return ip2gen(c, TYPE(c->qid), dp);
		//Dir: {"clone" or "stats", qid:{QTFILE, {proto, 0, TYPE(c->qid)}},,,}

	case Qconvdir:
		if(s == DEVDOTDOT){
			s = PROTO(c->qid);
			mkqid(&q, QID(s, 0, Qprotodir), 0, QTDIR);
			devdir(c, q, f->p[s]->name, 0, network, 0555, dp);
			// Dir:{f->p[s]->name, qid:{QTDIR, {s, 0, Qprotodir }},,,}
			return 1;
		}
		return ip3gen(c, s+Qconvbase, dp);
		//Dir:{"ctl" etc. , qid:{QTFILE, {proto, conv, s+Qconvbase}},,,}

	case Qctl:
	case Qdata:
	case Qerr:
	case Qlisten:
	case Qlocal:
	case Qremote:
	case Qstatus:
	case Qsnoop:
		return ip3gen(c, TYPE(c->qid), dp);
		//Dir:{"ctl"etc. , qid:{QTFILE, {proto, conv, TYPE(c->qid)}},,,}
	}
	return -1;
}

static void ipreset(void)
{
DBGPRN(">%s()\n", __FUNCTION__);
	nullmediumlink();
	pktmediumlink();

	fmtinstall('i', eipfmt);
	fmtinstall('I', eipfmt);
	fmtinstall('E', eipfmt);
	fmtinstall('V', eipfmt);
	fmtinstall('M', eipfmt);
DBGPRN("< %s()\n", __FUNCTION__);
}

static Fs* ipgetfs(int dev)
{
	extern void (*ipprotoinit[])(Fs*);
	Fs *f;
	int i;

	if(dev >= Nfs)
		return nil;

	qlock(&fslock);
	if(ipfs[dev] == nil){
		f = smalloc(sizeof(Fs));
		ip_init(f);
		arpinit(f);
		netloginit(f);
		for(i = 0; ipprotoinit[i]; i++)
			ipprotoinit[i](f);
		f->dev = dev;
		ipfs[dev] = f;
	}
	qunlock(&fslock);

	return ipfs[dev];
}

IPaux* newipaux(char *owner, char *tag)
{
	IPaux *a;
	int n;

	a = smalloc(sizeof(*a));
	kstrdup(&a->owner, owner);
	memset(a->tag, ' ', sizeof(a->tag));
	n = strlen(tag);
	if(n > sizeof(a->tag))
		n = sizeof(a->tag);
	memmove(a->tag, tag, n);
	return a;
}


#define ATTACHER(c) (((IPaux*)((c)->_u.aux))->owner) //% 


//% Example use;  # bind -a #I0 net 
static Chan* ipattach(char* spec)     //% ONERR
{
	Chan *c;
	int dev;

DBGPRN(">%s('%s')\n", __FUNCTION__, spec);
	dev = atoi(spec);
	if(dev >= Nfs)
	        ERROR_RETURN("bad specification", nil);  //%

	ipgetfs(dev);
	c = devattach('I', spec);
	mkqid(&c->qid, QID(0, 0, Qtopdir), 0, QTDIR);
	c->dev = dev;

	c->_u.aux = newipaux(commonuser(), "none");	//% _u
DBGPRN("< %s()\n", __FUNCTION__);

	return c;
}


static Walkqid* ipwalk(Chan* c, Chan *nc, char **name, int nname)
{
DBGPRN(">%s('%s' '%s' %s)\n", __FUNCTION__, c->path->s, nc->path->s, _dbg_names(name, nname));
	IPaux *a = c->_u.aux;		//% _u
	Walkqid* w;

	w = devwalk(c, nc, name, nname, nil, 0, ipgen);
	if(w != nil && w->clone != nil)
		w->clone->_u.aux = newipaux(a->owner, a->tag);	  //% _u
	return w;
}


static int ipstat(Chan* c, uchar* db, int n)
{
	return devstat(c, db, n, nil, 0, ipgen);
}


static int incoming(void* arg)
{
	Conv *conv;

	conv = arg;
	return conv->incall != nil;
}

static int m2p[] = {
	[OREAD]		4,
	[OWRITE]	2,
	[ORDWR]		6
};


static Chan* ipopen(Chan* c, int omode)   //% nil ONERR
{
	Conv *cv, *nc;
	Proto *p;
	int perm;
	Fs *f;

DBGPRN(">%s('%s' %x)\n", __FUNCTION__, c->path->s, omode);
	perm = m2p[omode&3];

	f = ipfs[c->dev];

	switch(TYPE(c->qid)) {
	default:
		break;
	case Qndb:
		if(omode & (OWRITE|OTRUNC) && !iseve())
		        ERROR_RETURN(Eperm, nil);  //%
		if((omode & (OWRITE|OTRUNC)) == (OWRITE|OTRUNC))
			f->ndb[0] = 0;
		break;

	case Qlog:
		netlogopen(f);
		break;

	case Qiproute:
		break;

	case Qtopdir:
	case Qprotodir:
	case Qconvdir:
	case Qstatus:
	case Qremote:
	case Qlocal:
	case Qstats:
	case Qbootp:
	case Qipselftab:
		if(omode != OREAD)
			ERROR_RETURN(Eperm, nil);  //%error(Eperm);
		break;

	case Qsnoop:
		if(omode != OREAD)
			ERROR_RETURN(Eperm, nil);  //%error(Eperm);
		p = f->p[PROTO(c->qid)];
		cv = p->conv[CONV(c->qid)];
		if(strcmp(ATTACHER(c), cv->owner) != 0 && !iseve())
			ERROR_RETURN(Eperm, nil);  //%error(Eperm);
		incref(&cv->snoopers);
		break;

	case Qclone:
		p = f->p[PROTO(c->qid)];
		qlock(&p->_qlock);			//% _qlock

		if(WASERROR()){   //%
		_ERR_1:
			qunlock(&p->_qlock);		//% _qlock
			NEXTERROR_RETURN(nil);    //%
		}
		cv = Fsprotoclone(p, ATTACHER(c));
		if (cv == nil) goto _ERR_1;   //%% Added

		qunlock(&p->_qlock);		//% _qlock
		POPERROR();  //%

		if(cv == nil) {
		        ERROR_RETURN(Enodev, nil);  //%
			break;
		}
		mkqid(&c->qid, QID(p->x, cv->x, Qctl), 0, QTFILE);
		break;

	case Qdata:
	case Qctl:
	case Qerr:
		p = f->p[PROTO(c->qid)];
		qlock(&p->_qlock);			//% _qlock
		cv = p->conv[CONV(c->qid)];
		qlock(&cv->_qlock);			//% _qlock

		if(WASERROR()) {  //%
		_ERR_2:
		        qunlock(&cv->_qlock);	        //% _qlock
			qunlock(&p->_qlock);		//% _qlock
			NEXTERROR_RETURN(nil);   //%
		}

		if((perm & (cv->perm>>6)) != perm) {
			if(strcmp(ATTACHER(c), cv->owner) != 0)
			        ERROR_GOTO(Eperm, _ERR_2);  //%
		 	if((perm & cv->perm) != perm)
				ERROR_GOTO(Eperm, _ERR_2);  //%error(Eperm); 

		}
		cv->inuse++;
		if(cv->inuse == 1){
			kstrdup(&cv->owner, ATTACHER(c));
			cv->perm = 0660;
		}
		qunlock(&cv->_qlock);			//% _qlock
		qunlock(&p->_qlock);			//% _qlock
		POPERROR();   //% _ERR_2

		break;

	case Qlisten:
		cv = f->p[PROTO(c->qid)]->conv[CONV(c->qid)];
		if((perm & (cv->perm>>6)) != perm) {
			if(strcmp(ATTACHER(c), cv->owner) != 0)
			        ERROR_RETURN(Eperm, nil);  //%
		 	if((perm & cv->perm) != perm)
				ERROR_RETURN(Eperm, nil);  //%error(Eperm); 

		}

		if(cv->state != Announced)
		        ERROR_RETURN("not announced", nil); //%

		if(WASERROR()){  //%
		_ERR_3:
			closeconv(cv);
			NEXTERROR_RETURN(nil);   //%
		}
		qlock(&cv->_qlock);			//% _qlock
		cv->inuse++;
		qunlock(&cv->_qlock);			//% _qlock

		nc = nil;
		while(nc == nil) {
			/* give up if we got a hangup */
			if(qisclosed(cv->rq))
			        ERROR_GOTO("listen hungup", _ERR_3); //%

			qlock(&cv->listenq);
			if(WASERROR()) {  //%
			_ERR_31:
				qunlock(&cv->listenq);
				NEXTERROR_GOTO(_ERR_3);   //%
			}

			/* wait for a connect */
			sleep(&cv->listenr, incoming, cv);

			qlock(&cv->_qlock);		//% _qlock
			nc = cv->incall;
			if(nc != nil){
				cv->incall = nc->next;
				mkqid(&c->qid, QID(PROTO(c->qid), nc->x, Qctl), 0, QTFILE);
				kstrdup(&cv->owner, ATTACHER(c));
			}
			qunlock(&cv->_qlock);		//% _qlock

			qunlock(&cv->listenq);
			POPERROR();  //% _ERR_31
		}
		closeconv(cv);
		POPERROR();  //% _ERR_3
		break;
	}

	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}


static Chan* ipcreate(Chan* _w, char* _x, int _y, ulong _z) //% ONERR <- void
{
        ERROR_RETURN(Eperm, nil);  //%
}


static int ipremove(Chan* _x)  //% ONERR <- void
{
	ERROR_RETURN(Eperm, ONERR);  //%ERROR(Eperm);
}


static int ipwstat(Chan *c, uchar *dp, int n)   //% ONERR
{
	Dir d;
	Conv *cv;
	Fs *f;
	Proto *p;

	f = ipfs[c->dev];
	switch(TYPE(c->qid)) {
	default:
	        ERROR_RETURN(Eperm, ONERR);  //%
		break;
	case Qctl:
	case Qdata:
		break;
	}

	n = convM2D(dp, n, &d, nil);
	if(n > 0){
		p = f->p[PROTO(c->qid)];
		cv = p->conv[CONV(c->qid)];
		if(!iseve() && strcmp(ATTACHER(c), cv->owner) != 0)
			ERROR_RETURN(Eperm, ONERR);  //%
		if(d.uid[0])
			kstrdup(&cv->owner, d.uid);
		cv->perm = d.mode & 0777;
	}
	return n;
}


void closeconv(Conv *cv)
{
	Conv *nc;
	Ipmulti *mp;

	qlock(&cv->_qlock);			//% _qlock

	if(--cv->inuse > 0) {
		qunlock(&cv->_qlock);		//% _qlock
		return;
	}

	/* close all incoming calls since no listen will ever happen */
	for(nc = cv->incall; nc; nc = cv->incall){
		cv->incall = nc->next;
		closeconv(nc);
	}
	cv->incall = nil;

	kstrdup(&cv->owner, network);
	cv->perm = 0660;

	while((mp = cv->multi) != nil)
		ipifcremmulti(cv, mp->ma, mp->ia);

	cv->r = nil;
	cv->rgen = 0;
	cv->p->close(cv);
	cv->state = Idle;
	qunlock(&cv->_qlock);			//% _qlock
}


static int ipclose(Chan* c)  //% int <- void
{
	Fs *f;

	f = ipfs[c->dev];
	switch(TYPE(c->qid)) {
	default:
		break;
	case Qlog:
		if(c->flag & COPEN)
			netlogclose(f);
		break;
	case Qdata:
	case Qctl:
	case Qerr:
		if(c->flag & COPEN)
			closeconv(f->p[PROTO(c->qid)]->conv[CONV(c->qid)]);
		break;
	case Qsnoop:
		if(c->flag & COPEN)
			decref(&f->p[PROTO(c->qid)]->conv[CONV(c->qid)]->snoopers);
		break;
	}
	free(((IPaux*)c->_u.aux)->owner);	//% _u
	free(c->_u.aux);			//% _u
	return  1;  //% added
}

enum
{
	Statelen=	32*1024,
};



static long ipread(Chan *ch, void *a, long n, vlong off)   //% ONERR
{
	Conv *c;
	Proto *x;
	char *buf, *p;
	long rv;
	Fs *f;
	ulong offset = off;
	int   cnt;

        // DPB(">%s('%s')\n", __FUNCTION__, ch->path->s); //%

	f = ipfs[ch->dev];

	p = a;
	switch(TYPE(ch->qid)) {
	default:
	        ERROR_RETURN(Eperm, ONERR);  //%
	case Qtopdir:
	case Qprotodir:
	case Qconvdir:
		return devdirread(ch, a, n, 0, 0, ipgen);

	case Qarp:
		return arpread(f->arp, a, offset, n);

 	case Qbootp:
 		return bootpread(a, offset, n);

 	case Qndb:
		return readstr(offset, a, n, f->ndb);

	case Qiproute:
		return routeread(f, a, offset, n);

	case Qipselftab:
		return ipselftabread(f, a, offset, n);

	case Qlog:
		return netlogread(f, a, offset, n);

	case Qctl:
		buf = smalloc(16);
		sprint(buf, "%lud", CONV(ch->qid));
		rv = readstr(offset, p, n, buf);
		free(buf);
		return rv;

	case Qremote:
		buf = smalloc(Statelen);
		x = f->p[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		if(x->remote == nil) {
			sprint(buf, "%I!%d\n", c->raddr, c->rport);
		} else {
			(*x->remote)(c, buf, Statelen-2);
		}
		rv = readstr(offset, p, n, buf);
		free(buf);
		return rv;

	case Qlocal:
		buf = smalloc(Statelen);
		x = f->p[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		if(x->local == nil) {
			sprint(buf, "%I!%d\n", c->laddr, c->lport);
		} else {
			(*x->local)(c, buf, Statelen-2);
		}
		rv = readstr(offset, p, n, buf);
		free(buf);
		return rv;

	case Qstatus:
		buf = smalloc(Statelen);
		x = f->p[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		(*x->state)(c, buf, Statelen-2);
		rv = readstr(offset, p, n, buf);
		free(buf);
		return rv;

	case Qdata:
	        // PRN("ipread-data\n");  //%
		c = f->p[PROTO(ch->qid)]->conv[CONV(ch->qid)];

		check_clock('i', ">ipread"); //%
		cnt = qread(c->rq, a, n);  //%
		check_clock('i', "<ipread"); //%
		return cnt;                //% 

	case Qerr:
		c = f->p[PROTO(ch->qid)]->conv[CONV(ch->qid)];
		return qread(c->eq, a, n);

	case Qsnoop:
		c = f->p[PROTO(ch->qid)]->conv[CONV(ch->qid)];
		return qread(c->sq, a, n);

	case Qstats:
		x = f->p[PROTO(ch->qid)];
		if(x->stats == nil)
		        ERROR_RETURN("stats not implemented", ONERR);  //%
		buf = smalloc(Statelen);
		(*x->stats)(x, buf, Statelen);
		rv = readstr(offset, p, n, buf);
		free(buf);
		return rv;
	}
}


static Block* ipbread(Chan* ch, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	Fs *f;

	switch(TYPE(ch->qid)){
	case Qdata:
		f = ipfs[ch->dev];
		x = f->p[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		return qbread(c->rq, n);
	default:
		return devbread(ch, n, offset);
	}
}

/*
 *  set local address to be that of the ifc closest to remote address
 */
static void setladdr(Conv* c)
{
	findlocalip(c->p->f, c->laddr, c->raddr);
}

/*
 *  set a local port making sure the quad of raddr,rport,laddr,lport is unique
 */
char* setluniqueport(Conv* c, int lport)
{
	Proto *p;
	Conv *xp;
	int x;

	p = c->p;

	qlock(&p->_qlock);			//% _qlock
	for(x = 0; x < p->nc; x++){
		xp = p->conv[x];
		if(xp == nil)
			break;
		if(xp == c)
			continue;
		if((xp->state == Connected || xp->state == Announced)
		&& xp->lport == lport
		&& xp->rport == c->rport
		&& ipcmp(xp->raddr, c->raddr) == 0
		&& ipcmp(xp->laddr, c->laddr) == 0){
			qunlock(&p->_qlock);		//% _qlock
			return "address in use";
		}
	}
	c->lport = lport;
	qunlock(&p->_qlock);				//% _qlock
	return nil;
}


/*
 *  pick a local port and set it
 */
void setlport(Conv* c)
{
	Proto *p;
	ushort *pp;
	int x, found;

	p = c->p;
	if(c->restricted)
		pp = &p->nextrport;
	else
		pp = &p->nextport;
	qlock(&p->_qlock);				//% _qlock
	for(;;(*pp)++){
		/*
		 * Fsproto initialises p->nextport to 0 and the restricted
		 * ports (p->nextrport) to 600.
		 * Restricted ports must lie between 600 and 1024.
		 * For the initial condition or if the unrestricted port number
		 * has wrapped round, select a random port between 5000 and 1<<15
		 * to start at.
		 */
		if(c->restricted){
			if(*pp >= 1024)
				*pp = 600;
		}
		else while(*pp < 5000)
			*pp = nrand(1<<15);

		found = 0;
		for(x = 0; x < p->nc; x++){
			if(p->conv[x] == nil)
				break;
			if(p->conv[x]->lport == *pp){
				found = 1;
				break;
			}
		}
		if(!found)
			break;
	}
	c->lport = (*pp)++;
	qunlock(&p->_qlock);				//% _qlock
}

/*
 *  set a local address and port from a string of the form
 *	[address!]port[!r]
 */
char* setladdrport(Conv* c, char* str, int announcing)  //% nil ONERR
{
	char *p;
	char *rv;
	ushort lport;
	uchar addr[IPaddrlen];

	rv = nil;

	/*
	 *  ignore restricted part if it exists.  it's
	 *  meaningless on local ports.
	 */
	p = strchr(str, '!');
	if(p != nil){
		*p++ = 0;
		if(strcmp(p, "r") == 0)
			p = nil;
	}

	c->lport = 0;
	if(p == nil){
		if(announcing)
			ipmove(c->laddr, IPnoaddr);
		else
			setladdr(c);
		p = str;
	} else {
		if(strcmp(str, "*") == 0)
			ipmove(c->laddr, IPnoaddr);
		else {
			parseip(addr, str);
			if(ipforme(c->p->f, addr))
				ipmove(c->laddr, addr);
			else
				return "not a local IP address";
		}
	}

	/* one process can get all connections */
	if(announcing && strcmp(p, "*") == 0){
		if(!iseve())
		        ERROR_RETURN(Eperm, nil); //%
		return setluniqueport(c, 0);
	}

	lport = atoi(p);
	if(lport <= 0)
		setlport(c);
	else
		rv = setluniqueport(c, lport);
	return rv;
}


static char* setraddrport(Conv* c, char* str)
{
	char *p;

	p = strchr(str, '!');
	if(p == nil)
		return "malformed address";
	*p++ = 0;
	parseip(c->raddr, str);
	c->rport = atoi(p);
	p = strchr(p, '!');
	if(p){
		if(strstr(p, "!r") != nil)
			c->restricted = 1;
	}
	return nil;
}


/*
 *  called by protocol connect routine to set addresses
 */
char* Fsstdconnect(Conv *c, char *argv[], int argc)
{
	char *p;

	switch(argc) {
	default:
		return "bad args to connect";
	case 2:
		p = setraddrport(c, argv[1]);
		if(p != nil)
			return p;
		setladdr(c);
		setlport(c);
		break;
	case 3:
		p = setraddrport(c, argv[1]);
		if(p != nil)
			return p;
		p = setladdrport(c, argv[2], 0);
		if(p != nil)
			return p;
	}

	if( (memcmp(c->raddr, v4prefix, IPv4off) == 0 &&
		memcmp(c->laddr, v4prefix, IPv4off) == 0)
		|| ipcmp(c->raddr, IPnoaddr) == 0)
		c->ipversion = V4;
	else
		c->ipversion = V6;

	return nil;
}

/*
 *  initiate connection and sleep till its set up
 */
static int connected(void* a)
{
	return ((Conv*)a)->state == Connected;
}

static int  connectctlmsg(Proto *x, Conv *c, Cmdbuf *cb)  //% ONERR <- void
{
	char *p;

DBGPRN(">%s() \n", __FUNCTION__);
	if(c->state != 0)
	        ERROR_RETURN(Econinuse, ONERR);  //%
	c->state = Connecting;
	c->cerr[0] = '\0';
	if(x->connect == nil)
	        ERROR_RETURN("connect not supported", ONERR);  //%
	p = x->connect(c, cb->f, cb->nf);
	if(p != nil)
	        ERROR_RETURN(p, ONERR);  //%

	qunlock(&c->_qlock);				//% _qlock

	if(WASERROR()){  //%
	_ERR_1:
		qlock(&c->_qlock);			//% _qlock
		NEXTERROR_RETURN(ONERR);   //%
	}
	sleep(&c->cr, connected, c);
	qlock(&c->_qlock);				//% _qlock
	POPERROR();   //%

	if(c->cerr[0] != '\0'){ 
DBGPRN("%s:'%s'\n", __FUNCTION__, c->cerr); //% DBG
	        ERROR_RETURN(c->cerr, ONERR);   //%
	}
	return   1; //% added
DBGPRN("<%s():%s \n", __FUNCTION__, c->cerr);
}

/*
 *  called by protocol announce routine to set addresses
 */
char* Fsstdannounce(Conv* c, char* argv[], int argc)
{
	memset(c->raddr, 0, sizeof(c->raddr));
	c->rport = 0;
	switch(argc){
	default:
		break;
	case 2:
		return setladdrport(c, argv[1], 1);
	}
	return "bad args to announce";
}

/*
 *  initiate announcement and sleep till its set up
 */
static int announced(void* a)
{
	return ((Conv*)a)->state == Announced;
}


static int announcectlmsg(Proto *x, Conv *c, Cmdbuf *cb)  //% ONERR  <- void
{
	char *p;

	if(c->state != 0)
	        ERROR_RETURN(Econinuse, ONERR);  //%
	c->state = Announcing;
	c->cerr[0] = '\0';
	if(x->announce == nil)
	        ERROR_RETURN("announce not supported", ONERR);  //%
	p = x->announce(c, cb->f, cb->nf);
	if(p != nil)
	        ERROR_RETURN(p, ONERR); //%

	qunlock(&c->_qlock);				//% _qlock

	if(WASERROR()){
	_ERR_1:
		qlock(&c->_qlock);			//% _qlock
		NEXTERROR_RETURN(ONERR);  //%
	}
	sleep(&c->cr, announced, c);
	qlock(&c->_qlock);				//% _qlock
	POPERROR();   //%

	if(c->cerr[0] != '\0')
	        ERROR_RETURN(c->cerr, ONERR);  //%
	return  1;  //% added
}


/*
 *  called by protocol bind routine to set addresses
 */
char* Fsstdbind(Conv* c, char* argv[], int argc)
{
DBGPRN("> %s(%x  %s)\n", __FUNCTION__, c, _dbg_names(argv, argc));
	switch(argc){
	default:
		break;
	case 2:
		return setladdrport(c, argv[1], 0);
	}
	return "bad args to bind";
}

static int  bindctlmsg(Proto *x, Conv *c, Cmdbuf *cb)  //% ONERR <- void
{
DBGPRN("> %s(%s %x %x) x->binx:%x\n", __FUNCTION__, x->name, c, cb, x->bind);
	char *p;

	if(x->bind == nil)
		p = Fsstdbind(c, cb->f, cb->nf);
	else
		p = x->bind(c, cb->f, cb->nf);
	if(p != nil)
	        ERROR_RETURN(p, ONERR);  //%
	return  1; //% added
DBGPRN("< %s()\n", __FUNCTION__);
}

static void tosctlmsg(Conv *c, Cmdbuf *cb)
{
	if(cb->nf < 2)
		c->tos = 0;
	else
		c->tos = atoi(cb->f[1]);
}

static void ttlctlmsg(Conv *c, Cmdbuf *cb)
{
	if(cb->nf < 2)
		c->ttl = MAXTTL;
	else
		c->ttl = atoi(cb->f[1]);
}


static long ipwrite(Chan* ch, void *v, long n, vlong off)   //% ONERR
{
	Conv *c;
	Proto *x;
	char *p;
	Cmdbuf *cb;
	uchar ia[IPaddrlen], ma[IPaddrlen];
	Fs *f;
	char *a;
	ulong offset = off;
	int   rc; //%

//%----------------
char  zz[64];
_prncpy(zz, v, (n<63)?n:63);
DBGPRN(">%s('%s' '%s' %d)\n", __FUNCTION__, ch->path->s, zz, off);
//%-----------------

	a = v;
	f = ipfs[ch->dev];

	switch(TYPE(ch->qid)){
	default:
	        ERROR_RETURN(Eperm, ONERR);  //%
	case Qdata:
	        //  PRN("ipwrite-data\n");  //%
		x = f->p[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];

		if(c->wq == nil)
		        ERROR_RETURN(Eperm, ONERR);  //%
		check_clock('i', ">ipwrite"); //%
		qwrite(c->wq, a, n);
		check_clock('i', "<ipwrite"); //%
		break;

	case Qarp:
		return arpwrite(f, a, n);

	case Qiproute:
		return routewrite(f, ch, a, n);

	case Qlog:
		netlogctl(f, a, n);
		return n;

	case Qndb:
		return ndbwrite(f, a, offset, n);
		break;

	case Qctl:
		x = f->p[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		cb = parsecmd(a, n);

		qlock(&c->_qlock);			//% _qlock
		if(WASERROR()) {    //%
		_ERR_1:
			qunlock(&c->_qlock);		//% _qlock
			free(cb);
			NEXTERROR_RETURN(ONERR);   //%
		}

		if(cb->nf < 1)
		        ERROR_GOTO("short control request", _ERR_1);  //%

		if(strcmp(cb->f[0], "connect") == 0) {
			rc = connectctlmsg(x, c, cb);
			if (rc == ONERR) return ONERR;  //%
		}

		else if(strcmp(cb->f[0], "announce") == 0){
			rc = announcectlmsg(x, c, cb);
			if (rc == ONERR) return ONERR;  //%
		}

		else if(strcmp(cb->f[0], "bind") == 0){
		        rc = bindctlmsg(x, c, cb);
			if (rc == ONERR) return ONERR;  //%
		        //% example use: echo "bind ether ether0" > ipifc/clone
		}

		else if(strcmp(cb->f[0], "ttl") == 0)
			ttlctlmsg(c, cb);

		else if(strcmp(cb->f[0], "tos") == 0)
			tosctlmsg(c, cb);

		else if(strcmp(cb->f[0], "ignoreadvice") == 0)
			c->ignoreadvice = 1;

		else if(strcmp(cb->f[0], "addmulti") == 0){
			if(cb->nf < 2)
			        ERROR_GOTO("addmulti needs interface address", _ERR_1); //%
			if(cb->nf == 2){
				if(!ipismulticast(c->raddr))
				    ERROR_GOTO("addmulti for a non multicast address", _ERR_1); //%
				parseip(ia, cb->f[1]);
				ipifcaddmulti(c, c->raddr, ia);
			} else {
				parseip(ma, cb->f[2]);
				if(!ipismulticast(ma))
				    ERROR_GOTO("addmulti for a non multicast address", _ERR_1); //%
				parseip(ia, cb->f[1]);
				ipifcaddmulti(c, ma, ia);
			}

		} else if(strcmp(cb->f[0], "remmulti") == 0){
			if(cb->nf < 2)
			        ERROR_GOTO("remmulti needs interface address", _ERR_1); //%
			if(!ipismulticast(c->raddr))
			        ERROR_GOTO("remmulti for a non multicast address", _ERR_1); //%
			parseip(ia, cb->f[1]);
			ipifcremmulti(c, c->raddr, ia);

		} else if(x->ctl != nil) {
			p = x->ctl(c, cb->f, cb->nf);
			//% ipifcctl() etc. is called.

			if(p != nil)
			        ERROR_GOTO(p, _ERR_1);  //%
		} else 
		        ERROR_GOTO("unknown control request", _ERR_1); //%
		qunlock(&c->_qlock);			//% _qlock
		free(cb);
		POPERROR();  //% _ERR_1
	}
	return n;
}

static long ipbwrite(Chan* ch, Block* bp, ulong offset)   //% ONERR
{
	Conv *c;
	Proto *x;
	Fs *f;
	int n;

	switch(TYPE(ch->qid)){
	case Qdata:
		f = ipfs[ch->dev];
		x = f->p[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];

		if(c->wq == nil)
		        ERROR_RETURN(Eperm, ONERR); //%

		if(bp->next)
			bp = concatblock(bp);
		n = BLEN(bp);
		qbwrite(c->wq, bp);
		return n;
	default:
		return devbwrite(ch, bp, offset);
	}
}

Dev ipdevtab = {
	'I',
	"ip",

	ipreset,
	devinit,
	devshutdown,
	ipattach,
	ipwalk,
	ipstat,
	ipopen,
	ipcreate,
	ipclose,
	ipread,
	ipbread,
	ipwrite,
	ipbwrite,
	ipremove,
	ipwstat,
};

int Fsproto(Fs *f, Proto *p)
{
	if(f->np >= Maxproto)
		return -1;

	p->f = f;

	if(p->ipproto > 0){
		if(f->t2p[p->ipproto] != nil)
			return -1;
		f->t2p[p->ipproto] = p;
	}

	p->qid.type = QTDIR;
	p->qid.path = QID(f->np, 0, Qprotodir);
	p->conv = malloc(sizeof(Conv*)*(p->nc+1));
	if(p->conv == nil)
		panic("Fsproto");

	p->x = f->np;
	p->nextport = 0;
	p->nextrport = 600;
	f->p[f->np++] = p;

	return 0;
}

/*
 *  return true if this protocol is
 *  built in
 */
int Fsbuiltinproto(Fs* f, uchar proto)
{
	return f->t2p[proto] != nil;
}

/*
 *  called with protocol locked
 */
Conv* Fsprotoclone(Proto *p, char *user)   //% nil ONERR
{
	Conv *c, **pp, **ep;

retry:
	c = nil;
	ep = &p->conv[p->nc];
	for(pp = p->conv; pp < ep; pp++) {
		c = *pp;
		if(c == nil){
			c = malloc(sizeof(Conv));
			if(c == nil)
			        ERROR_RETURN(Enomem, nil);  //%
			qlock(&c->_qlock);		//% _qlock
			c->p = p;
			c->x = pp - p->conv;
			if(p->ptclsize != 0){
				c->ptcl = malloc(p->ptclsize);
				if(c->ptcl == nil) {
					free(c);
					ERROR_RETURN(Enomem, nil);  //%
				}
			}
			*pp = c;
			p->ac++;
			c->eq = qopen(1024, Qmsg, 0, 0);
			(*p->create)(c);
			break;
		}
		if(canqlock(&c->_qlock)){		//% _qlock
			/*
			 *  make sure both processes and protocol
			 *  are done with this Conv
			 */
			if(c->inuse == 0 && (p->inuse == nil || (*p->inuse)(c) == 0))
				break;

			qunlock(&c->_qlock);		//% _qlock
		}
	}
	if(pp >= ep) {
		if(p->gc != nil && (*p->gc)(p))
			goto retry;
		return nil;
	}

	c->inuse = 1;
	kstrdup(&c->owner, user);
	c->perm = 0660;
	c->state = Idle;
	ipmove(c->laddr, IPnoaddr);
	ipmove(c->raddr, IPnoaddr);
	c->r = nil;
	c->rgen = 0;
	c->lport = 0;
	c->rport = 0;
	c->restricted = 0;
	c->ttl = MAXTTL;
	qreopen(c->rq);
	qreopen(c->wq);
	qreopen(c->eq);

	qunlock(&c->_qlock);				//% _qlock
	return c;
}

int Fsconnected(Conv* c, char* msg)
{
	if(msg != nil && *msg != '\0')
		strncpy(c->cerr, msg, ERRMAX-1);

	switch(c->state){

	case Announcing:
		c->state = Announced;
		break;

	case Connecting:
		c->state = Connected;
		break;
	}

	wakeup(&c->cr);
	return 0;
}


Proto* Fsrcvpcol(Fs* f, uchar proto)
{
	if(f->ipmux)
		return f->ipmux;
	else
		return f->t2p[proto];
}


Proto* Fsrcvpcolx(Fs *f, uchar proto)
{
	return f->t2p[proto];
}

/*
 *  called with protocol locked
 */
Conv*  Fsnewcall(Conv *c, uchar *raddr, ushort rport, uchar *laddr, ushort lport, uchar version)
{
	Conv *nc;
	Conv **l;
	int i;

	qlock(&c->_qlock);				//% _qlock
	i = 0;
	for(l = &c->incall; *l; l = &(*l)->next)
		i++;
	if(i >= Maxincall) {
		qunlock(&c->_qlock);			//% _qlock
		return nil;
	}

	/* find a free conversation */
	nc = Fsprotoclone(c->p, network);
	if(nc == nil) {
		qunlock(&c->_qlock);			//% _qlock
		return nil;
	}
	ipmove(nc->raddr, raddr);
	nc->rport = rport;
	ipmove(nc->laddr, laddr);
	nc->lport = lport;
	nc->next = nil;
	*l = nc;
	nc->state = Connected;
	nc->ipversion = version;

	qunlock(&c->_qlock);				//% _qlock

	wakeup(&c->listenr);

	return nc;
}

long ndbwrite(Fs *f, char *a, ulong off, int n)  //% ONERR
{
	if(off > strlen(f->ndb))
	        ERROR_RETURN(Eio, ONERR);  //%
	if(off+n >= sizeof(f->ndb))
		ERROR_RETURN(Eio, ONERR);  //%
	memmove(f->ndb+off, a, n);
	f->ndb[off+n] = 0;
	f->ndbvers++;
	f->ndbmtime = seconds();
	return n;
}

ulong scalednconv(void)
{
	if(cpuserver && conf.npage*BY2PG >= 128*MB)
		return Nchans*4;
	return Nchans;
}
