//%%%%%%% qlock-l4.c %%%%%%%%%%%

#include <u.h>
#include <libc.h>

#if 1 //%------------------------------------------
#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

#define   _DBGFLG  0
#define DBGPRN  if(_DBGFLG)print

#define ASIZE  512
#define printTBD  1
static void tobedefined(char *name)
{
  static int  nextx = 0;
  static char fname[ASIZE];
  int  i;
   
  if (!printTBD)  return;
 
  if (strcmp(name, fname) == 0) return ;
  for (i=0; i <= nextx; i++) {
    if (fname[i] == 0){
      if (strcmp(name, &fname[i+1]) == 0) return ;
    }
  }
                                                                                
  if (nextx > ASIZE - 32) {
    print("to-be-defined fname table overflow\n");
    return ;
  }
                                                                                
  print("! %s(): to be defined\n", name);
  strcpy(&fname[nextx], name);
  nextx += strlen(name) + 1;
}

#define  XX    // print("%s  ", __FUNCTION__)

#endif //%-----------------------------------------

static struct {
	QLp	*p;
        QLp	x[128];  //% <- 1024
} ql = {
	ql.x
};

enum
{
	Queuing,
	QueuingR,
	QueuingW,
	Sleeping,
};

static void*	(*_rendezvousp)(void*, void*) = rendezvous;

/* this gets called by the thread library ONLY to get us to use its rendezvous */
void
_qlockinit(void* (*r)(void*, void*))
{
	_rendezvousp = r;
}

/* find a free shared memory location to queue ourselves in */
static QLp*
getqlp(void)
{
        XX;  
#if 0 //%---------------------------	
	return 0;
#else //-----------------------------
	QLp *p, *op;

	op = ql.p;
	for(p = op+1; ; p++){
		if(p == &ql.x[nelem(ql.x)])
			p = ql.x;
		if(p == op)
			abort();
		if(_tas(&(p->inuse)) == 0){
			ql.p = p;
			p->next = nil;
			break;
		}
	}
	return p;
#endif //%-----------------------------------
}

void
qlock(QLock *q)
{
        XX;
#if 0
#else 
	QLp *p, *mp;

	lock(&q->lock);
	if(!q->locked){
		q->locked = 1;
		unlock(&q->lock);
		return;
	}

	/* chain into waiting list */
	mp = getqlp();
	p = q->tail;
	if(p == nil)
		q->head = mp;
	else
		p->next = mp;
	q->tail = mp;
	mp->state = Queuing;

	/* wait */
#if 1 //%----------------------------
	mp->tid = L4_Myself().raw;   //%
	unlock(&q->lock);
	{
	    L4_MsgTag_t    tag;
	    L4_ThreadId_t  sender;
	    tag = L4_Wait(&sender);
	    // if (L4_IpcFailed(tag)) XXX;
	}
#else //%original--------------------
	unlock(&q->lock);
	while((*_rendezvousp)(mp, (void*)1) == (void*)~0)
		;
#endif //%-----------------------------

	mp->inuse = 0;
#endif
}

void
qunlock(QLock *q)
{
        XX;
#if 0
#else
 	QLp *p;

	lock(&q->lock);
	p = q->head;
	if(p != nil){
		/* wakeup head waiting process */
		q->head = p->next;
		if(q->head == nil)
			q->tail = nil;

#if 1 //%---------------------------------------
		{
		    L4_ThreadId_t  tid;
		    L4_MsgTag_t    tag;
		    tid.raw = p->tid;
		    unlock(&q->lock);
		    L4_LoadMR(0, 0);
		    tag = L4_Send(tid);
		    // if (L4_IpcFailed(tag)) XXX;
		}
#else //%--------------------------------------
		unlock(&q->lock);
		while((*_rendezvousp)(p, (void*)0x12345) == (void*)~0)
			;
#endif //%------------------------------------
		return;
	}
	q->locked = 0;
	unlock(&q->lock);
#endif
}

int
canqlock(QLock *q)
{
        XX;
#if 0
        return 1;
#else
	if(!canlock(&q->lock))
		return 0;
	if(!q->locked){
		q->locked = 1;
		unlock(&q->lock);
		return 1;
	}
	unlock(&q->lock);
	return 0;
#endif 
}

void
rlock(RWLock *q)
{
        XX;
#if 0
#else
	QLp *p, *mp;

	lock(&q->lock);
	if(q->writer == 0 && q->head == nil){
		/* no writer, go for it */
		q->readers++;
		unlock(&q->lock);
		return;
	}

	mp = getqlp();
	p = q->tail;
	if(p == 0)
		q->head = mp;
	else
		p->next = mp;
	q->tail = mp;
	mp->next = nil;
	mp->state = QueuingR;

#if 1 //%------------------------------------
	mp->tid = L4_Myself().raw;   //%
	unlock(&q->lock);
        {
	    L4_MsgTag_t    tag;
	    L4_ThreadId_t  sender;
	    tag = L4_Wait(&sender);
	    // if (L4_IpcFailed(tag)) XXX;
        }
#else //%original--------------------------
	unlock(&q->lock);

	/* wait in kernel */
	while((*_rendezvousp)(mp, (void*)1) == (void*)~0)
		;
#endif //%-------------------------------
	mp->inuse = 0;
#endif
}

int
canrlock(RWLock *q)
{
        XX;
#if 0
        return 1;
#else
	lock(&q->lock);
	if (q->writer == 0 && q->head == nil) {
		/* no writer; go for it */
		q->readers++;
		unlock(&q->lock);
		return 1;
	}
	unlock(&q->lock);
	return 0;
#endif
}

void
runlock(RWLock *q)
{
        XX;
#if 0
#else
	QLp *p;

	lock(&q->lock);
	if(q->readers <= 0)
		abort();
	p = q->head;
	if(--(q->readers) > 0 || p == nil){
		unlock(&q->lock);
		return;
	}

	/* start waiting writer */
	if(p->state != QueuingW)
		abort();
	q->head = p->next;
	if(q->head == 0)
		q->tail = 0;
	q->writer = 1;
#if 1 //%---------------------------------------
	{
	    L4_ThreadId_t  tid;
	    L4_MsgTag_t    tag;
	    tid.raw = p->tid;
	    unlock(&q->lock);
	    L4_LoadMR(0, 0);
	    tag = L4_Send(tid);
	    // if (L4_IpcFailed(tag)) XXX;
	}
#else //%--------------------------------------
	unlock(&q->lock);

	/* wakeup waiter */
	while((*_rendezvousp)(p, 0) == (void*)~0)
		;
#endif //%-----------------------------------
#endif
}

void
wlock(RWLock *q)
{
        XX;
#if 0
#else 
	QLp *p, *mp;

	lock(&q->lock);
	if(q->readers == 0 && q->writer == 0){
		/* noone waiting, go for it */
		q->writer = 1;
		unlock(&q->lock);
		return;
	}

	/* wait */
	p = q->tail;
	mp = getqlp();
	if(p == nil)
		q->head = mp;
	else
		p->next = mp;
	q->tail = mp;
	mp->next = nil;
	mp->state = QueuingW;

#if 1 //%------------------------------------
        mp->tid = L4_Myself().raw;   //%
        unlock(&q->lock);
        {
	    L4_MsgTag_t    tag;
	    L4_ThreadId_t  sender;
	    tag = L4_Wait(&sender);
	    // if (L4_IpcFailed(tag)) XXX;
        }
#else //%original--------------------------
	unlock(&q->lock);

	/* wait in kernel */
	while((*_rendezvousp)(mp, (void*)1) == (void*)~0)
		;
#endif //%---------------------------
	mp->inuse = 0;
#endif
}

int
canwlock(RWLock *q)
{
        XX;
#if 0
        return 1;
#else
	lock(&q->lock);
	if (q->readers == 0 && q->writer == 0) {
		/* no one waiting; go for it */
		q->writer = 1;
		unlock(&q->lock);
		return 1;
	}
	unlock(&q->lock);
	return 0;
#endif
}

void
wunlock(RWLock *q)
{
        XX;
#if 0
#else
	QLp *p;

	lock(&q->lock);
	if(q->writer == 0)
		abort();
	p = q->head;
	if(p == nil){
		q->writer = 0;
		unlock(&q->lock);
		return;
	}
	if(p->state == QueuingW){
		/* start waiting writer */
		q->head = p->next;
		if(q->head == nil)
			q->tail = nil;
#if 1 //%---------------------------------------
		{
		    L4_ThreadId_t  tid;
		    L4_MsgTag_t    tag;
		    tid.raw = p->tid;
		    unlock(&q->lock);
		    L4_LoadMR(0, 0);
		    tag = L4_Send(tid);
		    // if (L4_IpcFailed(tag)) XXX;
		}
#else //%--------------------------------------
		unlock(&q->lock);
		while((*_rendezvousp)(p, 0) == (void*)~0)
			;
#endif //%----------------------------------
		return;
	}

	if(p->state != QueuingR)
		abort();

	/* wake waiting readers */
	while(q->head != nil && q->head->state == QueuingR){
		p = q->head;
		q->head = p->next;
		q->readers++;
		while((*_rendezvousp)(p, 0) == (void*)~0)
			;
	}
	if(q->head == nil)
		q->tail = nil;
	q->writer = 0;
	unlock(&q->lock);
#endif
}

void
rsleep(Rendez *r)
{
        XX;
#if 1
#else
	QLp *t, *me;

	if(!r->l)
		abort();
	lock(&r->l->lock);
	/* we should hold the qlock */
	if(!r->l->locked)
		abort();

	/* add ourselves to the wait list */
	me = getqlp();
	me->state = Sleeping;
	if(r->head == nil)
		r->head = me;
	else
		r->tail->next = me;
	me->next = nil;
	r->tail = me;

	/* pass the qlock to the next guy */
	t = r->l->head;
	if(t){
		r->l->head = t->next;
		if(r->l->head == nil)
			r->l->tail = nil;
		unlock(&r->l->lock);
		while((*_rendezvousp)(t, (void*)0x12345) == (void*)~0)
			;
	}else{
		r->l->locked = 0;
		unlock(&r->l->lock);
	}

	/* wait for a wakeup */
	while((*_rendezvousp)(me, (void*)1) == (void*)~0)
		;
	me->inuse = 0;
#endif
}

int
rwakeup(Rendez *r)
{
        XX;

#if 1
        return 1;
#else
	QLp *t;

	/*
	 * take off wait and put on front of queue
	 * put on front so guys that have been waiting will not get starved
	 */
	
	if(!r->l)
		abort();
	lock(&r->l->lock);
	if(!r->l->locked)
		abort();

	t = r->head;
	if(t == nil){
		unlock(&r->l->lock);
		return 0;
	}

	r->head = t->next;
	if(r->head == nil)
		r->tail = nil;

	t->next = r->l->head;
	r->l->head = t;
	if(r->l->tail == nil)
		r->l->tail = t;

	t->state = Queuing;
	unlock(&r->l->lock);
	return 1;
#endif
}

int
rwakeupall(Rendez *r)
{
        XX;

#if 1
        return 1;
#else
	int i;

	for(i=0; rwakeup(r); i++)
		;
	return i;
#endif
}

