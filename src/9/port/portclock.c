//% 

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../pc/io.h"
#include "ureg.h"

#if 1 //%------------------------------------------------------
#include  <l4all.h>
#include  <l4/l4io.h>

#define  _DBGFLG  0
#include <l4/_dbgpr.h>
#define  PRN  l4printf_r

// #define TBD tobedefined(__FUNCTION__)

typedef union {
    uvlong         raw;
    struct {
        uint         low;
        uint         high;
    } X;
} i64_i32_t;

extern L4_ThreadId_t create_start_thread(unsigned pc, unsigned sp);
extern uvlong microsectime();
extern char* vlong2str(vlong val, char *str);
extern char* vlong2a(vlong val);


void todinit()
{
  TBD;
}

static L4_ThreadId_t  timer_thread;

static L4_ThreadId_t kproc_l4(char *name, void (*func)(void *), void *arg, unsigned *sp)
{
  DBGPRN("! %s(%s %x %x) \n", __FUNCTION__, name, func, arg);
  L4_ThreadId_t  tid;
  *(--sp) = 0; // no meaning
  *(--sp) = (unsigned)arg;  
  *(--sp) = 0; // Return address ?
  tid = create_start_thread((unsigned)func, (unsigned)sp);
  return  tid;
}


void timerset(uvlong  microsec)   // 
{
    //    timerset(uvlong); // in devarch.c 
    //   (*arch->timerset)(tm);  // in i8253.c  
    //    after "tm", timerintr() is activated.

    i64_i32_t  x;
    x.raw = microsec;
    DBGPRN("> %s(%s)\n", __FUNCTION__, vlong2a(x.raw));
    //    DBGPRN("> %s(%u-%u)\n", __FUNCTION__, x.X.high, x.X.low);

    L4_Msg_t     _MRs;
    L4_MsgTag_t  tag;
    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, x.X.low);   // MR1
    L4_MsgAppendWord(&_MRs, x.X.high);  // MR2
    // L4_SetMsgLabel(&_MRs, 0);
    L4_MsgLoad(&_MRs);
    tag = L4_Send(timer_thread);
    if (L4_IpcFailed(tag)) 
      L4_KDB_Enter("ipc");
}
#endif //%---------------------------------------------------


//  struct Timer  // defined in portdat.h
//  {
//    int     tmode;          
//    vlong   tns;            
//    void    (*tf)(Ureg*, Timer*);
//    void    *ta;
//    Lock    _lock; //%
//    Timers  *tt;            
//    vlong   twhen;          
//    Timer   *tnext;
//  };


struct Timers
{
        Lock    _lock;   //%
	Timer	*head;
};

static Timers timers[MAXMACH];

ulong intrcount[MAXMACH];
ulong fcallcount[MAXMACH];

static vlong
tadd(Timers *tt, Timer *nt)
{
	Timer *t, **last;

	/* Called with tt locked */
	//% assert(nt->tt == nil);
	switch(nt->tmode){
	default:
		panic("timer");
		break;
       case Trelative:
		if(nt->tns <= 0)
			nt->tns = 1;
#if 1 //%----------------
		nt->twhen = microsectime() + (nt->tns);
#else //plan9------------
		nt->twhen = fastticks(nil) + ns2fastticks(nt->tns);
#endif //%---------------
		break;

	case Tperiodic:
	        //% assert(nt->tns >= 100000);	/* At least 100 micro secs period */
		if(nt->twhen == 0){
			/* look for another timer at same frequency for combining */
			for(t = tt->head; t; t = t->tnext){
				if(t->tmode == Tperiodic && t->tns == nt->tns)
					break;
			}
			if (t)
				nt->twhen = t->twhen;
			else
			        nt->twhen = microsectime();  //% fastticks(nil);
		}
#if 1 //%-------------------------
		nt->twhen += (nt->tns);
#else //plan9--------------------
		nt->twhen += ns2fastticks(nt->tns);
#endif //%------------------------
		break;
	}

	for(last = &tt->head; (t = *last); last = &t->tnext){
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

static uvlong
tdel(Timer *dt)
{

	Timer *t, **last;
	Timers *tt;

	tt = dt->tt;
	if (tt == nil)
		return 0;
	for(last = &tt->head; (t = *last); last = &t->tnext){
		if(t == dt){
		        //% assert(dt->tt);
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
void
timeradd(Timer *nt)
{
	Timers *tt;
	vlong when;

	/* Must lock Timer struct before Timers struct */
	ilock(&nt->_lock);  //% (nt)
	if((tt = nt->tt)){
		ilock(&tt->_lock);  //% (tt)
		tdel(nt);
		iunlock(&tt->_lock);  //% (tt)
	}

	tt = &timers[0];   //%plan9  	tt = &timers[m->machno];

	ilock(&tt->_lock);  //% (tt)
	when = tadd(tt, nt);
	DBGPRN("%s():%s\n", __FUNCTION__, vlong2a(when));
	if(when)
		timerset(when);
	iunlock(&tt->_lock);  //% (tt)
	iunlock(&nt->_lock);  //% (nt)
}


void
timerdel(Timer *dt)
{
	Timers *tt;
	uvlong when;

	ilock(&dt->_lock);  //% (dt)
	if((tt = dt->tt)){
		ilock(&tt->_lock);  //% (tt)
		when = tdel(dt);
		if(when && tt == &timers[0])      //% plan9  timers[m->machno]
			timerset(tt->head->twhen);
		iunlock(&tt->_lock);  //% (tt)
	}
	iunlock(&dt->_lock);  //% (dt)
}

#if 0 //%-------------------------------------------
void
hzclock(Ureg *ur)
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
#endif //----------------------------------------

#if 1 //%-----------------------------------------
static void timer_thread_body()   //  TBD
{
    L4_MsgTag_t     tag;
    L4_Msg_t        _MRs;
    L4_Time_t       tm = L4_Never; 
    L4_ThreadId_t   from;
    i64_i32_t       microsec; 
    i64_i32_t       waittime;

    Timer  *t;
    Timers *tt;
    uvlong  when, now;
    Ureg   *u = 0;
    static int  cnt = 0;

//print(">%s():TID=%x\n", __FUNCTION__, L4_Myself()); L4_KDB_Enter();

    for(;;) {
    Outerloop:     
        tag = L4_Wait_Timeout(tm, &from);
	now = microsectime();

//if(((++cnt)%10000)==0)print("#### timer_thread [%d] ###\n", cnt);

	if (L4_IpcSucceeded(tag)) {  // timer-set
	    L4_MsgStore(tag, &_MRs);
	    microsec.X.low  = L4_MsgWord(&_MRs, 0);  // MR1
	    microsec.X.high = L4_MsgWord(&_MRs, 1);  // MR2

	    if (microsec.raw > (uvlong)(now + 1000L) )
	        waittime.raw = microsec.raw - now;
	    else
	        waittime.raw = 1000L;


	    DBGPRN("now=%s\n", vlong2a(now));
	    DBGPRN("reqest=%s\n", vlong2a(microsec.raw));
	    DBGPRN("delta=%s\n", vlong2a(waittime.raw));
	    // DBGPRN("delta-time=%u,%u\n", waittime.X.high, waittime.X.low);

	    tm = L4_TimePeriod(waittime.raw);
	    continue;
	}
	else {  // head timer expired 
	    // DBGPRN("portclock-timer\n");
	    tt = &timers[0];

	    while((t = tt->head)){
	        /*
		 * No need to ilock t here: any manipulation of t
		 * requires tdel(t) and this must be done with a
		 * lock to tt held.  We have tt, so the tdel will
		 * wait until we're done
		 */
		when = t->twhen;
		if(when > now){
		    goto  Outerloop;
		}

		tt->head = t->tnext;
		t->tt = nil;
		if(t->tf)
			(*t->tf)(u, t);

		if(t->tmode == Tperiodic)
			tadd(tt, t);
	    }
	}
    }
}

#else //plan9-------------------------------------
void
timerintr(Ureg *u, uvlong _x)  //%
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
	ilock(&tt->_lock);  //% (tt)
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
			iunlock(&tt->_lock);  //% (tt)
			if(callhzclock)
				hzclock(u);
			return;
		}
		tt->head = t->tnext;
		//%  assert(t->tt == tt);
		t->tt = nil;
		fcallcount[m->machno]++;
		iunlock(&tt->_lock);  //% (tt)
		if(t->tf)
			(*t->tf)(u, t);
		else
			callhzclock++;
		ilock(&tt->_lock);  //% (tt)
		if(t->tmode == Tperiodic)
			tadd(tt, t);
	}
	iunlock(&tt->_lock);  //% (tt)
}
#endif //%------------------------------------------


static unsigned timerstk[512];

void
timersinit(void)
{
	Timer *t;

	DBGPRN("> %s()\n", __FUNCTION__);
#if 0
	kproc("timer_thread", timer_thread_body, 0);
#else
	timer_thread = kproc_l4("timer_thread", timer_thread_body, 0, &timerstk[500]);  //%
	PRN("timer_thread=%X \n", timer_thread); //%
	L4_ThreadSwitch(timer_thread);
#endif
	todinit();
#if 0 //%-------------
	t = malloc(sizeof(*t));
	t->tmode = Tperiodic;
	t->tt = nil;
	t->tns = 10000;   //%plan9  t->tns = 1000000000/HZ;
	t->tf = nil;
	timeradd(t);
#endif //---------------
}



Timer*
addclock0link(void (*f)(void), int ms)
{
	Timer *nt;
	uvlong when;

	/* Synchronize to hztimer if ms is 0 */
	nt = malloc(sizeof(Timer));
	if(ms == 0)
		ms = 1000/HZ;
	nt->tns = (vlong)ms;	//% plan9 nt->tns = (vlong)ms*1000000LL;
	nt->tmode = Tperiodic;
	nt->tt = nil;
	nt->tf = (void (*)(Ureg*, Timer*))f;

	ilock(&timers[0]._lock);    //% (&timers[0])
	when = tadd(&timers[0], nt);
	if(when)
		timerset(when);
	iunlock(&timers[0]._lock);    //% (&timers[0])
	return nt;
}

#if 0 //%-------------------------------------------
/*
 *  This tk2ms avoids overflows that the macro version is prone to.
 *  It is a LOT slower so shouldn't be used if you're just converting
 *  a delta.
 */
ulong
tk2ms(ulong ticks)
{
	uvlong t, hz;

	t = ticks;
	hz = HZ;
	t *= 1000L;
	t = t/hz;
	ticks = t;
	return ticks;
}

ulong
ms2tk(ulong ms)
{
	/* avoid overflows at the cost of precision */
	if(ms >= 1000000000/HZ)
		return (ms/1000)*HZ;
	return (ms*HZ+500)/1000;
}

#endif //%-----------------------------------------------------------
