#define LP49  1   //%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

//%-----------------
#include        "../errhandler-l4.h"   //%
#define   _DBGFLG  0
#include  <l4/_dbgpr.h>


enum
{
	Maxenvsize = 16300,
};

static Egrp	*envgrp(Chan *c);
static int	envwriteable(Chan *c);

static Egrp	confegrp;	/* global environment group containing the kernel configuration */

static Evalue*
envlookup(Egrp *eg, char *name, ulong qidpath)
{
	Evalue *e;
	int i;

	for(i=0; i<eg->nent; i++){
		e = eg->ent[i];
		if(e->qid.path == qidpath || (name && e->name[0]==name[0] && strcmp(e->name, name) == 0))
			return e;
	}
	return nil;
}

static int
envgen(Chan *c, char *name, Dirtab* _x, int _y, int s, Dir *dp)
{
	Egrp *eg;
	Evalue *e;

	if(s == DEVDOTDOT){
		devdir(c, c->qid, "#e", 0, eve, DMDIR|0775, dp);
		return 1;
	}

	eg = envgrp(c);
	rlock(& eg->_rwlock); //%
	e = 0;
	if(name)
		e = envlookup(eg, name, -1);
	else if(s < eg->nent)
		e = eg->ent[s];

	if(e == 0) {
	  runlock(& eg->_rwlock);  //%
		return -1;
	}

	/* make sure name string continues to exist after we release lock */
	kstrcpy(up->genbuf, e->name, sizeof up->genbuf);
	devdir(c, e->qid, up->genbuf, e->len, eve, 0666, dp);
	runlock(& eg->_rwlock);  //%
	return 1;
}

static Chan*
envattach(char *spec)  //% ONERR nil
{
	Chan *c;
	Egrp *egrp = nil;

	if(spec && *spec) {
		if(strcmp(spec, "c") == 0)
			egrp = &confegrp;
		if(egrp == nil)
		        ERROR_RETURN(Ebadarg, nil);   //%
	}

	c = devattach('e', spec);
	c->_u.aux = egrp;  //%
	return c;
}

static Walkqid*
envwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, 0, 0, envgen);
}

static int
envstat(Chan *c, uchar *db, int n)
{
	if(c->qid.type & QTDIR)
		c->qid.vers = envgrp(c)->vers;
	return devstat(c, db, n, 0, 0, envgen);
}

static Chan*
envopen(Chan *c, int omode)  //% ONERR nil
{
DBGPRN(">%s(%s, %x) ", __FUNCTION__, c->path->s, omode);
	Egrp *eg;
	Evalue *e;
	int trunc;

	eg = envgrp(c);
	if(c->qid.type & QTDIR) {
		if(omode != OREAD)
		        ERROR_RETURN(Eperm, nil);  //%
	}
	else {
		trunc = omode & OTRUNC;
		if(omode != OREAD && !envwriteable(c))
			ERROR_RETURN(Eperm, nil);  //%
		if(trunc)
		  wlock(& eg->_rwlock);  //%
		else
		  rlock(& eg->_rwlock);  //%
		e = envlookup(eg, nil, c->qid.path);
		if(e == 0) {
			if(trunc)
			  wunlock(& eg->_rwlock); //%
			else
			  runlock(& eg->_rwlock); //%
			ERROR_RETURN(Enonexist, nil);  //%
		}
		if(trunc && e->value) {
			e->qid.vers++;
			free(e->value);
			e->value = 0;
			e->len = 0;
		}
		if(trunc)
		  wunlock(& eg->_rwlock);  //%
		else
		  runlock(& eg->_rwlock); //%
	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static int   //%  void
envcreate(Chan *c, char *name, int omode, ulong _x)  //% ONERR
{
DBGPRN(">%s(%s, %s, %x) ", __FUNCTION__, c->path->s, name, omode);
	Egrp *eg;
	Evalue *e;
	Evalue **ent;

	if(c->qid.type != QTDIR)
	        ERROR_RETURN(Eperm, ONERR);  //%

	omode = openmode(omode);
	eg = envgrp(c);

	wlock(& eg->_rwlock);  //%
	if(WASERROR()) {           //%
	_ERR_1:
	        wunlock(& eg->_rwlock);  //%
		NEXTERROR_RETURN(ONERR);  //%
	}

	if(envlookup(eg, name, -1))
	        ERROR_GOTO(Eexist, _ERR_1);   //%

	e = smalloc(sizeof(Evalue));
	e->name = smalloc(strlen(name)+1);
	strcpy(e->name, name);

	if(eg->nent == eg->ment){
		eg->ment += 32;
		ent = smalloc(sizeof(eg->ent[0])*eg->ment);
		if(eg->nent)
			memmove(ent, eg->ent, sizeof(eg->ent[0])*eg->nent);
		free(eg->ent);
		eg->ent = ent;
	}
	e->qid.path = ++eg->path;
	e->qid.vers = 0;
	eg->vers++;
	eg->ent[eg->nent++] = e;
	c->qid = e->qid;

	wunlock(& eg->_rwlock);  //%
	POPERROR();   //%

	c->offset = 0;
	c->mode = omode;
	c->flag |= COPEN;
}

static int   //% void
envremove(Chan *c)    //% ONERR
{
	int i;
	Egrp *eg;
	Evalue *e;

	if(c->qid.type & QTDIR)
	        ERROR_RETURN(Eperm, ONERR);  //%

	eg = envgrp(c);
	wlock(& eg->_rwlock);  //%
	e = 0;
	for(i=0; i<eg->nent; i++){
		if(eg->ent[i]->qid.path == c->qid.path){
			e = eg->ent[i];
			eg->nent--;
			eg->ent[i] = eg->ent[eg->nent];
			eg->vers++;
			break;
		}
	}
	wunlock(& eg->_rwlock);  //%
	if(e == 0)
	        ERROR_RETURN(Enonexist, ONERR);  //%
	free(e->name);
	if(e->value)
		free(e->value);
	free(e);
}

static void
envclose(Chan *c)
{
	/*
	 * cclose can't fail, so errors from remove will be ignored.
	 * since permissions aren't checked,
	 * envremove can't not remove it if its there.
	 */
	if(c->flag & CRCLOSE)
		envremove(c);
}

static long
envread(Chan *c, void *a, long n, vlong off)   //% ONERR
{
DBGPRN(">%s(%s) ", __FUNCTION__, c->path->s);
	Egrp *eg;
	Evalue *e;
	ulong offset = off;

	if(c->qid.type & QTDIR)
		return devdirread(c, a, n, 0, 0, envgen);

	eg = envgrp(c);
	rlock(& eg->_rwlock);  //%
	e = envlookup(eg, nil, c->qid.path);
	if(e == 0) {
		runlock(& eg->_rwlock);
		ERROR_RETURN(Enonexist, ONERR);
	}

	if(offset > e->len)	/* protects against overflow converting vlong to ulong */
		n = 0;
	else if(offset + n > e->len)
		n = e->len - offset;
	if(n <= 0)
		n = 0;
	else
		memmove(a, e->value+offset, n);
	runlock(& eg->_rwlock);  //%
	return n;
}

static long
envwrite(Chan *c, void *a, long n, vlong off)  //% ONERR
{
DBGPRN(">%s(%s, %s, %x) ", __FUNCTION__, c->path->s, a, off);
	char *s;
	int vend;
	Egrp *eg;
	Evalue *e;
	ulong offset = off;

	if(n <= 0)
		return 0;

	vend = offset+n;
	if(vend > Maxenvsize)
	  ERROR_RETURN(Etoobig, ONERR);

	eg = envgrp(c);
	wlock(& eg->_rwlock);  //%
	e = envlookup(eg, nil, c->qid.path);
	if(e == 0) {
	        wunlock(& eg->_rwlock);  //%
		ERROR_RETURN(Enonexist, ONERR);  //%
	}

	if(vend > e->len) {
		s = smalloc(offset+n);
		if(e->value){
			memmove(s, e->value, e->len);
			free(e->value);
		}
		e->value = s;
		e->len = vend;
	}
	memmove(e->value+offset, a, n);
	e->qid.vers++;
	eg->vers++;
	wunlock(& eg->_rwlock);  //%
	return n;
}

Dev envdevtab = {
	'e',
	"env",

	devreset,
	devinit,
	devshutdown,
	envattach,
	envwalk,
	envstat,
	envopen,
	envcreate,
	envclose,
	envread,
	devbread,
	envwrite,
	devbwrite,
	envremove,
	devwstat,
};

void
envcpy(Egrp *to, Egrp *from)
{
	int i;
	Evalue *ne, *e;

	rlock(& from->_rwlock);  //%
	to->ment = (from->nent+31)&~31;
	to->ent = smalloc(to->ment*sizeof(to->ent[0]));
	for(i=0; i<from->nent; i++){
		e = from->ent[i];
		ne = smalloc(sizeof(Evalue));
		ne->name = smalloc(strlen(e->name)+1);
		strcpy(ne->name, e->name);
		if(e->value){
			ne->value = smalloc(e->len);
			memmove(ne->value, e->value, e->len);
			ne->len = e->len;
		}
		ne->qid.path = ++to->path;
		to->ent[i] = ne;
	}
	to->nent = from->nent;
	runlock(& from->_rwlock);  //%
}

void
closeegrp(Egrp *eg)
{
	int i;
	Evalue *e;

	if(decref(& eg->_ref ) == 0){    //%
		for(i=0; i<eg->nent; i++){
			e = eg->ent[i];
			free(e->name);
			if(e->value)
				free(e->value);
			free(e);
		}
		free(eg->ent);
		free(eg);
	}
}

static Egrp*
envgrp(Chan *c)
{
  if(c->_u.aux == nil)  //%
		return up->egrp;
  return c->_u.aux;  //%
}

static int
envwriteable(Chan *c)
{
  return iseve() || c->_u.aux == nil;  //%
}

/*
 *  to let the kernel set environment variables
 */
void
ksetenv(char *ename, char *eval, int conf)
{
	Chan *c;
	char buf[2*KNAMELEN];
	
	snprint(buf, sizeof(buf), "#e%s/%s", conf?"c":"", ename);
	c = namec(buf, Acreate, OWRITE, 0600);
	devtab[c->type]->write(c, eval, strlen(eval), 0);
	cclose(c);
}

/*
 * Return a copy of configuration environment as a sequence of strings.
 * The strings alternate between name and value.  A zero length name string
 * indicates the end of the list
 */
char *
getconfenv(void)  //% ONERR nil	
{
	Egrp *eg = &confegrp;
	Evalue *e;
	char *p, *q;
	int i, n;

	rlock(& eg->_rwlock);  //%
	if(WASERROR()) {      //%
	_ERR_1:
	        runlock(& eg->_rwlock);  //%
		NEXTERROR_RETURN(nil);   //%
	}
	
	/* determine size */
	n = 0;
	for(i=0; i<eg->nent; i++){
		e = eg->ent[i];
		n += strlen(e->name) + e->len + 2;
	}
	p = malloc(n + 1);
	if(p == nil)
	        ERROR_GOTO(Enomem, _ERR_1);  //%
	q = p;
	for(i=0; i<eg->nent; i++){
		e = eg->ent[i];
		strcpy(q, e->name);
		q += strlen(q) + 1;
		memmove(q, e->value, e->len);
		q[e->len] = 0;
		/* move up to the first null */
		q += strlen(q) + 1;
	}
	*q = 0;
	
	POPERROR();         //%
	runlock(& eg->_rwlock); //%
	return p;
}
