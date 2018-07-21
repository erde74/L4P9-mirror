//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/* (C) Bell lab
 * (C2) NII  KM
 */

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

#include	<u.h>
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"
#include	"../port/edf.h"
#include	<trace.h>

#if 1 //%begin ----------------------------------------
#include        <l4/l4io.h>
#include        <lp49/lp49.h>
 
#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN    l4printf_b

#define  iprint  l4printf  //%
#undef   nexterror
#undef   error

//%  # include "errstr.h"   //% moved to --> errstr.c 
extern  void tobedefined(char *name);
extern L4_ThreadId_t create_start_thread(unsigned pc, unsigned sp);
Proc* newproc_at(int procnr);

//% test code: moved from auth.c --------------------------
char *  eve = "eve";  // auth.c
extern  Clerkjob_t  cjob;

int iseve(void)  // auth.c
{
    return strcmp(eve, up->user) == 0;
}

void eve_procsetup()   // test only
{
    Proc *pp;
    pp = newproc();
    up = pp;
    cjob.pcb = pp;
    up->user = eve;
}
#endif //%end ---------------------------------------

Ref	noteidalloc;
static Ref	pidalloc;

static struct Procalloc
{
        Lock    _lock;   //%
	Proc*	ht[128];
#if 1 //%-------------------------------------------------
        Proc    pcb[PROCNRMAX+1]; // indexed by process-nr.
#else //original //---------------------------------------
	Proc*	arena;
#endif //%-----------------------------------------------
	Proc*	free;
} procalloc;

//%-----------------------------------------------
Proc * procnr2pcb(int procnr)
{
    if (procnr < 0 || procnr > PROCNRMAX) 
         return 0;
    return &procalloc.pcb[procnr];
}

Proc * tid2pcb(L4_ThreadId_t tid)
{
    return &procalloc.pcb[TID2PROCNR(tid)];
}
//%-------------------------------------


char *statename[] =
{	/* BUG: generate automatically */
	"Dead",
	"Moribund",
	"Ready",
	"Scheding",
	"Running",
	"Queueing",
	"QueueingR",
	"QueueingW",
	"Wakeme",
	"Broken",
	"Stopped",
	"Rendez",
	"Waitrelease",
};

static void pidhash(Proc*);
static void pidunhash(Proc*);
//%  static void rebalance(void);


void sched(void)
{        
  //l4printf_b("%s() ", __FUNCTION__); 
        L4_Yield();
}

void clear_pcb(Proc *p)
{
	p->state = 0;
	p->psstate = "New";
	p->mach = 0;
	p->qnext = 0;
	p->nchild = 0;
	p->nwait = 0;
	p->waitq = 0;
	p->parent = 0;
	p->pgrp = 0;
	p->egrp = 0;
	p->fgrp = 0;
	p->rgrp = 0;
	p->pdbg = 0;
	p->fpstate = FPinit;
	p->kp = 0;
	p->procctl = 0;
	p->notepending = 0;
	p->ureg = 0;
	p->privatemem = 0;
	p->noswap = 0;
	p->errstr = p->errbuf0;
	p->syserrstr = p->errbuf1;
	p->errbuf0[0] = '\0';
	p->errbuf1[0] = '\0';
	p->nlocks.ref = 0;
	p->delaysched = 0;
	p->trace = 0;

	kstrdup(&p->user, "*nouser");
	kstrdup(&p->text, "*notext");
	p->args = 0;
	p->nargs = 0;
	p->setargs = 0;

	memset(p->seg, 0, sizeof p->seg);
}

Proc* newproc(void)
{
	Proc *p;
	int   procnr;
	DBGPRN("> newproc( )\n"); //%

	lock(&procalloc._lock);  //% ._lock
	for(;;) {
		if((p = procalloc.free))
			break;

		unlock(&procalloc._lock);  //% ._lock
		resrcwait("no procs");
		lock(&procalloc._lock);  //% ._lock
	}
	procalloc.free = p->qnext;
	unlock(&procalloc._lock);  //% ._lock

#if 1 //%--------------------------------
	procnr = p - procalloc.pcb;
	p->thread = PROCNR2TID(procnr, 0, 2 );  //
#endif //%------------------------------

	clear_pcb(p);

	p->pid = incref(&pidalloc);
	pidhash(p);

	p->noteid = incref(&noteidalloc);
	if(p->pid==0 || p->noteid==0)
		panic("pidalloc");

	DBGPRN("< newproc()=>%x  \n", p); //%
	return p;
}


// procnr must be < PROCNRMIN
Proc* newproc_at(int procnr)
{
        DBGPRN("\n> %s  \n", __FUNCTION__);
	Proc *p;

	//	lock(&procalloc._lock);  //% ._lock
	p = &procalloc.pcb[procnr];
	//	if (p->state != Idle) return 0;
	//	unlock(&procalloc._lock);  //% ._lock
	//	procnr = p - procalloc.pcb;
	p->thread = PROCNR2TID(procnr, 0, 2);  //061208

	clear_pcb(p);

	p->pid = incref(&pidalloc);  
	pidhash(p);
	p->noteid = incref(&noteidalloc); 
	if(p->pid==0 || p->noteid==0)
		panic("pidalloc");
	p->edf = nil;

	return p;
}

void procinit0(void)	//%%
{
        DBGPRN("! %s \n", __FUNCTION__);

	int  i;
	memset(procalloc.pcb, 0, sizeof(procalloc.pcb));
	procalloc.free = &procalloc.pcb[PROCNRMIN];
	for (i = PROCNRMIN; i <= PROCNRMAX-1; i++)
	    procalloc.pcb[i].qnext = &procalloc.pcb[i+1];
}


void sleep(Rendez *r, int (*func)(void*), void *arg)  //% L4 
{
    L4_ThreadId_t  from;
    L4_MsgTag_t    tag;
    if ((*func)(arg) == 0) {
        r->p = (Proc*)L4_Myself().raw;  //Type

	tag = L4_Wait(&from);
	if (L4_IpcFailed(tag)) {
	    l4printf_c(6, "?#%d ", L4_ErrorCode()); //FIXME
	}
    }
}

void sleep_dbg(Rendez *r, int (*func)(void*), void *arg, char *msg) 
{
    l4printf_b(">sleep:%s  ", msg);
    sleep(r, func, arg);
    l4printf_b("<sleep:%s  ", msg);
}


static int isvalidtid(L4_ThreadId_t  tid)
{
    unsigned int version, localnr, procnr;
    procnr = tid.raw >> 24;
    localnr = (tid.raw >> 14) & 0x3ff;
    version = tid.raw & 0x03fff;
    
    if ((procnr != 1) || (version != 2) || (localnr > 64)){  //16
        l4printf_r("wakeup(%x) ", tid.raw);
	return  0;
    }
    return  1;
}

Proc* wakeup(Rendez *r)   //% L4
{
    L4_ThreadId_t whom;
    L4_MsgTag_t   tag;

    if (r->p == nil)  return nil;
 
    whom.raw = (unsigned)r->p;  //Type...
    r->p = nil;

    if (! isvalidtid(whom)) L4_KDB_Enter("wakeup:invalidtid");  //FIXME

    L4_LoadMR(0, 0); //Synchronization only. Shall we add error-checking ? 
    tag = L4_Send(whom);
    if (L4_IpcFailed(tag)) {
        l4printf_c(6, "!#%d ", L4_ErrorCode()); //FIXME
    }
    return  tid2pcb(whom);  
}

Proc* wakeup_dbg(Rendez *r, char *msg)   
{
    Proc *pcb;
    pcb = wakeup(r);
    l4printf_b("!wakeup:%s  ", msg);
    return pcb;
}


int  tsleep(Rendez *r, int (*func)(void*), void *arg, ulong micros)  //% ONERR ?
{
    L4_ThreadId_t  from;
    L4_MsgTag_t   tag;
 
    if ((*func)(arg) == 0) {
        r->p = (Proc*)L4_Myself().raw;  //Type

	tag = L4_Wait_Timeout(L4_TimePeriod(micros), &from );

	r->p = nil; 

	// if (L4_IpcFailed(tag) && L4_ErrorCode()==EXXX) 
	//   return  ONERR;
    }
    return  1;
}


int postnote(Proc *p, int dolock, char *n, int flag)
{
#if 1 //%-----------------------------------
        l4printf_b("%s(): To be defined \n", __FUNCTION__);
	return 0;
#else //plan9------------------------------
        int s, ret;
        Rendez *r;
        Proc *d, **l;
        if(dolock)
	        qlock(&p->debug);
        if(flag != NUser && (p->notify == 0 || p->notified))
	        p->nnote = 0;
        ret = 0;
        if(p->nnote < NNOTE) {
	       strcpy(p->note[p->nnote].msg, n);
	       p->note[p->nnote++].flag = flag;
	       ret = 1;
        }
        p->notepending = 1;
        if(dolock)
	        qunlock(&p->debug);
        /* this loop is to avoid lock ordering problems. */
        for(;;){
	        s = splhi();
		lock(&p->rlock);
		r = p->r;
		/* waiting for a wakeup? */
		if(r == nil)
		        break;  /* no */
		/* try for the second lock */
		if(canlock(r)){
		        if(p->state != Wakeme || r->p != p)
			  panic("postnote: state %d %d %d", r->p != p, p->r != r, p->state);
			p->r = nil;
			r->p = nil;
			ready(p);
			unlock(r);
			break;
		}
		/* give other process time to get out of critical section and try again */
		unlock(&p->rlock);
		splx(s);
		sched();
        }
        unlock(&p->rlock);
        splx(s);
        if(p->state != Rendezvous)
	        return ret;
        /* Try and pull out of a rendezvous */
        lock(p->rgrp);
        if(p->state == Rendezvous) {
	        p->rendval = ~0;
		l = &REND(p->rgrp, p->rendtag);
		for(d = *l; d; d = d->rendhash) {
		        if(d == p) {
			        *l = p->rendhash;
				break;
			}
			l = &d->rendhash;
		}
		ready(p);
        }
        unlock(p->rgrp);
        return ret;
#endif //%---------------------------------
}


void pexit(char *exitstr, int freemem)
{

	Proc *p;
	//%	Segment **s, **es;
	long utime, stime;
	Waitq *wq, *f, *next;
	Fgrp *fgrp;
	Egrp *egrp;
	Rgrp *rgrp;
	Pgrp *pgrp;
	Chan *dot;
	//%	void (*pt)(Proc*, int, vlong);

DBGPRN(">pexit()  ");
	up->alarm = 0;
	if (up->_timer.tt)  //% up->tt
	    timerdel(&up->_timer);  //% up
	//%	pt = proctrace;
	//%	if(pt)
	//%		pt(up, SDead, 0);

	/* nil out all the resources under lock (free later) */
	qlock(&up->debug);
	fgrp = up->fgrp;
	up->fgrp = nil;
	egrp = up->egrp;
	up->egrp = nil;
	rgrp = up->rgrp;
	up->rgrp = nil;
	pgrp = up->pgrp;
	up->pgrp = nil;
	dot = up->dot;
	up->dot = nil;
	qunlock(&up->debug);

	if(fgrp)
		closefgrp(fgrp);
	if(egrp)
		closeegrp(egrp);
	if(rgrp)
		closergrp(rgrp);
	if(dot)
		cclose(dot);
	if(pgrp)
		closepgrp(pgrp);

	/*
	 * if not a kernel process and have a parent,
	 * do some housekeeping.
	 */
	if(1) {  //% orig: if(up->kp == 0) -- LP49 has no kenel processes
		p = up->parent;
		if(p == 0) {
			if(exitstr == 0)
				exitstr = "unknown";
			panic("boot process died: %s", exitstr);
		}

		//%	while(waserror())
		//%		;

		wq = smalloc(sizeof(Waitq));
		//%	poperror();

		wq->w.pid = up->pid;
		utime = up->time[TUser] + up->time[TCUser];
		stime = up->time[TSys] + up->time[TCSys];

		//% wq->w.time[TUser] = tk2ms(utime);
		//% wq->w.time[TSys] = tk2ms(stime);
		//% wq->w.time[TReal] = tk2ms(MACHP(0)->ticks - up->time[TReal]);

		if(exitstr && exitstr[0])
			snprint(wq->w.msg, sizeof(wq->w.msg), "%s %lud: %s", 
				up->text, up->pid, exitstr);
		else
			wq->w.msg[0] = '\0';

		lock(&p->exl);
		/*
		 *  If my parent is no longer alive, or if there would be more
		 *  than 128 zombie child processes for my parent, then don't
		 *  leave a wait record behind.  This helps prevent badly
		 *  written daemon processes from accumulating lots of wait
		 *  records.
		 */
		if(p->pid == up->parentpid && p->state != Broken && p->nwait < 128) {
DBGPRN("pexit<nchild=%d, waitr=%x>  ", p->nchild, p->waitr.p);
			p->nchild--;
			p->time[TCUser] += utime;
			p->time[TCSys] += stime;

			wq->next = p->waitq;
			p->waitq = wq;
			p->nwait++;

			// wakeup_dbg(&p->waitr, "");
			wakeup(&p->waitr);
			unlock(&p->exl);
		}
		else {
			unlock(&p->exl);
			free(wq);
		}
	}

	//%	if(!freemem)
	//%		addbroken(up);

#if 1 //%------------------------------------------------------------
	{
	        L4_ThreadId_t  mx_pager = L4_Pager();
		L4_MsgTag_t  tag;
		int  rc;

		rc = requestThreadControl(up->thread, L4_nilthread, L4_nilthread, 
					  L4_nilthread, -1);
	        L4_LoadMR(1, TID2PROCNR(up->thread) );
		L4_LoadMR(0, MM_FREESPACE << 16 | 1);
		tag = L4_Call(mx_pager);

		up->thread = L4_nilthread;
	}	
#else //% original--------------------------------------------------
	//%	qlock(&up->seglock);
	//%	es = &up->seg[NSEG];
	//%	for(s = up->seg; s < es; s++) {
	//%		if(*s) {
	//%			putseg(*s);
	//%			*s = 0;
	//%		}
	//%	}
	//%	qunlock(&up->seglock);
#endif //%-------------------------------------------------------------

	lock(&up->exl);		/* Prevent my children from leaving waits */
	pidunhash(up);
	up->pid = 0;
	wakeup(&up->waitr);
	unlock(&up->exl);

	for(f = up->waitq; f; f = next) {
		next = f->next;
		free(f);
	}

	/* release debuggers */
	qlock(&up->debug);
	if(up->pdbg) {
		wakeup(&up->pdbg->sleep);
		up->pdbg = 0;
	}
	qunlock(&up->debug);

	/* Sched must not loop for these locks */
	//%	lock(&procalloc._lock);  //% ._lock
	//%	lock(&palloc._lock);     //% ._lock
	//%	edfstop(up);

	up->state = Moribund;
	//%	sched();
	//%	panic("pexit");
DBGPRN("<pexit()  ");
}

int haswaitq(void *x)
{
        DBGPRN(">%s() \n", __FUNCTION__);
	Proc *p;

	p = (Proc *)x;
	return p->waitq != 0;
}

ulong pwait(Waitmsg *w)
{
//l4printf_r("\n>pwait()  ");
	ulong cpid;
	Waitq *wq;

	if(!canqlock(&up->qwaitr)){
	        return  -1;  // error(Einuse);
	}

	lock(&up->exl);
	if(up->nchild == 0 && up->waitq == 0) {
		unlock(&up->exl);
		qunlock(&up->qwaitr);
		l4printf_b("pwait: ERR-1 \n");
		return  -1;	//% error(Enochild);
	}
	unlock(&up->exl);

        // sleep_dbg(&up->waitr, haswaitq, up, "");
	sleep(&up->waitr, haswaitq, up);

	lock(&up->exl);
	wq = up->waitq;
	up->waitq = wq->next;
	up->nwait--;
	unlock(&up->exl);
	qunlock(&up->qwaitr);
	//%	poperror();

	if(w)
		memmove(w, &wq->w, sizeof(Waitmsg));
	cpid = wq->w.pid;
	free(wq);
	//l4printf_r("<pwait<%x, %d> \n", up->waitr.p, cpid);
	return cpid;
}


Proc* proctab(int i)
{
	return &procalloc.pcb[i];
}

void procdump(void)
{        TBD;    }

void scheddump(void)
{        TBD;    }


#if 0 //%
L4_ThreadId_t kproc_l4(char *name, void (*func)(void *), void *arg, unsigned *sp)
{
        DBGPRN("! %s(%s %x %x) \n", __FUNCTION__, name, func, arg);
	L4_ThreadId_t  tid;
	*(--sp) = 0; // no meaning
	*(--sp) = (unsigned)arg;  
	*(--sp) = 0; // Return address ?
	tid = create_start_thread((unsigned)func, (unsigned)sp);
	return  tid;
}
#endif

void kproc(char *name, void (*func)(void *), void *arg)
{
DBGPRN(">%s(%s %x %x) \n", __FUNCTION__, name, func, arg);

	L4_ThreadId_t  tid;
	unsigned * stkbase;
	unsigned      *sp;

	stkbase = (unsigned*)malloc(512*4);   // Only 2 Kbytes
	sp = (unsigned)(stkbase + 500) & 0xfffffff0;  //????

	*(--sp) = 0; // no meaning
	*(--sp) = (unsigned)arg;  
	*(--sp) = 0; // Return address area
	//  l4printf("# SP=%x ARG=%s \n", sp, *(sp+1));
	tid = create_start_thread((unsigned)func, (unsigned)sp);
	L4_ThreadSwitch(tid);
}


void error(char *err)   //% replaced
{
        l4printf_g("!!! %s(%s) [%d]\n", __FUNCTION__, err, up->nerrlab);
}

void error_in(const char *err, const char *func)  //% replaced	// HK 20091031
{
        l4printf_g("!!! error '%s' in %s <%d>\n", err, func, up->nerrlab);
}

void nexterror(void)  //%  replaced
{
        l4printf("!!! %s[%d] \n", __FUNCTION__, up->nerrlab-1);
}

void nexterror_in(const char *__function__) //%---- Added in LP49 ----------------------	// HK 20091031
{
        l4printf("!!! nexterror[*] in %s \n",  __function__);
}

void exhausted(char *resource)
{
        DBGPRN("! %s \n", __FUNCTION__);
	char buf[ERRMAX];

	sprint(buf, "no free %s", resource);
	iprint("%s\n", buf);
	error_in(buf, __FUNCTION__);  //%  error(buf);
}

void killbig(char *why)
{
        DBGPRN("! %s \n", __FUNCTION__);
}

//  change ownership to 'new' of all processes owned by 'old'.  Used when
//  eve changes.
void renameuser(char *old, char *new)
{
        DBGPRN("! %s \n", __FUNCTION__);
	int  i;

	for (i = PROCNRMIN; i <= PROCNRMAX; i++)
	  if ( procalloc.pcb[i].user != nil && 
	       strcmp(old, procalloc.pcb[i].user))
	      kstrdup(&procalloc.pcb[i].user, new);
}

//   time accounting called by clock() splhi'd
void
accounttime(void)
{
        DBGPRN("! %s \n", __FUNCTION__);
}

static void pidhash(Proc *p)
{
        DBGPRN("> %s \n", __FUNCTION__);
	int h;

	h = p->pid % nelem(procalloc.ht);
	lock(&procalloc._lock);  //% ._lock
	p->pidhash = procalloc.ht[h];
	procalloc.ht[h] = p;
	unlock(&procalloc._lock);  //% ._lock
}

static void pidunhash(Proc *p)
{
        DBGPRN("! %s \n", __FUNCTION__);
	int h;
	Proc **l;

	h = p->pid % nelem(procalloc.ht);
	lock(&procalloc._lock);  //% ._lock
	for(l = &procalloc.ht[h]; *l != nil; l = &(*l)->pidhash)
		if(*l == p){
			*l = p->pidhash;
			break;
		}
	unlock(&procalloc._lock);  //% ._lock
}

int procindex(ulong pid)
{
        DBGPRN("! %s \n", __FUNCTION__);
	Proc *p;
	int h;
	int s;

	s = -1;
	h = pid % nelem(procalloc.ht);
	lock(&procalloc._lock);  //% ._lock
	for(p = procalloc.ht[h]; p != nil; p = p->pidhash)
		if(p->pid == pid){
			s = p - procalloc.pcb;
			break;
		}
	unlock(&procalloc._lock);  //% ._lock
	return s;
}


#if 0 //----------------------------------------------------
////////////% Plan9 originals and so on. ////////////

void kproc(char *name, void (*func)(void *), void *arg)
{
	Proc *p;
	static Pgrp *kpgrp;

	p = newproc();
	p->psstate = 0;
	p->procmode = 0640;
	p->kp = 1;
	p->noswap = 1;

	p->fpsave = up->fpsave;
	p->scallnr = up->scallnr;
	p->s = up->s;
	p->nerrlab = 0;
	p->slash = up->slash;
	p->dot = up->dot;
	if(p->dot)
	    incref(&p->dot->_ref);   //% p->dot

	memmove(p->note, up->note, sizeof(p->note));
	p->nnote = up->nnote;
	p->notified = 0;
	p->lastnote = up->lastnote;
	p->notify = up->notify;
	p->ureg = 0;
	p->dbgreg = 0;

	procpriority(p, PriKproc, 0);

	kprocchild(p, func, arg);

	kstrdup(&p->user, eve);
	kstrdup(&p->text, name);
	if(kpgrp == 0)
		kpgrp = newpgrp();
	p->pgrp = kpgrp;
	incref(&kpgrp->_ref);   //% kpgrp

	memset(p->time, 0, sizeof(p->time));
	p->time[TReal] = MACHP(0)->ticks;
	ready(p);
	/*
	 *  since the bss/data segments are now shareable,
	 *  any mmu info about this process is now stale
	 *  and has to be discarded.
	 */
	p->newtlb = 1;
	flushmmu();
}


void procctl(Proc *p)
{
        DBGPRN("! %s \n", __FUNCTION__);
	char *state;
	ulong s;

	switch(p->procctl) {
	case Proc_exitbig:
		spllo();
		pexit("Killed: Insufficient physical memory", 1);

	case Proc_exitme:
		spllo();		/* pexit has locks in it */
		pexit("Killed", 1);

	case Proc_traceme:
		if(p->nnote == 0)
			return;
		/* No break */

	case Proc_stopme:
		p->procctl = 0;
		state = p->psstate;
		p->psstate = "Stopped";
		/* free a waiting debugger */
		s = spllo();
		qlock(&p->debug);
		if(p->pdbg) {
			wakeup(&p->pdbg->sleep);
			p->pdbg = 0;
		}
		qunlock(&p->debug);
		splhi();
		p->state = Stopped;
		sched();
		p->psstate = state;
		splx(s);
		return;
	}
}

void schedinit(void)		/* never returns */
{        DBGPRN("! %s \n", __FUNCTION__); }

int anyready(void)
{       DBGPRN("! %s \n", __FUNCTION__); return runvec; }

int anyhigher(void)
{       DBGPRN("! %s \n", __FUNCTION__);
	return runvec & ~((1<<(up->priority+1))-1);
}

void hzsched(void)
{        DBGPRN("! %s \n", __FUNCTION__); }

int preempted(void)
{       DBGPRN("! %s \n", __FUNCTION__); return 0; }


void ready(Proc *p)
{        DBGPRN("! %s \n", __FUNCTION__); }

void yield(void)
{        DBGPRN("! %s \n", __FUNCTION__); }

ulong balancetime;


Proc* wakeup(Rendez *r) 
{
    Proc *p;
    int s;
    s = splhi();
    lock(r);
    p = r->p;
    if(p != nil){
            lock(&p->rlock);
	    if(p->state != Wakeme || p->r != r)
	      panic("wakeup: state");
	    r->p = nil;
	    p->r = nil;
	    ready(p);
	    unlock(&p->rlock);
    }
    unlock(r);
    splx(s);
    return p;
}

static int tfn(void *arg)
{       
	return up->trend == nil || up->tfn(arg);
}

void twakeup(Ureg* _x, Timer *t)
{
	Proc *p;
	Rendez *trend;

	p = t->ta;
	trend = p->trend;
	p->trend = 0;
	if(trend)
		wakeup(trend);
}

void tsleep(Rendez *r, int (*fn)(void*), void *arg, ulong ms)
{
    if (up->tt){
        print("tsleep: timer active: mode %d, tf 0x%lux\n", up->tmode, up->tf);
	timerdel(up);
    }
    up->tns = MS2NS(ms);
    up->tf = twakeup;
    up->tmode = Trelative;
    up->ta = up;
    up->trend = r;
    up->tfn = fn;
    timeradd(up);    //% After ms microsec, twakeup(u, r) is invoked. 
    if(waserror()){
        timerdel(up);
	nexterror();
    }
    sleep(r, tfn, arg);
    if (up->tt)
        timerdel(up);
    up->twhen = 0;
    poperror();
}

#endif //%-----------------------------------------


