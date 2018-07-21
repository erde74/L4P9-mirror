//%
#include  <l4all.h>      //%
#include  <lp49/l_actobj.h> //%

#include <u.h>
#include <libc.h>
#include <draw.h>
//#include <thread.h>	// HK 20100131
#include <cursor.h>
#include <mouse.h>    //% #include <mouse.h>
#include <keyboard.h> //% #include <keyboard.h>
#include <frame.h>
#include <fcall.h>

#include "dat.h"
#include "fns.h"

#define  ARECVP(varptr) {L4_ThreadId_t sender;	\
      l_arecv0(&sender, INF, nil); l_getarg(nil, "i1", varptr); }

//%   static Channel*	ctimer;	/* chan(Timer*)[100] */
static Timer *timer;

#if 1 //%---------------
extern  L4_Clock_t  L4_SystemClock();
static
uint
msec(void)
{
    L4_Clock_t  clk;
    uvlong      uv;
    clk = L4_SystemClock();
    uv = clk.raw;
    return  (uint)(uv >> 20);
}
#else //%original
static
uint
msec(void)
{
	return nsec()/1000000;
}
#endif //%-----------------------

void
timerstop(Timer *t)
{
  DXX(0);
	t->next = timer;
	timer = t;
}

void
timercancel(Timer *t)
{
  DXX(0);
	t->cancel = TRUE;
}

static
void
timerthread(void* _x)  //%
{
	int i, nt, na, dt, del;
	Timer **t, *x;
	uint old, new;
	L_mbuf *mbuf;		// HK 20091231

	rfork(RFFDG);
	l_thread_setname("TIMERTHREAD");  //%
	t = nil;
	na = 0;
	nt = 0;
	old = msec();

	for(;;){
DXX(0);
		sleep(1);	/* will sleep minimum incr */
		new = msec();
		dt = new-old;
		old = new;
		if(dt < 0)	/* timer wrapped; go around, losing a tick */
			continue;
		for(i=0; i<nt; i++){
			x = t[i];
			x->dt -= dt;
			del = 0;
			if(x->cancel){
				timerstop(x);
				del = 1;
			}else if(x->dt <= 0){
				/*
				 * avoid possible deadlock if client is
				 * now sending on ctimer
				 */
			  //%	if(nbsendul(x->c, 0) > 0)
			  //%		del = 1;
			}
			if(del){
				memmove(&t[i], &t[i+1], (nt-i-1)*sizeof t[0]);
				--nt;
				--i;
			}
		}
		if(nt == 0){
		        //%  x = recvp(ctimer);
			l_arecv0(nil, INF, &mbuf);	// HK 20091231
	gotit:
			l_getarg(mbuf, "i1", &x);	// HK 20091231
			if(nt == na){
				na += 10;
				t = realloc(t, na*sizeof(Timer*));
				if(t == nil){
				        fprint(2, "!timerthread:abort()\n");
					abort();
				}
			}
			t[nt++] = x;
			old = msec();
		}
		//%		if(nbrecv(ctimer, &x) > 0)
		//%			goto gotit;
		l_arecv0(nil, 0, &mbuf);
		if (mbuf != nil) goto gotit;	// HK 20091231
	}
}

void
timerinit(void)
{
        //%	ctimer = chancreate(sizeof(Timer*), 100);
        l_thread_create(timerthread, STACK, nil);  //%
	//%	proccreate(timerthread, nil, STACK);
}

/*
 * timeralloc() and timerfree() don't lock, so can only be
 * called from the main proc.
 */

Timer*
timerstart(int dt)
{
	Timer *t;

DXX(0);
	t = timer;
	if(t)
		timer = timer->next;
	else{
		t = emalloc(sizeof(Timer));
		//%	t->c = chancreate(sizeof(int), 0);
	}
	t->next = nil;
	t->dt = dt;
	t->cancel = FALSE;
	//%	sendp(ctimer, t);
	return t;
}
