//%%%%%%%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

#include        "../errhandler-l4.h"  //%

static Ref pgrpid;
static Ref mountid;

int  //% void
pgrpnote(ulong noteid, char *a, long n, int flag)  //% ONERR
{
	Proc *p, *ep;
	char buf[ERRMAX];

	if(n >= ERRMAX-1)
	        ERROR_RETURN(Etoobig, ONERR);  //%

	memmove(buf, a, n);
	buf[n] = 0;
	p = proctab(0);
	ep = p+conf.nproc;
	for(; p < ep; p++) {
		if(p->state == Dead)
			continue;
		if(up != p && p->noteid == noteid && p->kp == 0) {
			qlock(&p->debug);
			if(p->pid == 0 || p->noteid != noteid){
				qunlock(&p->debug);
				continue;
			}
			if(!WASERROR()) {    //%
				postnote(p, 0, buf, flag);
				POPERROR();   //%
			}
			qunlock(&p->debug);
		}
	}
}

Pgrp*
newpgrp(void)
{
	Pgrp *p;

	p = smalloc(sizeof(Pgrp));
	p->_ref.ref = 1;  //% p->ref = 1;
	p->pgrpid = incref(&pgrpid);
	return p;
}

Rgrp*
newrgrp(void)
{
	Rgrp *r;

	r = smalloc(sizeof(Rgrp));
	r->_ref.ref = 1;    //% 
	return r;
}

void
closergrp(Rgrp *r)
{
  if(decref(& r->_ref) == 0)    //% decref(r)  ??
		free(r);
}

void
closepgrp(Pgrp *p)
{
	Mhead **h, **e, *f, *next;

	if(decref(& p->_ref) != 0)  //% decref(p)
		return;

	qlock(&p->debug);
	wlock(&p->ns);
	p->pgrpid = -1;

	e = &p->mnthash[MNTHASH];
	for(h = p->mnthash; h < e; h++) {
		for(f = *h; f; f = next) {
			wlock(&f->lock);
			cclose(f->from);
			mountfree(f->mount);
			f->mount = nil;
			next = f->hash;
			wunlock(&f->lock);
			putmhead(f);
		}
	}
	wunlock(&p->ns);
	qunlock(&p->debug);
	free(p);
}

void
pgrpinsert(Mount **order, Mount *m)
{
	Mount *f;

	m->order = 0;
	if(*order == 0) {
		*order = m;
		return;
	}
	for(f = *order; f; f = f->order) {
		if(m->mountid < f->mountid) {
			m->order = f;
			*order = m;
			return;
		}
		order = &f->order;
	}
	*order = m;
}

/*
 * pgrpcpy MUST preserve the mountid allocation order of the parent group
 */
void
pgrpcpy(Pgrp *to, Pgrp *from)
{
	int i;
	Mount *n, *m, **link, *order;
	Mhead *f, **tom, **l, *mh;

	wlock(&from->ns);
	order = 0;
	tom = to->mnthash;
	for(i = 0; i < MNTHASH; i++) {
		l = tom++;
		for(f = from->mnthash[i]; f; f = f->hash) {
			rlock(&f->lock);
			mh = newmhead(f->from);
			*l = mh;
			l = &mh->hash;
			link = &mh->mount;
			for(m = f->mount; m; m = m->next) {
				n = newmount(mh, m->to, m->mflag, m->spec);
				m->copy = n;
				pgrpinsert(&order, m);
				*link = n;
				link = &n->next;
			}
			runlock(&f->lock);
		}
	}
	/*
	 * Allocate mount ids in the same sequence as the parent group
	 */
	lock(&mountid._lock);    //%  lock(&mountid);
	for(m = order; m; m = m->order)
		m->copy->mountid = mountid.ref++;
	unlock(&mountid._lock);       //%unlock(&mountid);
	wunlock(&from->ns);
}

Fgrp*
dupfgrp(Fgrp *f)  //% ONERR nil
{
	Fgrp *new;
	Chan *c;
	int i;

	new = smalloc(sizeof(Fgrp));
	if(f == nil){
		new->fd = smalloc(DELTAFD*sizeof(Chan*));
		new->nfd = DELTAFD;
		new->_ref.ref = 1;   //% new->ref = 1;  
		return new;
	}

	lock(&f->_ref._lock);     //%  lock(f);  ???
	/* Make new fd list shorter if possible, preserving quantization */
	new->nfd = f->maxfd+1;
	i = new->nfd%DELTAFD;
	if(i != 0)
		new->nfd += DELTAFD - i;
	new->fd = malloc(new->nfd*sizeof(Chan*));
	if(new->fd == nil){
	        unlock(&f->_ref._lock);    //%  unlock(f); 
		free(new);
		ERROR_RETURN("no memory for fgrp", nil);   //%
	}
	new->_ref.ref = 1;   //%  new->ref = 1; 

	new->maxfd = f->maxfd;
	for(i = 0; i <= f->maxfd; i++) {
	        if((c = f->fd[i])){
			incref(&c->_ref);      //%  incref(c)
			new->fd[i] = c;
		}
	}
	unlock(&f->_ref._lock);      //% unlock(f); 

	return new;
}

void
closefgrp(Fgrp *f)
{
	int i;
	Chan *c;

	if(f == 0)
		return;

	if(decref(&f->_ref) != 0)      //% decref(f)
		return;

	/*
	 * If we get into trouble, forceclosefgrp
	 * will bail us out.
	 */
	up->closingfgrp = f;
	for(i = 0; i <= f->maxfd; i++)
		if((c = f->fd[i])){
			f->fd[i] = nil;
			cclose(c);
		}
	up->closingfgrp = nil;

	free(f->fd);
	free(f);
}

/*
 * Called from sleep because up is in the middle
 * of closefgrp and just got a kill ctl message.
 * This usually means that up has wedged because
 * of some kind of deadly embrace with mntclose
 * trying to talk to itself.  To break free, hand the
 * unclosed channels to the close queue.  Once they
 * are finished, the blocked cclose that we've 
 * interrupted will finish by itself.
 */
void
forceclosefgrp(void)
{
	int i;
	Chan *c;
	Fgrp *f;

	if(up->procctl != Proc_exitme || up->closingfgrp == nil){
		print("bad forceclosefgrp call");
		return;
	}

	f = up->closingfgrp;
	for(i = 0; i <= f->maxfd; i++)
		if((c = f->fd[i])){
			f->fd[i] = nil;
			ccloseq(c);
		}
}


Mount*
newmount(Mhead *mh, Chan *to, int flag, char *spec)
{
	Mount *m;

	m = smalloc(sizeof(Mount));
	m->to = to;
	m->head = mh;
	incref(&to->_ref);    //% incref(to);
	m->mountid = incref(&mountid);
	m->mflag = flag;
	if(spec != 0)
		kstrdup(&m->spec, spec);

	return m;
}

void
mountfree(Mount *m)
{
	Mount *f;

	while(m) {
		f = m->next;
		cclose(m->to);
		m->mountid = 0;
		free(m->spec);
		free(m);
		m = f;
	}
}

void
resrcwait(char *reason)
{
	char *p;

	if(up == 0)
		panic("resrcwait");

	p = up->psstate;
	if(reason) {
		up->psstate = reason;
		print("%s\n", reason);
	}

	tsleep(&up->sleep, return0, 0, 300);
	up->psstate = p;
}
