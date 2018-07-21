//%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

#include  "../errhandler-l4.h"

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define    PRN  l4printf_b

extern int l4printf(const char *fmt, ...);
extern int l4printf_r(const char *fmt, ...);
extern int l4printf_g(const char *fmt, ...);
extern int l4printf_b(const char *fmt, ...);
extern int l4printf_s(const char *fmt, ...);


typedef struct Srv Srv;
struct Srv
{
	char	*name;
	char	*owner;
	ulong	perm;
	Chan	*chan;
	Srv	*link;
	ulong	path;
};

static QLock	srvlk;
static Srv	*srv;
static int	qidpath;

static int srvgen(Chan *c, char* _x, Dirtab* _y, int _z, int s, Dir *dp)
{
	Srv *sp;
	Qid q;

	DBGPRN("> %s(\"%s\" %d) ", __FUNCTION__, c->path->s, s);
	if(s == DEVDOTDOT){
		devdir(c, c->qid, "#s", 0, eve, 0555, dp);
		return 1;
	}

	qlock(&srvlk);
	for(sp = srv; sp && s; sp = sp->link)
		s--;

	if(sp == 0) {
		qunlock(&srvlk);
		return -1;
	}

	mkqid(&q, sp->path, 0, QTFILE);
	/* make sure name string continues to exist after we release lock */
	kstrcpy(up->genbuf, sp->name, sizeof up->genbuf);
	devdir(c, q, up->genbuf, 0, sp->owner, sp->perm, dp);
	qunlock(&srvlk);
	return 1;
}

static void srvinit(void)
{
	qidpath = 1;
}

static Chan* srvattach(char *spec)
{
        DBGPRN(">%s() ", __FUNCTION__);
	return devattach('s', spec);
}

static Walkqid* srvwalk(Chan *c, Chan *nc, char **name, int nname)
{
        DBGPRN(">%s(%s) ", __FUNCTION__, c->path->s);
	return devwalk(c, nc, name, nname, 0, 0, srvgen);
}

static Srv* srvlookup(char *name, ulong qidpath)
{
	Srv *sp;
	DBGPRN(">%s(\"%s\" %x) ", __FUNCTION__, name, qidpath);
	for(sp = srv; sp; sp = sp->link)
		if(sp->path == qidpath || (name && strcmp(sp->name, name) == 0))
			return sp;
	return nil;
}

static int srvstat(Chan *c, uchar *db, int n)
{
	return devstat(c, db, n, 0, 0, srvgen);
}

char* srvname(Chan *c)
{
	Srv *sp;
	char *s;

        DBGPRN(">%s() ", __FUNCTION__);
	for(sp = srv; sp; sp = sp->link)
		if(sp->chan == c){
			s = smalloc(3+strlen(sp->name)+1);
			sprint(s, "#s/%s", sp->name);
			return s;
		}
	return nil;
}

static Chan* srvopen(Chan *c, int omode)
{
	Srv *sp;

	DBGPRN(">%s(%s %x) ", __FUNCTION__, c->path->s, omode);
	if(c->qid.type == QTDIR){
		if(omode & ORCLOSE)
		        ERROR_RETURN(Eperm, nil); //% error(Eperm);
		if(omode != OREAD)
		        ERROR_RETURN(Eisdir, nil); //% error(Eisdir);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}
	qlock(&srvlk);
	if(WASERROR()){  //%
	_ERR_1: //%
		qunlock(&srvlk);
		NEXTERROR_RETURN(nil);  //% nexterror(); 
	}

	sp = srvlookup(nil, c->qid.path);
	if(sp == 0 || sp->chan == 0)
	        ERROR_GOTO(Eshutdown, _ERR_1); //% error(Eshutdown);

	if(omode&OTRUNC)
	        ERROR_GOTO("srv file already exists", _ERR_1); //%

	DBGPRN("srvopen<%s: %x != %x && %x != %x>\n", sp->chan->path->s,
	    openmode(omode), sp->chan->mode, sp->chan->mode, ORDWR);  //%

	if(openmode(omode)!=sp->chan->mode && sp->chan->mode!=ORDWR)
	        ERROR_GOTO(Eperm, _ERR_1);  //%
	devpermcheck(sp->owner, sp->perm, omode);

	cclose(c);
	incref(& sp->chan->_ref);  //% <- sp->chan 
	qunlock(&srvlk);
	POPERROR();  //%
	return sp->chan;
}

static Chan* srvcreate(Chan *c, char *name, int omode, ulong perm) //% ONERR <- void
{
	Srv *sp;

	l4printf(">%s(%s %s 0%o 0%o) \n", __FUNCTION__, c->path->s, name, omode, perm);
	if(openmode(omode) != OWRITE){
	        l4printf_b("srvcreate(%s, %s): permission ?\n", c->path->s, name);
	        // ERROR_RETURN(Eperm, nil); //% error(Eperm);
	}

	if(omode & OCEXEC)	/* can't happen */
		panic("someone broke namec");

	sp = malloc(sizeof(Srv)+strlen(name)+1);
	if(sp == 0)
	        ERROR_RETURN(Enomem, nil); //% error(Enomem);

	qlock(&srvlk);
	if(WASERROR()){  //%
	_ERR_1: //%
		free(sp);
		qunlock(&srvlk);
		NEXTERROR_RETURN(nil);  //%
	}
	if(srvlookup(name, -1))
	        ERROR_GOTO(Eexist, _ERR_1); //%

	sp->path = qidpath++;
	sp->link = srv;
	sp->name = (char*)(sp+1);
	strcpy(sp->name, name);
	c->qid.type = QTFILE;
	c->qid.path = sp->path;
	srv = sp;
	qunlock(&srvlk);
	POPERROR();  //%

	kstrdup(&sp->owner, up->user);
	sp->perm = perm&0777;

	c->flag |= COPEN;
	c->mode = OWRITE;
	return  c; //%%%
}

static int  srvremove(Chan *c) //% ONERR<-void
{
	Srv *sp, **l;
	DBGPRN(">%s(%s) ", __FUNCTION__, c->path->s);

	if(c->qid.type == QTDIR)
		error(Eperm);

	qlock(&srvlk);
	if(WASERROR()){
	_ERR_1:
		qunlock(&srvlk);
		NEXTERROR_RETURN(ONERR);  //%
	}
	l = &srv;
	for(sp = *l; sp; sp = sp->link) {
		if(sp->path == c->qid.path)
			break;

		l = &sp->link;
	}
	if(sp == 0)
	        ERROR_GOTO(Enonexist, _ERR_1); //%

	/*
	 * Only eve can remove system services.
	 * No one can remove #s/boot.
	 */
	if(strcmp(sp->owner, eve) == 0 && !iseve())
	        ERROR_GOTO(Eperm, _ERR_1); //%
	if(strcmp(sp->name, "boot") == 0)
	        ERROR_GOTO(Eperm, _ERR_1);  //%

	/*
	 * No removing personal services.
	 */
	if((sp->perm&7) != 7 && strcmp(sp->owner, up->user) && !iseve())
	        ERROR_GOTO(Eperm, _ERR_1); //%

	*l = sp->link;
	qunlock(&srvlk);
	POPERROR();  //%

	if(sp->chan)
		cclose(sp->chan);
	free(sp);
	return  1;  //%
}

static int srvwstat(Chan *c, uchar *dp, int n)
{
	Dir d;
	Srv *sp;

	DBGPRN(">%s(%s) ", __FUNCTION__, c->path->s);
	if(c->qid.type & QTDIR)
	        ERROR_RETURN(Eperm, ONERR);  //%

	qlock(&srvlk);
	if(WASERROR()){ //%
	_ERR_1:
		qunlock(&srvlk);
		NEXTERROR_RETURN(ONERR);  //%
	}

	sp = srvlookup(nil, c->qid.path);
	if(sp == 0)
	        ERROR_GOTO(Enonexist, _ERR_1); //%

	if(strcmp(sp->owner, up->user) && !iseve())
		ERROR_GOTO(Eperm, _ERR_1);

	n = convM2D(dp, n, &d, nil);
	if(n == 0)
	  ERROR_GOTO(Eshortstat, _ERR_1);  //%
	if(d.mode != ~0UL)
		sp->perm = d.mode & 0777;
	if(d.uid && *d.uid)
		kstrdup(&sp->owner, d.uid);

	qunlock(&srvlk);
	POPERROR(); //%
	return n;
}

static int srvclose(Chan *c) //% ONERR <- void
{
	/*
	 * in theory we need to override any changes in removability
	 * since open, but since all that's checked is the owner,
	 * which is immutable, all is well.
	 */
	if(c->flag & CRCLOSE){
	        if(WASERROR()) {  //%
		_ERR_1:
			return 1;
		}
	        srvremove(c);   //% ??? ONERR
		POPERROR();  //%
	}
	return  1;  //%
}

static long srvread(Chan *c, void *va, long n, vlong _x)
{
        DBGPRN(">%s() ", __FUNCTION__);
#if 0 // Plan9
       isdir(c);
#else //%
       int  rc;
       rc = isdir(c);
       IF_ERR_RETURN(rc, ONERR, ONERR);
#endif
	return devdirread(c, va, n, 0, 0, srvgen);
}

static long srvwrite(Chan *c, void *va, long n, vlong _x)
{
	Srv *sp;
	Chan *c1;
	int fd;
	char buf[32];

	DBGPRN(">%s(\"%s\" \"%s\" %d) ", __FUNCTION__, c->path->s, va, n);
	if(n >= sizeof buf)
	        ERROR_RETURN(Egreg, ONERR);  //%
	memmove(buf, va, n);	/* so we can NUL-terminate */
	buf[n] = 0;
	fd = strtoul(buf, 0, 0);

	c1 = fdtochan(fd, -1, 0, 1);	/* error check and inc ref */

	qlock(&srvlk);
	if(WASERROR()) {  //%
	_ERR_1:
		qunlock(&srvlk);
		cclose(c1);
		NEXTERROR_RETURN(ONERR);
	}
	if(c1->flag & (CCEXEC|CRCLOSE))
	        ERROR_GOTO("posted fd has remove-on-close or close-on-exec", _ERR_1); //%
	if(c1->qid.type & QTAUTH)
	        ERROR_GOTO("cannot post auth file in srv", _ERR_1); //%
	sp = srvlookup(nil, c->qid.path);
	if(sp == 0)
	        ERROR_GOTO(Enonexist, _ERR_1); //%

	if(sp->chan)
	        ERROR_GOTO(Ebadusefd, _ERR_1); //%

	sp->chan = c1;
	qunlock(&srvlk);
	POPERROR();  //%
	return n;
}

Dev srvdevtab = {
	's',
	"srv",

	devreset,
	srvinit,	
	devshutdown,
	srvattach,
	srvwalk,
	srvstat,
	srvopen,
	srvcreate,
	srvclose,
	srvread,
	devbread,
	srvwrite,
	devbwrite,
	srvremove,
	srvwstat,
};
