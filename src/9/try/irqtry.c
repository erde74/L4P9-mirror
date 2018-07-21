
#include <l4/kip.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kdebug.h>
#include <l4/l4io.h>

#define  printf l4printf
#define  DIRECTPRINT     set_l4print_direct(1)   //%%%
#define  bool  int

#define BUSY_WAITING  0
#define MILISEC   1000

L4_ThreadId_t master_tid, pager_tid, ping_tid, pong_tid, irq_tid;

L4_Word_t ping_stack[1024] ;
L4_Word_t pong_stack[1024] ;
L4_Word_t pager_stack[1024];
L4_Word_t irq_stack[1024]  ;  

//===================================================
#define KBD_STATUS_REG          0x64
#define KBD_CNTL_REG            0x64
#define KBD_DATA_REG            0x60

#define KBD_STAT_OBF            0x01    /* Keyboard output buffer full */

int inb(int port)
{
    unsigned char data;
    __asm__ __volatile__("inb %%dx,%0" : "=a" (data) : "d" (port));
    return (int)data;
}

void outb(int port, int value)
{
  unsigned char data = (unsigned char)value;
  __asm__ __volatile__("outb %0,%%dx" : : "a" (data), "d" (port));
}

//#define kbd_read_input() inb(KBD_DATA_REG)
//#define kbd_read_status() inb(KBD_STATUS_REG)

static unsigned char keyb_layout[128] =
  "\000\0331234567890-+\177\t"                    /* 0x00 - 0x0f */
  "qwertyuiop[]\r\000as"                          /* 0x10 - 0x1f */
  "dfghjkl;'`\000\\zxcv"                          /* 0x20 - 0x2f */
  "bnm,./\000*\000 \000\201\202\203\204\205"      /* 0x30 - 0x3f */
  "\206\207\210\211\212\000\000789-456+1"         /* 0x40 - 0x4f */
  "230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
  "\r\000/";                                      /* 0x60 - 0x6f */


#if BUSY_WAITING  //=========================================================

int getc()
{
    static unsigned char last_key = 0;
    char c;
    while(1) {
        unsigned char status = inb(KBD_STATUS_REG);  
	//  printf("%d ", status);
	while (status & KBD_STAT_OBF) {
	  unsigned char scancode;
	  scancode = inb(KBD_DATA_REG);
	  if (scancode & 0x80)
	    last_key = 0;
	  else if (last_key != scancode)
	    {
	      printf("<%c>\n", keyb_layout[scancode]);
	      //printf("kbd: %d, %d, %c\n", scancode, last_key, keyb_layout[scancode]);
	      last_key = scancode;
	      c = keyb_layout[scancode];
	      if (c > 0)  return c;
	    }
	}
	L4_Sleep(L4_TimePeriod(20 * MILISEC)); 
    }
}

#else //===============================================

enum {
  Data=           0x60,           /* data port */
  Status=         0x64,           /* status port */
  Inready=       0x01,           /*  input character ready */
  Outbusy=       0x02,           /*  output busy */
                                                                                        
  Cmd=            0x64,           /* command port (write only) */
};                                                                                        

enum  {    /* controller command byte */
    Cscs1=          (1<<6),         /* scan code set 1 */
    Cauxdis=        (1<<5),         /* mouse disable */
    Ckbddis=        (1<<4),         /* kbd disable */
    Csf=            (1<<2),         /* system flag */
    Cauxint=        (1<<1),         /* mouse interrupt enable */
    Ckbdint=        (1<<0),         /* kbd interrupt enable */
};


static unsigned char  ccc;

static int outready(void)
{
    int tries;
    for(tries = 0; (inb(Status) & Outbusy); tries++){
        if(tries > 500)	    return -1;
	L4_Sleep(L4_TimePeriod(2 * MILISEC)); //  delay(2);
  }
  return 0;
}

static int inready(void)
{
    int tries;
    for(tries = 0; !(inb(Status) & Inready); tries++){
        if(tries > 500)	    return -1;
	L4_Sleep(L4_TimePeriod(2 * MILISEC)); //  delay(2);
  }
  return 0;
}

void kbdinit(void)
{
    int c;
 
    /* wait for a quiescent controller */
    while((c = inb(Status)) & (Outbusy | Inready))
        if(c & Inready)  inb(Data);
 
    /* get current controller command byte */
    outb(Cmd, 0x20);
    if(inready() < 0){
        printf("kbdinit: can't read ccc\n");
	ccc = 0;
    } else
        ccc = inb(Data);
 
    /* enable kbd xfers and interrupts */
    /* disable mouse */
    ccc &= ~Ckbddis;
    ccc |= Csf | Ckbdint | Cscs1;
    if(outready() < 0)
        printf("kbd init failed\n");
    outb(Cmd, 0x60);
    if(outready() < 0)
        printf("kbd init failed\n");
    outb(Data, ccc);
    outready();
}


//-----------------------------------------------------------------
struct irqaction {
    int    irq;
    void   (*func)(int irq, void* arg);
    void   *arg;
    char   name[28];
} irq_handler[16];

void register_irq_handler(int irq, void (*func)(int, void*),
			  void* arg, char *name)
{
    L4_ThreadId_t tid = L4_nilthread;
    int   i, rc;
    // printf(">%s(%d, %x, %x, %s)\n", __FUNCTION__, irq, func, arg, name);

    if (irq_handler[irq].func)
        return ;
    irq_handler[irq].func = func;
    irq_handler[irq].irq = irq;
    irq_handler[irq].arg = arg;
    for(i = 0; i<28; i++)
        if ((irq_handler[irq].name[i] = name[i]) == 0) 
	  break;
    irq_handler[irq].name[i] = 0;

    tid.global.X.thread_no = irq;
    tid.global.X.version = 1;

    rc = L4_AssociateInterrupt(tid, irq_tid);
    if (rc != 1) {
        printf("IntrAssocErr<%d> \n", L4_ErrorCode());
        L4_KDB_Enter("irqAssoc");
    }
}

//-------------------------------------------
static void irq_thread(  )
{
    L4_Word_t tid_user_base = L4_ThreadIdUserBase(L4_GetKernelInterface());
    L4_ThreadId_t tid = L4_nilthread;
    L4_ThreadId_t ack_tid = L4_nilthread;
    L4_MsgTag_t tag;
 
    printf("irq_thread<%X> \n", L4_Myself());
 
 Retry:
    tag = L4_Wait( &tid );
 
    for (;;)
    {
	if( L4_IpcFailed(tag) ) {
	  L4_KDB_Enter("IRQ IPC ");
	  goto Retry;
	}
  
	// Received message.
	if(tid.global.X.thread_no < tid_user_base){ //Hardware IRQ.
	    L4_Word_t irq = tid.global.X.thread_no;
 
	    if (irq_handler[irq].func) 
	        irq_handler[irq].func(irq, irq_handler[irq].arg); 

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
 

void kbd_irq_handler(int irq, void* x)
{
    L4_MsgTag_t  tag;
    unsigned char scancode, ch;
    unsigned char status = inb(KBD_STATUS_REG);  
	// printf("%d ", status);
    
    if (!(status & Inready))
      return ;

    scancode = inb(KBD_DATA_REG);

    printf("\t0x%x ", scancode);
    if (scancode & 0x80){
        printf("\n");
        return;
    }

    ch = keyb_layout[scancode & 0x7f];
    //printf("%d ", scancode);

    L4_LoadMR(0, ch <<16 | 0);
    tag = L4_Send(ping_tid);
    if (L4_IpcFailed(tag))
      printf("err:kbd->ping \n");
}


static void enable_kbd( )
{
    register_irq_handler(1, kbd_irq_handler, 1, "kbd");
}
#endif //%============================================


L4_Fpage_t kip_area, utcb_area;
L4_Word_t utcb_size;

#define UTCB(x) ((void*)(L4_Address(utcb_area) + (x) * utcb_size))
#define NOUTCB	((void*)-1)

static L4_Word_t page_bits;

void ping_thread (void);
void pong_thread (void);

////////////////////////////////////////////////////////////////
#include <l4/arch.h>


#define L4_REQUEST_MASK	 ( ~((~0UL) >> ((sizeof (L4_Word_t) * 8) - 20)))

#define UTCB_ADDRESS	(0x00800000UL)
#define KIP_ADDRESS	(0x00C00000UL)

#define L4_IO_PAGEFAULT		(-8UL << 20)
#define L4_IO_PORT_START	(0)
#define L4_IO_PORT_END		(1<<16)

extern L4_Word_t __L4_syscalls_start;
extern L4_Word_t __L4_syscalls_end;
extern void __L4_copy_syscalls_in (L4_Word_t dest);

char syscall_stubs[4096] __attribute__ ((aligned (4096)));


L4_Fpage_t handle_arch_pagefault 
(L4_MsgTag_t tag, L4_Word_t faddr, L4_Word_t fip, L4_Word_t log2size)
{
#if 1 //-----------------------------------------
    if (faddr >= (L4_Word_t) &__L4_syscalls_start &&
	faddr <  (L4_Word_t) &__L4_syscalls_end)
    {
	__L4_copy_syscalls_in ((L4_Word_t) syscall_stubs);
	return L4_FpageLog2 ( (L4_Word_t) syscall_stubs, log2size);
    }

    // If pagefault is an IO-Page, return that IO Fpage 
    L4_Fpage_t fp;
    fp.raw = faddr;
    if ((tag.raw & L4_REQUEST_MASK) == L4_IO_PAGEFAULT && L4_IsIoFpage(fp)){
	return fp;
    }
#endif //-------------------------------------------	
    return L4_FpageLog2 (faddr, log2size);
}

///////////////////////////////////////////////

void pong_thread (void)
{
    L4_MsgTag_t  tag;
    int          label;
    printf("Pong-thread<%X> \n", L4_Myself().raw);
    L4_ThreadId_t src; //%

    for (;;){
        tag = L4_Wait(&src);
        label = L4_Label(tag);
	printf("-> '%c'  ", label);
    }
}


void ping_thread (void)
{
    L4_ThreadId_t src;
    L4_MsgTag_t   tag;
    int  i, label;

    printf("Ping-thread<%X> \n", L4_Myself().raw);

    for (;;) {
        tag = L4_Wait(&src);
	label = L4_Label(tag);
	//	if (label == '\\') break;

	L4_LoadMR(0, label<<16 | 0);
	L4_Send(pong_tid);
    }


    // Tell master that we're finished
    L4_Set_MsgTag (L4_Niltag);
    L4_Send (master_tid);

    for (;;) L4_Sleep (L4_Never);

    /* NOTREACHED */
}

static void send_startup_ipc (L4_ThreadId_t tid, L4_Word_t ip, L4_Word_t sp)
{
    L4_Msg_t msg;
    L4_MsgTag_t tag;
    L4_MsgClear (&msg);
    L4_MsgAppendWord (&msg, ip); 
    L4_MsgAppendWord (&msg, sp); 
    L4_MsgLoad (&msg);  //%
    tag = L4_Send (tid);
    //  printf ("sent startup message to %lX, (ip=%lx, sp=%lx)%d\n",
    //     (long) tid.raw, (long) ip, (long) sp, L4_IpcFailed(tag));
}


void pager (void)
{
    L4_ThreadId_t tid;
    L4_MsgTag_t tag;
    L4_Msg_t msg;
    printf("Pager_thread<%x> \n", L4_Myself());
    
    for (;;) {
	tag = L4_Wait (&tid);

	for (;;) {
	    L4_MsgStore (tag, &msg);  //%

	    if (L4_GlobalIdOf (tid).raw == master_tid.raw)  //% 
	    {
		//start ping and pong threads
	        send_startup_ipc (irq_tid, (unsigned)(irq_thread), 
				  (L4_Word_t)&irq_stack[1000]);
	        send_startup_ipc (pong_tid, (unsigned)(pong_thread), 
				  (L4_Word_t)&pong_stack[1000]);
	        send_startup_ipc (ping_tid, (unsigned)(ping_thread), 
				  (L4_Word_t)&ping_stack[1000]); 
		break;
	    }

	    if (L4_UntypedWords (tag) != 2 || L4_TypedWords (tag) != 0 ||
		!L4_IpcSucceeded (tag))
	    {
		printf ("pingpong: malformed pagefault IPC from %p (tag=%p)\n",
			(void *) tid.raw, (void *) tag.raw);
		L4_KDB_Enter ("malformed pf");
		break;
	    }

	    L4_Word_t faddr = L4_MsgWord (&msg, 0);  //% Get()
	    L4_Word_t fip   = L4_MsgWord (&msg, 1);  //% Get

	    L4_Fpage_t fpage = handle_arch_pagefault (tag, faddr, fip, page_bits);
	    L4_Set_Rights(&fpage, L4_FullyAccessible);  //%

	    L4_MsgClear (&msg);
	    L4_MsgAppendMapItem (&msg, L4_MapItem (fpage, faddr)); //%
	    L4_MsgLoad (&msg);  //%
	    tag = L4_ReplyWait (tid, &tid);
	}
    }
}


int main (void)
{
    L4_Word_t control;
    L4_Msg_t msg;
    L4_MsgTag_t tag;
    int rc;

    DIRECTPRINT; //%

    L4_KernelInterfacePage_t * kip =
         (L4_KernelInterfacePage_t *) L4_GetKernelInterface (); //%

    for (page_bits = 0;  
         !((1 << page_bits) & L4_PageSizeMask(kip)); 
         page_bits++);

    utcb_size = L4_UtcbSize (kip);

    kip_area = L4_FpageLog2 (KIP_ADDRESS, L4_KipAreaSizeLog2 (kip));

    utcb_area = L4_FpageLog2 ((L4_Word_t) UTCB_ADDRESS,
			      L4_UtcbAreaSizeLog2 (kip) + 2);  //% 2<-1
    printf ("kip_area=%lx, utcb_area=%lx, utcb_size=%lx\n", 
	    kip_area.raw, utcb_area.raw, utcb_size);

    // Touch the memory to make sure we never get pagefaults
    extern L4_Word_t _end, _start;
    L4_Word_t * x; //%
    for (x = (&_start); x < &_end; x += 1024)
    {
	volatile L4_Word_t q;
	q = *(volatile L4_Word_t*) x;
    }
    
    master_tid = L4_Myself ();
    pager_tid = L4_GlobalId (L4_ThreadNo (master_tid) + 1, 2);
    ping_tid  = L4_GlobalId (L4_ThreadNo (master_tid) + 2, 2);
    pong_tid  = L4_GlobalId (L4_ThreadNo (master_tid) + 3, 2);
    irq_tid   = L4_GlobalId (L4_ThreadNo (master_tid) + 4, 2); 

    //---- create pager ----
    L4_Word_t pager_utcb = L4_MyLocalId().raw;
    pager_utcb = (pager_utcb & ~(utcb_size - 1)) + utcb_size;

    printf("localId=%lx, pager-UTCB=%lx\n", L4_MyLocalId().raw, pager_utcb);

    L4_ThreadControl (pager_tid, master_tid, master_tid, master_tid, 
		      (void*)pager_utcb);
    L4_Start_SpIp (pager_tid, (L4_Word_t) &pager_stack[1000], (unsigned)(pager));

    // Intra-as -- put both into the same space
    rc = L4_ThreadControl(ping_tid, ping_tid, master_tid, L4_nilthread, UTCB(0));
    rc = L4_SpaceControl(ping_tid, 0, kip_area, utcb_area, L4_nilthread, &control);
    rc = L4_ThreadControl(ping_tid, ping_tid, master_tid, pager_tid, NOUTCB);
    rc = L4_ThreadControl(pong_tid, ping_tid, master_tid, pager_tid, UTCB(1));
    rc = L4_ThreadControl(irq_tid, ping_tid, master_tid, pager_tid,  UTCB(2)); 

    L4_Sleep(L4_TimePeriod(100*MILISEC));

    // Send message to notify pager to startup both threads
    L4_MsgClear (&msg);
    L4_MsgAppendWord (&msg, (unsigned)ping_thread);  
    L4_MsgAppendWord (&msg, (unsigned)pong_thread);  
    L4_MsgLoad (&msg);  
    
    tag = L4_Send (pager_tid);

    L4_Sleep(L4_TimePeriod(1000*MILISEC));
    enable_kbd();
    kbdinit();

    L4_Receive (ping_tid);

    // Kill both threads
    L4_ThreadControl (ping_tid, L4_nilthread, L4_nilthread, 
		      L4_nilthread, NOUTCB);
    L4_ThreadControl (pong_tid, L4_nilthread, L4_nilthread, 
		      L4_nilthread, NOUTCB);

    for (;;)
	L4_KDB_Enter ("EOW");
}
