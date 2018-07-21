::::::::::::::::::
portdat.h
::::::::::::::::::

struct Proc
{
	Label	sched;		/* known to l.s */
	char	*kstack;	/* known to l.s */
	Mach	*mach;		/* machine running this proc */
	char	*text;
	char	*user;
	char	*args;
	int	nargs;		/* number of bytes of args */
	Proc	*rnext;		/* next process in run queue */
	Proc	*qnext;		/* next process on queue for a QLock */
	QLock	*qlock;		/* addr of qlock being queued for DEBUG */
	int	state;
	char	*psstate;	/* What /proc/#/status reports */
	Segment	*seg[NSEG];
	QLock	seglock;	/* locked whenever seg[] changes */
	ulong	pid;
	ulong	noteid;		/* Equivalent of note group */
	Proc	*pidhash;	/* next proc in pid hash */

	Lock	exl;		/* Lock count and waitq */
	Waitq	*waitq;		/* Exited processes wait children */
	int	nchild;		/* Number of living children */
	int	nwait;		/* Number of uncollected wait records */
	QLock	qwaitr;
	Rendez	waitr;		/* Place to hang out in wait */
	Proc	*parent;

	Pgrp	*pgrp;		/* Process group for namespace */
	Egrp 	*egrp;		/* Environment group */
	Fgrp	*fgrp;		/* File descriptor group */
	Rgrp	*rgrp;		/* Rendez group */

	Fgrp	*closingfgrp;	/* used during teardown */

	ulong	parentpid;
	ulong	time[6];	/* User, Sys, Real; child U, S, R */

	uvlong	kentry;		/* Kernel entry time stamp (for profiling) */
	/*
	 * pcycles: cycles spent in this process (updated on procsave/restore)
	 * when this is the current proc and we're in the kernel
	 * (procrestores outnumber procsaves by one)
	 * the number of cycles spent in the proc is pcycles + cycles()
	 * when this is not the current process or we're in user mode
	 * (procrestores and procsaves balance), it is pcycles.
	 */
	vlong	pcycles;

	int	insyscall;
	int	fpstate;

	QLock	debug;		/* to access debugging elements of User */
	Proc	*pdbg;		/* the debugging process */
	ulong	procmode;	/* proc device file mode */
	ulong	privatemem;	/* proc does not let anyone read mem */
	int	hang;		/* hang at next exec for debug */
	int	procctl;	/* Control for /proc debugging */
	ulong	pc;		/* DEBUG only */

	Lock	rlock;		/* sync sleep/wakeup with postnote */
	Rendez	*r;		/* rendezvous point slept on */
	Rendez	sleep;		/* place for syssleep/debug */
	int	notepending;	/* note issued but not acted on */
	int	kp;		/* true if a kernel process */
	Proc	*palarm;	/* Next alarm time */
	ulong	alarm;		/* Time of call */
	int	newtlb;		/* Pager has changed my pte's, I must flush */
	int	noswap;		/* process is not swappable */

	uintptr	rendtag;	/* Tag for rendezvous */
	uintptr	rendval;	/* Value for rendezvous */
	Proc	*rendhash;	/* Hash list for tag values */

	Timer;			/* For tsleep and real-time */
	Rendez	*trend;
	int	(*tfn)(void*);
	void	(*kpfun)(void*);
	void	*kparg;

	FPsave	fpsave;		/* address of this is known by db */
	int	scallnr;	/* sys call number - known by db */
	Sargs	s;		/* address of this is known by db */
	int	nerrlab;
	Label	errlab[NERR];
	char	*syserrstr;	/* last error from a system call, errbuf0 or 1 */
	char	*errstr;	/* reason we're unwinding the error stack, errbuf1 or 0 */
	char	errbuf0[ERRMAX];
	char	errbuf1[ERRMAX];
	char	genbuf[128];	/* buffer used e.g. for last name element from namec */
	Chan	*slash;
	Chan	*dot;

	Note	note[NNOTE];
	short	nnote;
	short	notified;	/* sysnoted is due */
	Note	lastnote;
	int	(*notify)(void*, char*);

	Lock	*lockwait;
	Lock	*lastlock;	/* debugging */
	Lock	*lastilock;	/* debugging */

	Mach	*wired;
	Mach	*mp;		/* machine this process last ran on */
	Ref	nlocks;		/* number of locks held by proc */
	ulong	delaysched;
	ulong	priority;	/* priority level */
	ulong	basepri;	/* base priority level */
	uchar	fixedpri;	/* priority level deson't change */
	ulong	cpu;		/* cpu average */
	ulong	lastupdate;
	uchar	yield;		/* non-zero if the process just did a sleep(0) */
	ulong	readytime;	/* time process came ready */
	ulong	movetime;	/* last time process switched processors */
	int	preempted;	/* true if this process hasn't finished the interrupt
				 *  that last preempted it
				 */
	Edf	*edf;		/* if non-null, real-time proc, edf contains scheduling params */
	int	trace;		/* process being traced? */

	ulong	qpc;		/* pc calling last blocking qlock */

	int	setargs;

	void	*ureg;		/* User registers for notes */
	void	*dbgreg;	/* User registers for devproc */
	Notsave;

	/*
	 *  machine specific MMU
	 */
	PMMU;
};



::::::::::::::
proc.c
::::::::::::::


/*
 *  sleep if a condition is not true.  Another process will
 *  awaken us after it sets the condition.  When we awaken
 *  the condition may no longer be true.
 *
 *  we lock both the process and the rendezvous to keep r->p
 *  and p->r synchronized.
 */

void  sleep(Rendez *r, int (*f)(void*), void *arg)
{
	int s;
	void (*pt)(Proc*, int, vlong);

	s = splhi();

	if(up->nlocks.ref)
		print("process %lud sleeps with %lud locks held, last lock 0x%p locked at pc 0x%lux, sleep called from 0x%lux\n",
			up->pid, up->nlocks.ref, up->lastlock, up->lastlock->pc, getcallerpc(&r));
	lock(r);
	lock(&up->rlock);
	if(r->p){
		print("double sleep called from 0x%lux, %lud %lud\n", getcallerpc(&r), r->p->pid, up->pid);
		dumpstack();
	}

	/*
	 *  Wakeup only knows there may be something to do by testing
	 *  r->p in order to get something to lock on.
	 *  Flush that information out to memory in case the sleep is
	 *  committed.
	 */
	r->p = up;

	if((*f)(arg) || up->notepending){
		/*
		 *  if condition happened or a note is pending
		 *  never mind
		 */
		r->p = nil;
		unlock(&up->rlock);
		unlock(r);
	} else {
		/*
		 *  now we are committed to
		 *  change state and call scheduler
		 */
		pt = proctrace;
		if(pt)
			pt(up, SSleep, 0);
		up->state = Wakeme;
		up->r = r;

		/* statistics */
		m->cs++;

		procsave(up);
		if(setlabel(&up->sched)) {
			/*
			 *  here when the process is awakened
			 */
			procrestore(up);
			spllo();
		} else {
			/*
			 *  here to go to sleep (i.e. stop Running)
			 */
			unlock(&up->rlock);
			unlock(r);
			gotolabel(&m->sched);
		}
	}

	if(up->notepending) {
		up->notepending = 0;
		splx(s);
		if(up->procctl == Proc_exitme && up->closingfgrp)
			forceclosefgrp();
		error(Eintr);
	}

	splx(s);
}


static int tfn(void *arg)
{
	return up->trend == nil || up->tfn(arg);
}


void twakeup(Ureg*, Timer *t)
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
	timeradd(up);

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


/*
 *  Expects that only one process can call wakeup for any given Rendez.
 *  We hold both locks to ensure that r->p and p->r remain consistent.
 *  Richard Miller has a better solution that doesn't require both to
 *  be held simultaneously, but I'm a paranoid - presotto.
 */

Proc*  wakeup(Rendez *r)
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


/*
 *  if waking a sleeping process, this routine must hold both
 *  p->rlock and r->lock.  However, it can't know them in
 *  the same order as wakeup causing a possible lock ordering
 *  deadlock.  We break the deadlock by giving up the p->rlock
 *  lock if we can't get the r->lock and retrying.
 */

int postnote(Proc *p, int dolock, char *n, int flag)
{
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
			break;	/* no */

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
}



void pexit(char *exitstr, int freemem)
{
	Proc *p;
	Segment **s, **es;
	long utime, stime;
	Waitq *wq, *f, *next;
	Fgrp *fgrp;
	Egrp *egrp;
	Rgrp *rgrp;
	Pgrp *pgrp;
	Chan *dot;
	void (*pt)(Proc*, int, vlong);

	up->alarm = 0;
	if (up->tt)
		timerdel(up);
	pt = proctrace;
	if(pt)
		pt(up, SDead, 0);

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
	if(up->kp == 0) {
		p = up->parent;
		if(p == 0) {
			if(exitstr == 0)
				exitstr = "unknown";
			panic("boot process died: %s", exitstr);
		}

		while(waserror())
			;

		wq = smalloc(sizeof(Waitq));
		poperror();

		wq->w.pid = up->pid;
		utime = up->time[TUser] + up->time[TCUser];
		stime = up->time[TSys] + up->time[TCSys];
		wq->w.time[TUser] = tk2ms(utime);
		wq->w.time[TSys] = tk2ms(stime);
		wq->w.time[TReal] = tk2ms(MACHP(0)->ticks - up->time[TReal]);
		if(exitstr && exitstr[0])
			snprint(wq->w.msg, sizeof(wq->w.msg), "%s %lud: %s", up->text, up->pid, exitstr);
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
			p->nchild--;
			p->time[TCUser] += utime;
			p->time[TCSys] += stime;

			wq->next = p->waitq;
			p->waitq = wq;
			p->nwait++;

			wakeup(&p->waitr);
			unlock(&p->exl);
		}
		else {
			unlock(&p->exl);
			free(wq);
		}
	}

	if(!freemem)
		addbroken(up);

	qlock(&up->seglock);
	es = &up->seg[NSEG];
	for(s = up->seg; s < es; s++) {
		if(*s) {
			putseg(*s);
			*s = 0;
		}
	}
	qunlock(&up->seglock);

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
	lock(&procalloc);
	lock(&palloc);

	edfstop(up);
	up->state = Moribund;
	sched();
	panic("pexit");
}


int haswaitq(void *x)
{
	Proc *p;

	p = (Proc *)x;
	return p->waitq != 0;
}


ulong pwait(Waitmsg *w)
{
	ulong cpid;
	Waitq *wq;

	if(!canqlock(&up->qwaitr))
		error(Einuse);

	if(waserror()) {
		qunlock(&up->qwaitr);
		nexterror();
	}

	lock(&up->exl);
	if(up->nchild == 0 && up->waitq == 0) {
		unlock(&up->exl);
		error(Enochild);
	}
	unlock(&up->exl);

	sleep(&up->waitr, haswaitq, up);

	lock(&up->exl);
	wq = up->waitq;
	up->waitq = wq->next;
	up->nwait--;
	unlock(&up->exl);

	qunlock(&up->qwaitr);
	poperror();

	if(w)
		memmove(w, &wq->w, sizeof(Waitmsg));
	cpid = wq->w.pid;
	free(wq);
	return cpid;
}


::::::::::::::
sysproc.c
::::::::::::::

long sysr1(ulong*)
{
	checkpagerefs();
	return 0;
}


long sysrfork(ulong *arg)
{
	Proc *p;
	int n, i;
	Fgrp *ofg;
	Pgrp *opg;
	Rgrp *org;
	Egrp *oeg;
	ulong pid, flag;
	Mach *wm;

	flag = arg[0];
	/* Check flags before we commit */
	if((flag & (RFFDG|RFCFDG)) == (RFFDG|RFCFDG))
		error(Ebadarg);
	if((flag & (RFNAMEG|RFCNAMEG)) == (RFNAMEG|RFCNAMEG))
		error(Ebadarg);
	if((flag & (RFENVG|RFCENVG)) == (RFENVG|RFCENVG))
		error(Ebadarg);

	if((flag&RFPROC) == 0) {
		if(flag & (RFMEM|RFNOWAIT))
			error(Ebadarg);
		if(flag & (RFFDG|RFCFDG)) {
			ofg = up->fgrp;
			if(flag & RFFDG)
				up->fgrp = dupfgrp(ofg);
			else
				up->fgrp = dupfgrp(nil);
			closefgrp(ofg);
		}
		if(flag & (RFNAMEG|RFCNAMEG)) {
			opg = up->pgrp;
			up->pgrp = newpgrp();
			if(flag & RFNAMEG)
				pgrpcpy(up->pgrp, opg);
			/* inherit noattach */
			up->pgrp->noattach = opg->noattach;
			closepgrp(opg);
		}
		if(flag & RFNOMNT)
			up->pgrp->noattach = 1;
		if(flag & RFREND) {
			org = up->rgrp;
			up->rgrp = newrgrp();
			closergrp(org);
		}
		if(flag & (RFENVG|RFCENVG)) {
			oeg = up->egrp;
			up->egrp = smalloc(sizeof(Egrp));
			up->egrp->ref = 1;
			if(flag & RFENVG)
				envcpy(up->egrp, oeg);
			closeegrp(oeg);
		}
		if(flag & RFNOTEG)
			up->noteid = incref(&noteidalloc);
		return 0;
	}

	p = newproc();

	p->fpsave = up->fpsave;
	p->scallnr = up->scallnr;
	p->s = up->s;
	p->nerrlab = 0;
	p->slash = up->slash;
	p->dot = up->dot;
	incref(p->dot);

	memmove(p->note, up->note, sizeof(p->note));
	p->privatemem = up->privatemem;
	p->noswap = up->noswap;
	p->nnote = up->nnote;
	p->notified = 0;
	p->lastnote = up->lastnote;
	p->notify = up->notify;
	p->ureg = up->ureg;
	p->dbgreg = 0;

	/* Make a new set of memory segments */
	n = flag & RFMEM;
	qlock(&p->seglock);
	if(waserror()){
		qunlock(&p->seglock);
		nexterror();
	}
	for(i = 0; i < NSEG; i++)
		if(up->seg[i])
			p->seg[i] = dupseg(up->seg, i, n);
	qunlock(&p->seglock);
	poperror();

	/* File descriptors */
	if(flag & (RFFDG|RFCFDG)) {
		if(flag & RFFDG)
			p->fgrp = dupfgrp(up->fgrp);
		else
			p->fgrp = dupfgrp(nil);
	}
	else {
		p->fgrp = up->fgrp;
		incref(p->fgrp);
	}

	/* Process groups */
	if(flag & (RFNAMEG|RFCNAMEG)) {
		p->pgrp = newpgrp();
		if(flag & RFNAMEG)
			pgrpcpy(p->pgrp, up->pgrp);
		/* inherit noattach */
		p->pgrp->noattach = up->pgrp->noattach;
	}
	else {
		p->pgrp = up->pgrp;
		incref(p->pgrp);
	}
	if(flag & RFNOMNT)
		up->pgrp->noattach = 1;

	if(flag & RFREND)
		p->rgrp = newrgrp();
	else {
		incref(up->rgrp);
		p->rgrp = up->rgrp;
	}

	/* Environment group */
	if(flag & (RFENVG|RFCENVG)) {
		p->egrp = smalloc(sizeof(Egrp));
		p->egrp->ref = 1;
		if(flag & RFENVG)
			envcpy(p->egrp, up->egrp);
	}
	else {
		p->egrp = up->egrp;
		incref(p->egrp);
	}
	p->hang = up->hang;
	p->procmode = up->procmode;

	/* Craft a return frame which will cause the child to pop out of
	 * the scheduler in user mode with the return register zero
	 */
	forkchild(p, up->dbgreg);

	p->parent = up;
	p->parentpid = up->pid;
	if(flag&RFNOWAIT)
		p->parentpid = 0;
	else {
		lock(&up->exl);
		up->nchild++;
		unlock(&up->exl);
	}
	if((flag&RFNOTEG) == 0)
		p->noteid = up->noteid;

	p->fpstate = up->fpstate;
	pid = p->pid;
	memset(p->time, 0, sizeof(p->time));
	p->time[TReal] = MACHP(0)->ticks;

	kstrdup(&p->text, up->text);
	kstrdup(&p->user, up->user);
	/*
	 *  since the bss/data segments are now shareable,
	 *  any mmu info about this process is now stale
	 *  (i.e. has bad properties) and has to be discarded.
	 */
	flushmmu();
	p->basepri = up->basepri;
	p->priority = up->basepri;
	p->fixedpri = up->fixedpri;
	p->mp = up->mp;
	wm = up->wired;
	if(wm)
		procwired(p, wm->machno);
	ready(p);
	sched();
	return pid;
}


static ulong l2be(long l)
{
	uchar *cp;

	cp = (uchar*)&l;
	return (cp[0]<<24) | (cp[1]<<16) | (cp[2]<<8) | cp[3];
}


long sysexec(ulong *arg)
{
	Segment *s, *ts;
	ulong t, d, b;
	int i;
	Chan *tc;
	char **argv, **argp;
	char *a, *charp, *args, *file;
	char *progarg[sizeof(Exec)/2+1], *elem, progelem[64];
	ulong ssize, spage, nargs, nbytes, n, bssend;
	int indir;
	Exec exec;
	char line[sizeof(Exec)];
	Fgrp *f;
	Image *img;
	ulong magic, text, entry, data, bss;
	Tos *tos;

	validaddr(arg[0], 1, 0);
	file = (char*)arg[0];
	indir = 0;
	elem = nil;
	if(waserror()){
		free(elem);
		nexterror();
	}
	for(;;){
		tc = namec(file, Aopen, OEXEC, 0);
		if(waserror()){
			cclose(tc);
			nexterror();
		}
		if(!indir)
			kstrdup(&elem, up->genbuf);

		n = devtab[tc->type]->read(tc, &exec, sizeof(Exec), 0);
		if(n < 2)
			error(Ebadexec);
		magic = l2be(exec.magic);
		text = l2be(exec.text);
		entry = l2be(exec.entry);
		if(n==sizeof(Exec) && (magic == AOUT_MAGIC)){
			if(text >= USTKTOP-UTZERO
			|| entry < UTZERO+sizeof(Exec)
			|| entry >= UTZERO+sizeof(Exec)+text)
				error(Ebadexec);
			break; /* for binary */
		}

		/*
		 * Process #! /bin/sh args ...
		 */
		memmove(line, &exec, sizeof(Exec));
		if(indir || line[0]!='#' || line[1]!='!')
			error(Ebadexec);
		n = shargs(line, n, progarg);
		if(n == 0)
			error(Ebadexec);
		indir = 1;
		/*
		 * First arg becomes complete file name
		 */
		progarg[n++] = file;
		progarg[n] = 0;
		validaddr(arg[1], BY2WD, 1);
		arg[1] += BY2WD;
		file = progarg[0];
		if(strlen(elem) >= sizeof progelem)
			error(Ebadexec);
		strcpy(progelem, elem);
		progarg[0] = progelem;
		poperror();
		cclose(tc);
	}

	data = l2be(exec.data);
	bss = l2be(exec.bss);
	t = (UTZERO+sizeof(Exec)+text+(BY2PG-1)) & ~(BY2PG-1);
	d = (t + data + (BY2PG-1)) & ~(BY2PG-1);
	bssend = t + data + bss;
	b = (bssend + (BY2PG-1)) & ~(BY2PG-1);
	if(t >= KZERO || d >= KZERO || b >= KZERO)
		error(Ebadexec);

	/*
	 * Args: pass 1: count
	 */
	nbytes = sizeof(Tos);		/* hole for profiling clock at top of stack (and more) */
	nargs = 0;
	if(indir){
		argp = progarg;
		while(*argp){
			a = *argp++;
			nbytes += strlen(a) + 1;
			nargs++;
		}
	}
	evenaddr(arg[1]);
	argp = (char**)arg[1];
	validaddr((ulong)argp, BY2WD, 0);
	while(*argp){
		a = *argp++;
		if(((ulong)argp&(BY2PG-1)) < BY2WD)
			validaddr((ulong)argp, BY2WD, 0);
		validaddr((ulong)a, 1, 0);
		nbytes += ((char*)vmemchr(a, 0, 0x7FFFFFFF) - a) + 1;
		nargs++;
	}
	ssize = BY2WD*(nargs+1) + ((nbytes+(BY2WD-1)) & ~(BY2WD-1));

	/*
	 * 8-byte align SP for those (e.g. sparc) that need it.
	 * execregs() will subtract another 4 bytes for argc.
	 */
	if((ssize+4) & 7)
		ssize += 4;
	spage = (ssize+(BY2PG-1)) >> PGSHIFT;

	/*
	 * Build the stack segment, putting it in kernel virtual for the moment
	 */
	if(spage > TSTKSIZ)
		error(Enovmem);

	qlock(&up->seglock);
	if(waserror()){
		qunlock(&up->seglock);
		nexterror();
	}
	up->seg[ESEG] = newseg(SG_STACK, TSTKTOP-USTKSIZE, USTKSIZE/BY2PG);

	/*
	 * Args: pass 2: assemble; the pages will be faulted in
	 */
	tos = (Tos*)(TSTKTOP - sizeof(Tos));
	tos->cyclefreq = m->cyclefreq;
	cycles((uvlong*)&tos->pcycles);
	tos->pcycles = -tos->pcycles;
	tos->kcycles = tos->pcycles;
	tos->clock = 0;
	argv = (char**)(TSTKTOP - ssize);
	charp = (char*)(TSTKTOP - nbytes);
	args = charp;
	if(indir)
		argp = progarg;
	else
		argp = (char**)arg[1];

	for(i=0; i<nargs; i++){
		if(indir && *argp==0) {
			indir = 0;
			argp = (char**)arg[1];
		}
		*argv++ = charp + (USTKTOP-TSTKTOP);
		n = strlen(*argp) + 1;
		memmove(charp, *argp++, n);
		charp += n;
	}

	free(up->text);
	up->text = elem;
	elem = nil;	/* so waserror() won't free elem */
	USED(elem);

	/* copy args; easiest from new process's stack */
	n = charp - args;
	if(n > 128)	/* don't waste too much space on huge arg lists */
		n = 128;
	a = up->args;
	up->args = nil;
	free(a);
	up->args = smalloc(n);
	memmove(up->args, args, n);
	if(n>0 && up->args[n-1]!='\0'){
		/* make sure last arg is NUL-terminated */
		/* put NUL at UTF-8 character boundary */
		for(i=n-1; i>0; --i)
			if(fullrune(up->args+i, n-i))
				break;
		up->args[i] = 0;
		n = i+1;
	}
	up->nargs = n;

	/*
	 * Committed.
	 * Free old memory.
	 * Special segments are maintained across exec
	 */
	for(i = SSEG; i <= BSEG; i++) {
		putseg(up->seg[i]);
		/* prevent a second free if we have an error */
		up->seg[i] = 0;
	}
	for(i = BSEG+1; i < NSEG; i++) {
		s = up->seg[i];
		if(s != 0 && (s->type&SG_CEXEC)) {
			putseg(s);
			up->seg[i] = 0;
		}
	}

	/*
	 * Close on exec
	 */
	f = up->fgrp;
	for(i=0; i<=f->maxfd; i++)
		fdclose(i, CCEXEC);

	/* Text.  Shared. Attaches to cache image if possible */
	/* attachimage returns a locked cache image */
	img = attachimage(SG_TEXT|SG_RONLY, tc, UTZERO, (t-UTZERO)>>PGSHIFT);
	ts = img->s;
	up->seg[TSEG] = ts;
	ts->flushme = 1;
	ts->fstart = 0;
	ts->flen = sizeof(Exec)+text;
	unlock(img);

	/* Data. Shared. */
	s = newseg(SG_DATA, t, (d-t)>>PGSHIFT);
	up->seg[DSEG] = s;

	/* Attached by hand */
	incref(img);
	s->image = img;
	s->fstart = ts->fstart+ts->flen;
	s->flen = data;

	/* BSS. Zero fill on demand */
	up->seg[BSEG] = newseg(SG_BSS, d, (b-d)>>PGSHIFT);

	/*
	 * Move the stack
	 */
	s = up->seg[ESEG];
	up->seg[ESEG] = 0;
	up->seg[SSEG] = s;
	qunlock(&up->seglock);
	poperror();	/* seglock */
	poperror();	/* elem */
	s->base = USTKTOP-USTKSIZE;
	s->top = USTKTOP;
	relocateseg(s, USTKTOP-TSTKTOP);

	/*
	 *  '/' processes are higher priority (hack to make /ip more responsive).
	 */
	if(devtab[tc->type]->dc == L'/')
		up->basepri = PriRoot;
	up->priority = up->basepri;
	poperror();
	cclose(tc);

	/*
	 *  At this point, the mmu contains info about the old address
	 *  space and needs to be flushed
	 */
	flushmmu();
	qlock(&up->debug);
	up->nnote = 0;
	up->notify = 0;
	up->notified = 0;
	up->privatemem = 0;
	procsetup(up);
	qunlock(&up->debug);
	if(up->hang)
		up->procctl = Proc_stopme;

	return execregs(entry, ssize, nargs);
}



int return0(void*)
{
	return 0;
}


long syssleep(ulong *arg)
{

	int n;

	n = arg[0];
	if(n <= 0) {
		if (up->edf && (up->edf->flags & Admitted))
			edfyield();
		else
			yield();
		return 0;
	}
	if(n < TK2MS(1))
		n = TK2MS(1);
	tsleep(&up->sleep, return0, 0, n);
	return 0;
}


long sysalarm(ulong *arg)
{
	return procalarm(arg[0]);
}


long sysexits(ulong *arg)
{
	char *status;
	char *inval = "invalid exit string";
	char buf[ERRMAX];

	status = (char*)arg[0];
	if(status){
		if(waserror())
			status = inval;
		else{
			validaddr((ulong)status, 1, 0);
			if(vmemchr(status, 0, ERRMAX) == 0){
				memmove(buf, status, ERRMAX);
				buf[ERRMAX-1] = 0;
				status = buf;
			}
			poperror();
		}

	}
	pexit(status, 1);
	return 0;		/* not reached */
}


long sys_wait(ulong *arg)
{
	int pid;
	Waitmsg w;
	OWaitmsg *ow;

	if(arg[0] == 0)
		return pwait(nil);

	validaddr(arg[0], sizeof(OWaitmsg), 1);
	evenaddr(arg[0]);
	pid = pwait(&w);
	if(pid >= 0){
		ow = (OWaitmsg*)arg[0];
		readnum(0, ow->pid, NUMSIZE, w.pid, NUMSIZE);
		readnum(0, ow->time+TUser*NUMSIZE, NUMSIZE, w.time[TUser], NUMSIZE);
		readnum(0, ow->time+TSys*NUMSIZE, NUMSIZE, w.time[TSys], NUMSIZE);
		readnum(0, ow->time+TReal*NUMSIZE, NUMSIZE, w.time[TReal], NUMSIZE);
		strncpy(ow->msg, w.msg, sizeof(ow->msg));
		ow->msg[sizeof(ow->msg)-1] = '\0';
	}
	return pid;
}


long sysawait(ulong *arg)
{
	int i;
	int pid;
	Waitmsg w;
	ulong n;

	n = arg[1];
	validaddr(arg[0], n, 1);
	pid = pwait(&w);
	if(pid < 0)
		return -1;
	i = snprint((char*)arg[0], n, "%d %lud %lud %lud %q",
		w.pid,
		w.time[TUser], w.time[TSys], w.time[TReal],
		w.msg);

	return i;
}


long sysdeath(ulong*)
{
	pprint("deprecated system call\n");
	pexit("Suicide", 0);
	return 0;	/* not reached */
}


void werrstr(char *fmt, ...)
{
	va_list va;

	if(up == nil)
		return;

	va_start(va, fmt);
	vseprint(up->syserrstr, up->syserrstr+ERRMAX, fmt, va);
	va_end(va);
}


static long generrstr(char *buf, uint nbuf)
{
	char tmp[ERRMAX];

	if(nbuf == 0)
		error(Ebadarg);
	validaddr((ulong)buf, nbuf, 1);
	if(nbuf > sizeof tmp)
		nbuf = sizeof tmp;
	memmove(tmp, buf, nbuf);

	/* make sure it's NUL-terminated */
	tmp[nbuf-1] = '\0';
	memmove(buf, up->syserrstr, nbuf);
	buf[nbuf-1] = '\0';
	memmove(up->syserrstr, tmp, nbuf);
	return 0;
}


long syserrstr(ulong *arg)
{
	return generrstr((char*)arg[0], arg[1]);
}


/* compatibility for old binaries */
long sys_errstr(ulong *arg)
{
	return generrstr((char*)arg[0], 64);
}

long
sysnotify(ulong *arg)
{
	if(arg[0] != 0)
		validaddr(arg[0], sizeof(ulong), 0);
	up->notify = (int(*)(void*, char*))(arg[0]);
	return 0;
}


long sysnoted(ulong *arg)
{
	if(arg[0]!=NRSTR && !up->notified)
		error(Egreg);
	return 0;
}


long sysrendezvous(ulong *arg)
{
	uintptr tag, val;
	Proc *p, **l;

	tag = arg[0];
	l = &REND(up->rgrp, tag);
	up->rendval = ~(uintptr)0;

	lock(up->rgrp);
	for(p = *l; p; p = p->rendhash) {
		if(p->rendtag == tag) {
			*l = p->rendhash;
			val = p->rendval;
			p->rendval = arg[1];

			while(p->mach != 0)
				;
			ready(p);
			unlock(up->rgrp);
			return val;
		}
		l = &p->rendhash;
	}

	/* Going to sleep here */
	up->rendtag = tag;
	up->rendval = arg[1];
	up->rendhash = *l;
	*l = up;
	up->state = Rendezvous;
	unlock(up->rgrp);

	sched();

	return up->rendval;
}



::::::::::::::::::
portclock.c
::::::::::::::::::

struct Timers
{
	Lock;
	Timer	*head;
};

static Timers timers[MAXMACH];

ulong intrcount[MAXMACH];
ulong fcallcount[MAXMACH];


static vlong tadd(Timers *tt, Timer *nt)
{
	Timer *t, **last;

	/* Called with tt locked */
	assert(nt->tt == nil);
	switch(nt->tmode){
	default:
		panic("timer");
		break;
	case Trelative:
		if(nt->tns <= 0)
			nt->tns = 1;
		nt->twhen = fastticks(nil) + ns2fastticks(nt->tns);
		break;
	case Tperiodic:
		assert(nt->tns >= 100000);	/* At least 100 µs period */
		if(nt->twhen == 0){
			/* look for another timer at same frequency for combining */
			for(t = tt->head; t; t = t->tnext){
				if(t->tmode == Tperiodic && t->tns == nt->tns)
					break;
			}
			if (t)
				nt->twhen = t->twhen;
			else
				nt->twhen = fastticks(nil);
		}
		nt->twhen += ns2fastticks(nt->tns);
		break;
	}

	for(last = &tt->head; t = *last; last = &t->tnext){
		if(t->twhen > nt->twhen)
			break;
	}
	nt->tnext = *last;
	*last = nt;
	nt->tt = tt;
	if(last == &tt->head)
		return nt->twhen;
	return 0;
}


static uvlong tdel(Timer *dt)
{

	Timer *t, **last;
	Timers *tt;

	tt = dt->tt;
	if (tt == nil)
		return 0;
	for(last = &tt->head; t = *last; last = &t->tnext){
		if(t == dt){
			assert(dt->tt);
			dt->tt = nil;
			*last = t->tnext;
			break;
		}
	}
	if(last == &tt->head && tt->head)
		return tt->head->twhen;
	return 0;
}


/* add or modify a timer */
void timeradd(Timer *nt)
{
	Timers *tt;
	vlong when;

	/* Must lock Timer struct before Timers struct */
	ilock(nt);
	if(tt = nt->tt){
		ilock(tt);
		tdel(nt);
		iunlock(tt);
	}
	tt = &timers[m->machno];
	ilock(tt);
	when = tadd(tt, nt);
	if(when)
		timerset(when);
	iunlock(tt);
	iunlock(nt);
}


void timerdel(Timer *dt)
{
	Timers *tt;
	uvlong when;

	ilock(dt);
	if(tt = dt->tt){
		ilock(tt);
		when = tdel(dt);
		if(when && tt == &timers[m->machno])
			timerset(tt->head->twhen);
		iunlock(tt);
	}
	iunlock(dt);
}


void hzclock(Ureg *ur)
{
	m->ticks++;
	if(m->proc)
		m->proc->pc = ur->pc;

	if(m->flushmmu){
		if(up)
			flushmmu();
		m->flushmmu = 0;
	}

	accounttime();
	kmapinval();

	if(kproftimer != nil)
		kproftimer(ur->pc);

	if((active.machs&(1<<m->machno)) == 0)
		return;

	if(active.exiting) {
		print("someone's exiting\n");
		exit(0);
	}

	checkalarms();

	if(up && up->state == Running)
		hzsched();	/* in proc.c */
}

//----------------------------------------
void timerintr(Ureg *u, uvlong)
{
	Timer *t;
	Timers *tt;
	uvlong when, now;
	int callhzclock;
	static int sofar;

	intrcount[m->machno]++;
	callhzclock = 0;
	tt = &timers[m->machno];
	now = fastticks(nil);
	ilock(tt);
	while(t = tt->head){
		/*
		 * No need to ilock t here: any manipulation of t
		 * requires tdel(t) and this must be done with a
		 * lock to tt held.  We have tt, so the tdel will
		 * wait until we're done
		 */
		when = t->twhen;
		if(when > now){
			timerset(when);
			iunlock(tt);
			if(callhzclock)
				hzclock(u);
			return;
		}
		tt->head = t->tnext;
		assert(t->tt == tt);
		t->tt = nil;
		fcallcount[m->machno]++;
		iunlock(tt);
		if(t->tf)
			(*t->tf)(u, t);
		else
			callhzclock++;
		ilock(tt);
		if(t->tmode == Tperiodic)
			tadd(tt, t);
	}
	iunlock(tt);
}

void timersinit(void)
{
	Timer *t;

	/*
	 * This is only called on the bootstrap processor; hence,
	 * the Hzclock will only be active on that processor
	 */
	todinit();
	t = malloc(sizeof(*t));
	t->tmode = Tperiodic;
	t->tt = nil;
	t->tns = 1000000000/HZ;
	t->tf = nil;
	timeradd(t);
}

Timer* addclock0link(void (*f)(void), int ms)
{
	Timer *nt;
	uvlong when;

	/* Synchronize to hztimer if ms is 0 */
	nt = malloc(sizeof(Timer));
	if(ms == 0)
		ms = 1000/HZ;
	nt->tns = (vlong)ms*1000000LL;
	nt->tmode = Tperiodic;
	nt->tt = nil;
	nt->tf = (void (*)(Ureg*, Timer*))f;

	ilock(&timers[0]);
	when = tadd(&timers[0], nt);
	if(when)
		timerset(when);
	iunlock(&timers[0]);
	return nt;
}


/*
 *  This tk2ms avoids overflows that the macro version is prone to.
 *  It is a LOT slower so shouldn't be used if you're just converting
 *  a delta.
 */
ulong tk2ms(ulong ticks)
{
	uvlong t, hz;

	t = ticks;
	hz = HZ;
	t *= 1000L;
	t = t/hz;
	ticks = t;
	return ticks;
}

ulong ms2tk(ulong ms)
{
	/* avoid overflows at the cost of precision */
	if(ms >= 1000000000/HZ)
		return (ms/1000)*HZ;
	return (ms*HZ+500)/1000;
}


:::::::::::::::::
trap.c
:::::::::::::::::


/* go to user space */
void kexit(Ureg*)
{
	uvlong t;
	Tos *tos;

	/* precise time accounting, kernel exit */
	tos = (Tos*)(USTKTOP-sizeof(Tos));
	cycles(&t);
	tos->kcycles += t - up->kentry;
	tos->pcycles = up->pcycles;
	tos->pid = up->pid;
}


/*
 *  Call user, if necessary, with note.
 *  Pass user the Ureg struct and the note on his stack.
 */
int notify(Ureg* ureg)
{
	int l;
	ulong s, sp;
	Note *n;

	if(up->procctl)
		procctl(up);
	if(up->nnote == 0)
		return 0;

	if(up->fpstate == FPactive){
		fpsave(&up->fpsave);
		up->fpstate = FPinactive;
	}
	up->fpstate |= FPillegal;

	s = spllo();
	qlock(&up->debug);
	up->notepending = 0;
	n = &up->note[0];
	if(strncmp(n->msg, "sys:", 4) == 0){
		l = strlen(n->msg);
		if(l > ERRMAX-15)	/* " pc=0x12345678\0" */
			l = ERRMAX-15;
		sprint(n->msg+l, " pc=0x%.8lux", ureg->pc);
	}

	if(n->flag!=NUser && (up->notified || up->notify==0)){
		if(n->flag == NDebug)
			pprint("suicide: %s\n", n->msg);
		qunlock(&up->debug);
		pexit(n->msg, n->flag!=NDebug);
	}

	if(up->notified){
		qunlock(&up->debug);
		splhi();
		return 0;
	}
		
	if(!up->notify){
		qunlock(&up->debug);
		pexit(n->msg, n->flag!=NDebug);
	}
	sp = ureg->usp;
	sp -= sizeof(Ureg);

	if(!okaddr((ulong)up->notify, 1, 0)
	|| !okaddr(sp-ERRMAX-4*BY2WD, sizeof(Ureg)+ERRMAX+4*BY2WD, 1)){
		pprint("suicide: bad address in notify\n");
		qunlock(&up->debug);
		pexit("Suicide", 0);
	}

	up->ureg = (void*)sp;
	memmove((Ureg*)sp, ureg, sizeof(Ureg));
	*(Ureg**)(sp-BY2WD) = up->ureg;	/* word under Ureg is old up->ureg */
	up->ureg = (void*)sp;
	sp -= BY2WD+ERRMAX;
	memmove((char*)sp, up->note[0].msg, ERRMAX);
	sp -= 3*BY2WD;
	*(ulong*)(sp+2*BY2WD) = sp+3*BY2WD;		/* arg 2 is string */
	*(ulong*)(sp+1*BY2WD) = (ulong)up->ureg;	/* arg 1 is ureg* */
	*(ulong*)(sp+0*BY2WD) = 0;			/* arg 0 is pc */
	ureg->usp = sp;
	ureg->pc = (ulong)up->notify;
	up->notified = 1;
	up->nnote--;
	memmove(&up->lastnote, &up->note[0], sizeof(Note));
	memmove(&up->note[0], &up->note[1], up->nnote*sizeof(Note));

	qunlock(&up->debug);
	splx(s);
	return 1;
}


/*
 *   Return user to state before notify()
 */
void noted(Ureg* ureg, ulong arg0)
{
	Ureg *nureg;
	ulong oureg, sp;

	qlock(&up->debug);
	if(arg0!=NRSTR && !up->notified) {
		qunlock(&up->debug);
		pprint("call to noted() when not notified\n");
		pexit("Suicide", 0);
	}
	up->notified = 0;

	nureg = up->ureg;	/* pointer to user returned Ureg struct */

	up->fpstate &= ~FPillegal;

	/* sanity clause */
	oureg = (ulong)nureg;
	if(!okaddr((ulong)oureg-BY2WD, BY2WD+sizeof(Ureg), 0)){
		pprint("bad ureg in noted or call to noted when not notified\n");
		qunlock(&up->debug);
		pexit("Suicide", 0);
	}

	/*
	 * Check the segment selectors are all valid, otherwise
	 * a fault will be taken on attempting to return to the
	 * user process.
	 * Take care with the comparisons as different processor
	 * generations push segment descriptors in different ways.
	 */
	if((nureg->cs & 0xFFFF) != UESEL || (nureg->ss & 0xFFFF) != UDSEL
	  || (nureg->ds & 0xFFFF) != UDSEL || (nureg->es & 0xFFFF) != UDSEL
	  || (nureg->fs & 0xFFFF) != UDSEL || (nureg->gs & 0xFFFF) != UDSEL){
		pprint("bad segment selector in noted\n");
		qunlock(&up->debug);
		pexit("Suicide", 0);
	}

	/* don't let user change system flags */
	nureg->flags = (ureg->flags & ~0xCD5) | (nureg->flags & 0xCD5);

	memmove(ureg, nureg, sizeof(Ureg));

	switch(arg0){
	case NCONT:
	case NRSTR:
		if(!okaddr(nureg->pc, 1, 0) || !okaddr(nureg->usp, BY2WD, 0)){
			qunlock(&up->debug);
			pprint("suicide: trap in noted\n");
			pexit("Suicide", 0);
		}
		up->ureg = (Ureg*)(*(ulong*)(oureg-BY2WD));
		qunlock(&up->debug);
		break;

	case NSAVE:
		if(!okaddr(nureg->pc, BY2WD, 0)
		|| !okaddr(nureg->usp, BY2WD, 0)){
			qunlock(&up->debug);
			pprint("suicide: trap in noted\n");
			pexit("Suicide", 0);
		}
		qunlock(&up->debug);
		sp = oureg-4*BY2WD-ERRMAX;
		splhi();
		ureg->sp = sp;
		((ulong*)sp)[1] = oureg;	/* arg 1 0(FP) is ureg* */
		((ulong*)sp)[0] = 0;		/* arg 0 is pc */
		break;

	default:
		pprint("unknown noted arg 0x%lux\n", arg0);
		up->lastnote.flag = NDebug;
		/* fall through */
		
	case NDFLT:
		if(up->lastnote.flag == NDebug){ 
			qunlock(&up->debug);
			pprint("suicide: %s\n", up->lastnote.msg);
		} else
			qunlock(&up->debug);
		pexit(up->lastnote.msg, up->lastnote.flag!=NDebug);
	}
}


long execregs(ulong entry, ulong ssize, ulong nargs)
{
	ulong *sp;
	Ureg *ureg;

	up->fpstate = FPinit;
	fpoff();

	sp = (ulong*)(USTKTOP - ssize);
	*--sp = nargs;

	ureg = up->dbgreg;
	ureg->usp = (ulong)sp;
	ureg->pc = entry;
	return USTKTOP-sizeof(Tos);		/* address of kernel/user shared data */
}


/*
 *  return the userpc the last exception happened at
 */
ulong userpc(void)
{
	Ureg *ureg;

	ureg = (Ureg*)up->dbgreg;
	return ureg->pc;
}


/* This routine must save the values of registers the user is not permitted
 * to write from devproc and then restore the saved values before returning.
 */
void setregisters(Ureg* ureg, char* pureg, char* uva, int n)
{
	ulong flags;
	ulong cs;
	ulong ss;

	flags = ureg->flags;
	cs = ureg->cs;
	ss = ureg->ss;
	memmove(pureg, uva, n);
	ureg->flags = (ureg->flags & 0x00FF) | (flags & 0xFF00);
	ureg->cs = cs;
	ureg->ss = ss;
}


static void linkproc(void)
{
	spllo();
	up->kpfun(up->kparg);
	pexit("kproc dying", 0);
}


void kprocchild(Proc* p, void (*func)(void*), void* arg)
{
	/*
	 * gotolabel() needs a word on the stack in
	 * which to place the return PC used to jump
	 * to linkproc().
	 */
	p->sched.pc = (ulong)linkproc;
	p->sched.sp = (ulong)p->kstack+KSTACK-BY2WD;

	p->kpfun = func;
	p->kparg = arg;
}

//--------------------------------------------------
void forkchild(Proc *p, Ureg *ureg)
{
	Ureg *cureg;

	/*
	 * Add 2*BY2WD to the stack to account for
	 *  - the return PC
	 *  - trap's argument (ur)
	 */
	p->sched.sp = (ulong)p->kstack+KSTACK-(sizeof(Ureg)+2*BY2WD);
	p->sched.pc = (ulong)forkret;

	cureg = (Ureg*)(p->sched.sp+2*BY2WD);
	memmove(cureg, ureg, sizeof(Ureg));
	/* return value of syscall in child */
	cureg->ax = 0;

	/* Things from bottom of syscall which were never executed */
	p->psstate = 0;
	p->insyscall = 0;
}


/* Give enough context in the ureg to produce a kernel stack for
 * a sleeping process
 */
void setkernur(Ureg* ureg, Proc* p)
{
	ureg->pc = p->sched.pc;
	ureg->sp = p->sched.sp+4;
}


ulong dbgpc(Proc *p)
{
	Ureg *ureg;

	ureg = p->dbgreg;
	if(ureg == 0)
		return 0;

	return ureg->pc;
}


