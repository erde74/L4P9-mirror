/*
 * TO BE DONE: IRQ-sharing
 */

#include  <l4all.h>
#include  <l4/l4io.h>

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_r
#define   NULL  (void*)0

extern  void*   malloc(unsigned int);
extern  int requestAssociateInterrupt (L4_ThreadId_t InterruptThread, 
				       L4_ThreadId_t HandlerThread);
extern  int requestDeassociateInterrupt( L4_ThreadId_t irq_tid );
extern  int get_new_tid_and_utcb (L4_ThreadId_t *tid, unsigned int *utcb);
extern  int requestThreadControl(L4_ThreadId_t dest, L4_ThreadId_t space,
				 L4_ThreadId_t sched, L4_ThreadId_t pager, L4_Word_t utcb);

static L4_ThreadId_t irq_thread_tid;  


typedef  struct irqaction  irqaction_t;

struct irqaction {
    int    irq;
    void   (*func)(int irq, void* arg);
    void   *arg;
    char   name[28];
    irqaction_t  *next;
} ;

static irqaction_t *irq_handlers[16]
  = {0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0};


void p9_register_irq_handler(int irq, void (*func)(void*, void*), 
			     void* arg, char *name)
{
    L4_ThreadId_t tid = L4_nilthread;
    int   i;
    irqaction_t *actionp, *nextp;

    DBGPRN(">%s(%d, %x, %x, %s)\n", __FUNCTION__, irq, func, arg, name);
  
    actionp = (irqaction_t *)malloc(sizeof(irqaction_t));
    if (actionp==0) L4_KDB_Enter("no mem in register_irq_handler");
  
    actionp->func = func;
    actionp->irq = irq;
    actionp->arg = arg;
    actionp->next = NULL;
    name[27] = 0;
    for(i = 0; i<28; i++) 
        if ((actionp->name[i] = name[i]) == 0) 
	    break;
    actionp->name[i] = 0;

  
    if (irq_handlers[irq] == NULL) {
        irq_handlers[irq] = actionp;
        tid.global.X.thread_no = irq;
	tid.global.X.version = 1;

	if( !requestAssociateInterrupt(tid, irq_thread_tid) )
	    L4_KDB_Enter("ERR in associate interrupt");

	DBGPRN("<%s():%d\n", __FUNCTION__, L4_ErrorCode());
    }
    else {
        nextp = irq_handlers[irq];
	while (nextp->next)
	    nextp = nextp->next;
	nextp->next = actionp;
    }
} 


void  intrenable(int irq, void (*f)(void*, void*), void* a, int tbdf, char *name)
{
    DBGBRK(">intrenable(%d)\n", irq);
    p9_register_irq_handler(irq, f, a, name);
}


//-----------------------------------------------
void p9_unregister_irq_handler(int irq)
{
  L4_ThreadId_t  tid = L4_nilthread;
  irqaction_t *nextp;

  l4printf_r("TBD:p9_unregister_irq_handler\n");

  nextp = irq_handlers[irq];
  /**********************************************************/

  tid.global.X.thread_no = irq;
  tid.global.X.version = 1;

  if( !requestDeassociateInterrupt(tid) )
      L4_KDB_Enter("ERR in Deassociate interrupt");
  // DBGPRN("requestDeassociateInterrupt");
} 


int intrdisable(int irq, void (*f)(void *, void *), void *a, int tbdf, char *name)
{
  DBGBRK(">intrdisable(%d) \n", irq);
  p9_unregister_irq_handler(irq);
  return 0;
}


//-------------------------------------------
static void p9_irq_thread_body(  )
{
    L4_Word_t tid_user_base = L4_ThreadIdUserBase(L4_GetKernelInterface());
    L4_ThreadId_t tid = L4_nilthread;
    L4_ThreadId_t ack_tid = L4_nilthread;
    //    int  irq;
    irqaction_t   *actionp;
    L4_MsgTag_t tag;

    //PRN("\n IRQ_THREAD=%X \n", L4_Myself()); L4_KDB_Enter("");

 Retry:    
    tag = L4_Wait( &tid );

    for (;;)
    {
        //PRN("IRQ<%x,%x>  ", tid.global.X.thread_no, tid.raw); 

	if( L4_IpcFailed(tag) )	{
	    L4_KDB_Enter("IRQ IPC ");
	    goto Retry;
	}
 
	// Received message.
	if(tid.global.X.thread_no < tid_user_base){ //Hardware IRQ.
	    L4_Word_t irq = tid.global.X.thread_no;

	    actionp = irq_handlers[irq];
	    while (actionp) {
	        if (actionp->func)
		    actionp->func(irq, actionp->arg);
		actionp = actionp->next;
	    }
	    
	    // Send an ack message to the L4 interrupt thread. ????
	    //  the main thread should be able to do this via propagated IPC.
	    ack_tid.global.X.thread_no = irq;
	    ack_tid.global.X.version = 1;
	    L4_LoadMR( 0, 0 );  // Ack msg.

	    tag = L4_ReplyWait( ack_tid, &tid );
	    ack_tid = L4_nilthread;
	}
	else  L4_KDB_Enter("IRQ MSG");
    } /* while */
}


//-----------------------------------------
static int irqstk[1024];
extern	L4_ThreadId_t create_start_thread(unsigned pc, unsigned stacktop);

L4_ThreadId_t p9_irq_init()
{
    int   rc;
    static int iamalive = 0;

    if (iamalive == 0)  {
	L4_ThreadId_t  myself = L4_Myself(); 
	L4_ThreadId_t  mypager = L4_Pager();
	L4_Word_t  utcb;
        iamalive = 1;

#if 1 //----------------
	irq_thread_tid = create_start_thread((L4_Word_t)p9_irq_thread_body, &irqstk[1020]);
#else //-----------------
	get_new_tid_and_utcb(& irq_thread_tid, (void*)& utcb);
	PRN("irq_thread=%X \n", irq_thread_tid.raw); //%
	rc = requestThreadControl(irq_thread_tid, myself, myself, 
				  mypager, utcb);
	DBGPRN("<> irq_thread=%X utcb=%X rc=%d \n", irq_thread_tid, utcb, rc);
	L4_Start_SpIp(irq_thread_tid, (L4_Word_t) &irqstk[1020], 
		      (L4_Word_t)p9_irq_thread_body);
#endif //-----------------
    }
    return irq_thread_tid;
}

