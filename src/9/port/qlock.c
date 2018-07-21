//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%  (m) KM-NII  2007-07-29

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"

#if 1 //%------------------------------------------------------
#include  <l4all.h>
#include  <l4/l4io.h>

#define  _DBGFLG  1
#include <l4/_dbgpr.h>
#endif //%-----------------------------------------------------

/********  to be debugged  ****************
 *  Modified to be run on L4, but not yet debugged. 
 *  L4 msg check : to be strengthened.
 *
 */

// struct QLock
// {
//   Lock    use;            /* to access Qlock structure */
//   Threadque    *head;          /* next thread waiting for object */
//   Threadque    *tail;          /* last thread waiting for object */
//   int     locked;         /* flag */
// };
// struct RWlock
// {
//    Lock    use;
//    Threadque    *head;          /* list of waiting threads */
//    Threadque    *tail;
//    ulong   wpc;            /* pc of writer */
//    Proc    *wproc;         /* writing proc */
//    int     readers;        /* number of readers */
//    int     writer;         /* number of writers */
// };


struct {
	ulong rlock;
	ulong rlockq;
	ulong wlock;
	ulong wlockq;
	ulong qlock;
	ulong qlockq;
} rwstats;

void
qlock(QLock *q)
{
	Threadque  *p;   //% Proc *p;
	Threadque  *r;   //% 
	L4_ThreadId_t  tid;  //%
	L4_Msg_t       _MRs;

	if(m->ilockdepth != 0)
		l4printf_g("qlock: ilockdepth %d\n",  m->ilockdepth);
	if(up != nil && up->nlocks.ref)
		l4printf_g("qlock: nlocks %lud\n", up->nlocks.ref);

	if(q->use.key == 0x55555555)
		panic("qlock: q %p, key 5*\n", q);
	lock(&q->use);
	rwstats.qlock++;
	if(!q->locked) {
		q->locked = 1;
		unlock(&q->use);
		return;
	}
	//%	if(up == 0)
	//%		panic("qlock");

	rwstats.qlockq++;
	r = (Threadque*)malloc(sizeof(Threadque));
	r->qnext = nil;
	r->tid = L4_Myself(); 
	r->state = Queueing;
	  
	p = q->tail;
	if(p == 0)
	        q->head = r;  //%  up;
	else
	        p->qnext = r;  //% up;
	q->tail = r;  //% up;

	//	up->qpc = getcallerpc(&q);
	unlock(&q->use);
	L4_LoadMR(0, 0); //%
	L4_Wait(&tid);    //% sched();
}

int
canqlock(QLock *q)
{
	if(!canlock(&q->use))
		return 0;
	if(q->locked){
		unlock(&q->use);
		return 0;
	}
	q->locked = 1;
	unlock(&q->use);
	return 1;
}

void
qunlock(QLock *q)
{
	Threadque  *p;   //% Proc *p;
	L4_ThreadId_t  tid;  //%

	lock(&q->use);
	p = q->head;
	if(p){
		q->head = p->qnext;
		if(q->head == 0)
			q->tail = 0;
		unlock(&q->use);
		tid = p->tid;
		free(p);

		//% buildup L4 message
		L4_LoadMR(0, 0);
		L4_Send(tid);   //%   ready(p);
		return;
	}
	q->locked = 0;
	unlock(&q->use);
}

void
rlock(RWlock *q)
{
	Threadque  *p;  //% Proc *p;
	Threadque  *r;  //% 
	L4_ThreadId_t  tid;  //%
	L4_Msg_t  _MRs;  //%

	lock(&q->use);
	rwstats.rlock++;
	if(q->writer == 0 && q->head == nil){
		/* no writer, go for it */
		q->readers++;
		unlock(&q->use);
		return;
	}

	rwstats.rlockq++;
	r = (Threadque*)malloc(sizeof(Threadque)); //%
	r->qnext = nil;  //%
	r->state = QueueingR;	//%
	r->tid = L4_Myself();   //%

	p = q->tail;
	//	if(up == nil)
	//		panic("rlock");
	if(p == 0)
	        q->head = r;   //% up;
	else
	        p->qnext = r;  //%  up;
	q->tail = r;   //% up;
	//%	up->qnext = 0;
	//%	up->state = QueueingR;
	unlock(&q->use);
	//%	sched();
	L4_LoadMR(0, 0); //%
	L4_Wait(&tid);    //% sched();
}

void
runlock(RWlock *q)
{
	Threadque   *p;    //% Proc *p;
	L4_ThreadId_t  tid;

	lock(&q->use);
	p = q->head;
	if(--(q->readers) > 0 || p == nil){
		unlock(&q->use);
		return;
	}

	/* start waiting writer */
	if(p->state != QueueingW)
		panic("runlock");
	q->head = p->qnext;
	if(q->head == 0)
		q->tail = 0;
	q->writer = 1;
	unlock(&q->use);
	tid = p->tid;
	free(p);

	//%	ready(p);	
	L4_LoadMR(0, 0);
	L4_Send(tid);   //%   ready(p);
}

void
wlock(RWlock *q)
{
	Threadque  *p;  //%  Proc *p;
	Threadque  *r;  //%  
	L4_ThreadId_t  tid; //%

	lock(&q->use);
	rwstats.wlock++;
	if(q->readers == 0 && q->writer == 0){
		/* noone waiting, go for it */
		q->wpc = getcallerpc(&q);
		q->wproc = up;
		q->writer = 1;
		unlock(&q->use);
		return;
	}

	/* wait */
	rwstats.wlockq++;
	p = q->tail;

	r = (Threadque*)malloc(sizeof(Threadque)); //%
	r->qnext = nil;
	r->tid = L4_Myself();
	r->state = QueueingW;

	//%	if(up == nil)
	//%		panic("wlock");
	if(p == nil)
	        q->head = r;  //% up;
	else
	        p->qnext = r;  //%  up;
	//%	q->tail = up;
	//%	up->qnext = 0;
	//%	up->state = QueueingW;
	unlock(&q->use);
	//%	sched();

	L4_LoadMR(0, 0);
	L4_Wait(&tid);  //%
}

void
wunlock(RWlock *q)
{
	Threadque  *p;  //%  Proc *p;
	L4_ThreadId_t  tid;

	lock(&q->use);
	p = q->head;
	if(p == nil){
		q->writer = 0;
		unlock(&q->use);
		return;
	}
	if(p->state == QueueingW){
		/* start waiting writer */
		q->head = p->qnext;
		if(q->head == nil)
			q->tail = nil;
		unlock(&q->use);
		//%		ready(p);
		tid = p->tid;   //%
		free(p);        //%
		L4_LoadMR(0, 0);  //%
		L4_Send(tid);   //%

		return;
	}

	if(p->state != QueueingR)
		panic("wunlock");

	/* waken waiting readers */
	while(q->head != nil && q->head->state == QueueingR){
		p = q->head;
		q->head = p->qnext;
		q->readers++;

		//%		ready(p);
                tid = p->tid;   //%
                free(p);        //%
                L4_LoadMR(0, 0);  //%
                L4_Send(tid);   //%
	}
	if(q->head == nil)
		q->tail = nil;
	q->writer = 0;
	unlock(&q->use);
}

/* same as rlock but punts if there are any writers waiting */
int
canrlock(RWlock *q)
{
	lock(&q->use);
	rwstats.rlock++;
	if(q->writer == 0 && q->head == nil){
		/* no writer, go for it */
		q->readers++;
		unlock(&q->use);
		return 1;
	}
	unlock(&q->use);
	return 0;
}
