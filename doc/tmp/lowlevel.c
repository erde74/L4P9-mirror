::::::::::::::
../src/9/pc/_relief-l4.c
::::::::::::::

ulong paddr(void* laddr)
{
#if 1
    L4_Msg_t msgreg = {{0}};
    L4_MsgTag_t tag;
    L4_ThreadId_t mx_pager;
    mx_pager.raw = MX_PAGER;
    L4_Word_t arg[2];

    //  DBGPRN(">>%s va=0x%x\n", __FUNCTION__, (uint)laddr); 
    L4_MsgClear(&msgreg);
    L4_MsgAppendWord(&msgreg, (L4_Word_t)laddr);
    L4_Set_MsgLabel(&msgreg, GET_PADDR);

    L4_MsgLoad(&msgreg);
    tag = L4_Call(mx_pager);

    L4_MsgStore(tag, &msgreg);
    L4_StoreMRs(1, 2, arg);

    //  DBGPRN("<<%s  ret=0x%x\n", __FUNCTION__, arg[0]); 
    return arg[0];
#else
    return  x_paddr((ulong)laddr);
#endif
}


void* kaddr(ulong paddr)
{    
    return  (void*)x_laddr(paddr);
}

//------------------------------------
long seconds(void)  // tod.c
{   
    // DBGPRN("! %s \n", __FUNCTION__);
    L4_Clock_t  clock;
    uvlong  x;
    uint    sec;

    clock = L4_SystemClock();
    x = clock.raw / 1000000UL; // micro-sec --> sec.
    sec = x;
    return  sec;
}


void delay(int millisecs) 
{
    L4_Word64_t microsecs ;
    if (millisecs == 0) 
        microsecs = 100; 
    else 
        microsecs = millisecs * 500;  // 1000
    L4_Sleep(L4_TimePeriod(microsecs));
}


void microdelay(int microsecs)    //9/pc/i8253.c:291: 
{
  if (microsecs == 0) microsecs = 1;
  L4_Word64_t tt = microsecs;
  // L4_Sleep(L4_TimePeriod(tt));
  {  int i; for(i=0; i<500*tt; i++) ;  }
}


void abort( )
{  TBD;  L4_KDB_Enter("abort");   }


//-------------------------------
void  intrenable(int irq, void (*f)(Ureg*, void*), void* a, int tbdf, char *name)
{
    DBGBRK("intrenable(%d)\n", irq);
    p9_register_irq_handler(irq, f, a, name);  
}

int intrdisable(int irq, void (*f)(Ureg *, void *), void *a, int tbdf, char *name)
{
    DBGBRK("intrenable(%d) \n", irq);
    p9_unregister_irq_handler(irq);  
    return 0;
}


int pcmspecial(char *idstr, ISAConf *isa)
{  TBD;  return 0;}


int okaddr(ulong addr, ulong len, int write)
{  DBGPRN("! %s: %x \n", __FUNCTION__, addr);  return  1;}

void validaddr(ulong addr, ulong len, int write)
{  DBGPRN("! %s(%x, %d) \n", __FUNCTION__, addr, len);   }


void* vmemchr(void *s, int c, int n)
{
  int m;
  ulong a;
  void *t;
 
  a = (ulong)s;
  while(PGROUND(a) != PGROUND(a+n-1)){
    /* spans pages; handle this page */
    m = BY2PG - (a & (BY2PG-1));
    t = memchr((void*)a, c, m);
    if(t)
      return t;
    a += m;
    n -= m;
    if(a < KZERO)
      validaddr(a, 1, 0);
  }
 
  /* fits in one page */
  return memchr((void*)a, c, n);
}


// Followings 6 functions are referenced in port/cache.c 

KMap*   kmap(Page* p){  TBD;  return 0; }

void    kunmap(KMap* p){  TBD; }

Page*   lookpage(Image* _x, ulong _y){  TBD;  return 0; }

void    putpage(Page* p){  TBD; }

Page*   auxpage(void){  TBD;  return 0; }

void    cachepage(Page* _x, Image* _y){  TBD;  }

                                                                                       
::::::::::::::
../src/9/pc/asm-lp49.S
::::::::::::::
/*	int foo(int x, int y)
 *	{  int  a, b;
 *	   ....
 *	}
 *	
 *	|   	 |
 *	+--------+
 *	|   b    |  -8(EBP) 
 *	+--------+
 *	|   a    |  -4(EBP)
 *	+--------+ <-------- EBP
 *	| oldFP  |
 *	+========+ 
 *	|  PC    |
 *	+--------+
 *	|   x    |  8(EBP)
 *	+--------+
 *	|   y	 |  12(EBP)
 *	+--------+
 *	|        |
 */	
	
	.text


/*  
 *	int tas(void *adrs)  
 *	{
 *	  int  tmp; 
 *	  tmp = *addr;
 *	  *addr = 0xdeaddead;
 *	  return  tmp;
 *	}
 */
.global  tas
	.type   tas,@function
		
tas:            
		pushl   %ebp
		movl    %esp, %ebp
		pushl   %ebx	
		movl    $0xdeaddead, %eax
	        movl    8(%ebp), %ebx
	        xchgl   %eax, (%ebx)        /* lock->key */
		popl    %ebx
		leave
	        ret

.global  _xinc
	.type   _xinc,@function	
_xinc:	        /* void inc(int*)  */
		pushl   %ebp
	        movl    %esp, %ebp

	        movl    8(%ebp), %eax
	        lock    
		incl    0(%eax)
		leave
	        ret

.global  _xdec
	 .type   _xdec,@function
	
_xdec:		/* long _xdec(int*) */
		pushl   %ebp
	        movl    %esp, %ebp
		pushl   %ebx			
	        movl    8(%ebp), %ebx
	        xorl    %eax, %eax
	        lock
		decl   0(%ebx)
	        jl     _xdeclt
	        jg     _xdecgt
		popl   %ebx			
		leave
	        ret
_xdecgt:
	        incl   %eax
		popl   %ebx			
		leave
	        ret
_xdeclt:
	        decl    %eax
		popl   %ebx			
		leave
	        ret

.global  xchgw
	.type   xchgw,@function
	
xchgw:		/*  int   xchgw(ushort*, int) ; */
	        pushl   %ebp
	        movl    %esp, %ebp
		pushl   %ebx			
	        movl    12(%ebp), %eax
	        movl    8(%ebp), %ebx
	        xchgw   %ax, (%ebx)
		popl   %ebx			
		leave
	        ret


/*	
 *  int  cmpswap486(long* addr, long old, long new)  
 *  {	
 *	if (*addr == old){
 *	    *addr = new ;
 *           return 1;
 *	}
 *	else
 *	    retrun 0;
 *   }
 */

.global  cmpswap486
	.type   compswap,@function	
cmpswap486:	
	        pushl   %ebp
	        movl    %esp, %ebp
		pushl   %ebx		
		pushl   %ecx				
	        movl    8(%ebp), %ebx  // addr
	        movl    12(%ebp), %eax  // old
	        movl    16(%ebp), %ecx  // new
	        lock
	        cmpxchgl  %ecx, (%ebx) 
	        jnz     didnt
	        movl    $1, %eax
		popl    %ecx
		popl    %ebx			
		leave
	        ret
didnt:
	        xorl    %eax, %eax
		popl    %ecx
		popl    %ebx			
		leave
	        ret

	

/*  int splhi(void); */
.global    splhi
	.type   splhi,@function	

splhi:
		pushl  %ebp
		movl   %esp, %ebp
	shi:	
	        pushfl
	        popl    %eax
	        testl   $0x200, %eax
	        jz      alreadyhi
	        // MOVL    $(MACHADDR+0x04), CX            /* save PC in m->splpc */
	        // MOVL    (SP), BX
	        // MOVL    BX, (CX)
	alreadyhi:	
	        cli
		leave
	        ret

/* int spllo(void); */
.global    spllo
	.type   spllo,@function	
spllo:
	        pushl  %ebp
	        movl   %esp, %ebp
	
	slo:	
	        pushfl
	        popl    %eax
	        testl   $0x200, %eax
	        jnz      alreadylo
	        // MOVL    $(MACHADDR+0x04), CX            /* clear m->splpc */
	        // MOVL    $0, (CX)
	alreadylo:	
	        sti
		leave
	        ret
	
/* void splx(int)  */
.global    splx
	.type   splx,@function	
	
splx:
	        pushl   %ebp
		movl    %esp, %ebp
	
	        movl    8(%ebp), %eax
	        testl   $0x200, %eax
	        jnz     slo
	        jmp     shi

	
/* int  islo(void);*/
.global    islo
	.type   islo,@function	
islo:
	        pushl   %ebp
	        movl    %esp, %ebp
	
	        pushfl
	        popl    %eax
	        andl    $0x200, %eax       /* interrupt enable flag */
		leave
	        ret


::::::::::::::
../src/9/pc/l-l4.s
::::::::::::::
	.file	"func.c"
	
#########  int setlabel(Label *label) ############
#
#	|	|				_________
#	|-------| <-- EBP, ESP        label -->	| SP-o  |	
#	|  FP   | saved EBP			|-------|	
#	|-------|				| PC-o  |	
#	|  PC	| saved PC			|-------|	
#	|-------|				| FP-o	|
#	| label |				---------	
#	|-------|				| EBX ? |
#	|	|				+-------+
		
.globl setlabel
	.type	setlabel,@function
setlabel:
	pushl	%ebp
	movl	%esp, %ebp
#	subl	$0, %esp       # no local vars
	movl	8(%ebp), %eax  # eax=label 

	movl	%ebx, 12(%eax) ## label->ebx := EBX ##??
	
       movl    %esp, 0(%eax)   # label->sp := SP
       movl    4(%esp), %ebx   # PC in stack
       movl    %ebx, 4(%eax)   # label->ip := IP in stack 
       movl    (%esp), %ebx    # FP in stack
       movl    %ebx, 8(%eax)   # label->fp := FP in stack 
       movl    $0, %eax
       leave	               # ESP := EBP;  EBP := Pop();
       ret		

.Lfe3:
	.size	setlabel,.Lfe3-setlabel

	.section	.note.GNU-stack,"",@progbits
	.ident	"GCC: (GNU) 3.2.3 20030502 (Red Hat Linux 3.2.3-20)"


######## plan 9 original ########
#
#TEXT setlabel(SB), $0
#        MOVL    label+0(FP), AX
#        MOVL    SP, 0(AX)        /* store sp */
#        MOVL    0(SP), BX        /* store return pc */
#        MOVL    BX, 4(AX)
#        MOVL    $0, AX           /* return 0 */
#        RET


	
#########  int gotolabel(Label *label) ###########
#
#       |       |                               _________
#       |-------| <-- EBP, ESP        label --> | SP-o  |
#       |  FP   |           |                   |-------|
#       |-------|           |                   | PC-o  |
#       |  PC   |           |                   |-------|
#       |-------|           |                   | FP-o  |
#       | label |           |                   ---------
#       |-------|	    |
#			    |	
#       |       |          \/        
#       |-------| <-- EBP, ESP     
#       |  FP-0 |                  
#       |-------|                  
#       |  PC-o |                  
#       |-------|                  
#       |	|
					
	.text
.globl gotolabel
	.type	gotolabel,@function
gotolabel:
	pushl	%ebp
	movl	%esp, %ebp
#	subl	$0, %esp       # no local vars
	movl	8(%ebp), %eax  # eax := label

       movl    0(%eax), %esp   # SP = label->sp
       movl    4(%eax), %ebx   # label->ip     
       movl    %ebx, 4(%esp)   # stack[IP] := label->ip
       movl    8(%eax), %ebx   # label->fp     
       movl    %ebx, 0(%esp)   # stack[FP] := label->fp

       movl    12(%eax), %ebx   # EBX :	= label->ebx ???     
	
       movl    $1, %eax 
	popl	%ebp	
       ret	
.Lfe2:
	.size	gotolabel,.Lfe2-gotolabel


### plan9 original ###
#		
# TEXT gotolabel(SB), $0
#        MOVL    label+0(FP), AX
#        MOVL    0(AX), SP        /* restore sp */
#        MOVL    4(AX), AX        /* put return pc on the stack */
#        MOVL    AX, 0(SP)
#        MOVL    $1, AX           /* return 1 */
#        RET
	

	
::::::::::::::
../src/9/port/qlock.c
::::::::::::::

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



::::::::::::::
../src/9/port/taslock.c
::::::::::::::

/*  defined in pc/dat.h. <--> libc.h for APL 
 *  struct Lock
 *  {
 *    ulong   key;
 *    ulong   sr;
 *    ulong   pc;
 *    Proc    *p;
 *    Mach    *m;
 *    ushort  isilock;
 *  };
 */


struct
{
	ulong	locks;
	ulong	glare;
	ulong	inglare;
} lockstats;

static void
inccnt(Ref *r)
{
	_xinc(&r->ref);
}

static int
deccnt(Ref *r)
{
	int x;

	x = _xdec(&r->ref);
	if(x < 0)
	  L4_KDB_Enter("deccnt pc");  //%
	return x;
}


static int localtas(void *adrs)
{
          int  tmp;
	  int  *intp = (int*)adrs;
          tmp = *intp;
          *intp = 0xdeaddead;
	  //	  l4printf("%c", (tmp)?'#':'.');
          return  tmp;
}


int
lock(Lock *l)
{
	int i;

	if (l == nil) L4_KDB_Enter("lock(nil)");

	lockstats.locks++;
	//	if(up)	inccnt(&up->nlocks);	/* prevent being scheded */

	if(tas(&l->key) == 0){
	        //	if(up)	up->lastlock = l;
		l->isilock = 0;   //??????
		return 0;
	}
	//	if(up)	deccnt(&up->nlocks);
	//	lockstats.glare++;

	for(;;){
	        //	lockstats.inglare++;
		i = 0;
		while(l->key){
		        /* Priority inversion might be considered ? */ 

			if(i++ > 1000000000){
				i = 0;
				l4printf_b("lockloop<%x:%x> ", l, l->key); 
				_backtrace_();  
			}
		}
		// if(up) inccnt(&up->nlocks);
		if(tas(&l->key) == 0){
		        // if(up) up->lastlock = l;
			l->isilock = 0;
			return 1;
		}
		// if(up) deccnt(&up->nlocks);
	}
	return 0;	/* For the compiler */
}

void
ilock(Lock *l)
{
	ulong x;
	ulong pc;

	//	pc = getcallerpc(&l);
	lockstats.locks++;

	//?	x = splhi();  //% ?
	if(tas(&l->key) != 0){
	  //		lockstats.glare++;
	         // Cannot also check l->pc and l->m here because
		 // they might just not be set yet, or the lock might 
		 // even have been let go.
		if(!l->isilock){
		        l4printf_g("!isilock ");
			_backtrace_();
		  //%   dumplockmem("ilock:", l);
		  //%	panic("corrupt ilock %p pc=%luX m=%p isilock=%d", 
		  //%		l, l->pc, l->m, l->isilock);
		}

		//% if(l->m == MACHP(m->machno))
		//%	panic("ilock: deadlock on cpu%d pc=%luX lockpc=%luX\n", 
		//%		m->machno, pc, l->pc);

		for(;;){
			lockstats.inglare++;
			//?	splx(x);  //% ?
			while(l->key)
				;
			//?	x = splhi();  //% ?
			if(tas(&l->key) == 0)
				goto acquire;
		}
	}
acquire:
	m->ilockdepth++;
	//	if(up)	up->lastilock = l;
	//?	l->sr = x;
	//%	l->pc = pc;
	//	l->p = up;
	l->isilock = 1;
	//%	l->m = MACHP(m->machno);
}

int
canlock(Lock *l)
{
        //  if(up)  inccnt(&up->nlocks);
	if(tas(&l->key)){
	        //  if(up)  deccnt(&up->nlocks);
		return 0;
	}

	//  if(up)  up->lastlock = l;
	l->isilock = 0;
	return 1;
}

void
unlock(Lock *l)
{
	// DBGPRN(">%s() ", __FUNCTION__);
	if (l == 0) L4_KDB_Enter("unlock(nil)");

	//  if(l->key == 0) l4printf_g("unlock: not locked\n");  //%
	//  if(l->isilock)  l4printf_g("unlock of ilock: \n");  //%
	//  if(l->p != up)  l4printf_g("unlock: up changed: \n");  //%
	l->key = 0;

	L4_Yield(); //%%%%

	//% if(up && deccnt(&up->nlocks) == 0 && up->delaysched && islo()){
	           // Call sched if the need arose while locks were held
		   // But, don't do it from interrupt routines, hence the islo() test
	//%        L4_Yield(); //%
		   //% sched();
	//% }
}

void
iunlock(Lock *l)
{
	ulong sr;

	if(l->key == 0)
	        l4printf_g("iunlock: not locked: \n");  //%
	if(!l->isilock)
	        l4printf_g("iunlock of lock: \n");  //%
	//	if(islo())
	//	        l4printf_g("iunlock while lo:\n");  //%

	//?	sr = l->sr;
	//	l->m = nil;
	l->key = 0;
	//%	coherence();

	m->ilockdepth--;
	//	if(up)
	//		up->lastilock = nil;
	//?	splx(sr);
}


::::::::::::::
../src/9/port/proc-l4.c
::::::::::::::

void sleep(Rendez *r, int (*func)(void*), void *arg)  //% L4
{
    DBGPRN("> %s  \n", __FUNCTION__);
    L4_ThreadId_t  from;
    L4_MsgTag_t   tag;
    if ((*func)(arg) == 0) {
        r->p = (Proc*)L4_Myself().raw;  //Type
	tag = L4_Wait(&from);
    }
    DBGPRN("< sleep()  \n");
}
                                                                                         
static int tfn(void *arg)
{       DBGPRN("! %s \n", __FUNCTION__);
    return up->trend == nil || up->tfn(arg);
}
                                                                                         
void twakeup(Ureg* _x, Timer *t)
{
#if 1 //%----------------------------------------
    DBGPRN("! %s \n", __FUNCTION__);
    Proc *p;
    Rendez *trend;
                                                                                         
    p = t->ta;
    trend = p->trend;
    p->trend = 0;
    if(trend)
        wakeup(trend);
#else //plan9 -------------------------------
    Proc *p;
    Rendez *trend;
                                                                                         
    p = t->ta;
    trend = p->trend;
    p->trend = 0;
    if(trend)
        wakeup(trend);
#endif //%------------------------------------
}
                                                                                         
#if 1 //%-------------------------------------------
int  tsleep(Rendez *r, int (*func)(void*), void *arg, ulong micros)  //% ONERR ?
{
    L4_ThreadId_t  from;
    L4_MsgTag_t   tag;
                                                                                         
    DBGPRN("> %s() \n", __FUNCTION__);
    if ((*func)(arg) == 0) {
        r->p = (Proc*)L4_Myself().raw;  //Type
	tag = L4_Wait_Timeout(L4_TimePeriod(micros), &from );
	// if (L4_IpcFailed(tag) && L4_ErrorCode()==EXXX)
	//   return  ONERR;
    }
    DBGBRK("< TSLEEP \n");
    return  1;
}
#else //plan9---------------------------------------
void
tsleep(Rendez *r, int (*fn)(void*), void *arg, ulong ms)
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
#endif //%------------------------------------------
                                                                                         
                                                                                         
Proc* wakeup(Rendez *r)   //% L4
{
#if 1 //%------------------------------------
    L4_ThreadId_t whom;
    L4_Msg_t msgreg;
    L4_MsgTag_t tag;
                                                                                         
    DBGPRN("%s() \n", __FUNCTION__);
    whom.raw = (unsigned)r->p;  //Type...
                                                                                         
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, 7);
    L4_MsgLoad(&msgreg);
                                                                                         
    tag = L4_Send(whom);
    return (Proc*) whom.raw;
#else //plan9------------------------------
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
#endif //---------------------------------
}

                                                                                                      
int postnote(Proc *p, int dolock, char *n, int flag)
{
    DBGPRN("! %s() \n", __FUNCTION__);
    return 0;
}


::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
../src/libc/9sys/qlock-l4.c    ===== LIBC ========
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

static struct {
	QLp	*p;
	QLp	x[1024];
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
}

void
qlock(QLock *q)
{
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
	unlock(&q->lock);

	/* wait */
	while((*_rendezvousp)(mp, (void*)1) == (void*)~0)
		;
	mp->inuse = 0;
}

void
qunlock(QLock *q)
{
 	QLp *p;

	lock(&q->lock);
	p = q->head;
	if(p != nil){
		/* wakeup head waiting process */
		q->head = p->next;
		if(q->head == nil)
			q->tail = nil;
		unlock(&q->lock);
		while((*_rendezvousp)(p, (void*)0x12345) == (void*)~0)
			;
		return;
	}
	q->locked = 0;
	unlock(&q->lock);
}

int
canqlock(QLock *q)
{
	if(!canlock(&q->lock))
		return 0;
	if(!q->locked){
		q->locked = 1;
		unlock(&q->lock);
		return 1;
	}
	unlock(&q->lock);
	return 0;
}

void
rlock(RWLock *q)
{
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
	unlock(&q->lock);

	/* wait in kernel */
	while((*_rendezvousp)(mp, (void*)1) == (void*)~0)
		;
	mp->inuse = 0;
}

int
canrlock(RWLock *q)
{
	lock(&q->lock);
	if (q->writer == 0 && q->head == nil) {
		/* no writer; go for it */
		q->readers++;
		unlock(&q->lock);
		return 1;
	}
	unlock(&q->lock);
	return 0;
}

void
runlock(RWLock *q)
{
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
	unlock(&q->lock);

	/* wakeup waiter */
	while((*_rendezvousp)(p, 0) == (void*)~0)
		;
}

void
wlock(RWLock *q)
{
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
	unlock(&q->lock);

	/* wait in kernel */
	while((*_rendezvousp)(mp, (void*)1) == (void*)~0)
		;
	mp->inuse = 0;
}

int
canwlock(RWLock *q)
{
	lock(&q->lock);
	if (q->readers == 0 && q->writer == 0) {
		/* no one waiting; go for it */
		q->writer = 1;
		unlock(&q->lock);
		return 1;
	}
	unlock(&q->lock);
	return 0;
}

void
wunlock(RWLock *q)
{
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
		unlock(&q->lock);
		while((*_rendezvousp)(p, 0) == (void*)~0)
			;
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
}

void
rsleep(Rendez *r)
{
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
}

int
rwakeup(Rendez *r)
{
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
}

int
rwakeupall(Rendez *r)
{
	int i;

	for(i=0; rwakeup(r); i++)
		;
	return i;
}

