/*********************************************************************
 *                
 * Copyright (C) 2002, 2004-2006,  Karlsruhe University
 *                
 * File path:     bench/pingpong/pingpong.cc
 * Description:   Pingpong test application
 *                
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *                
 * $Id: pingpong.cc,v 1.34.2.14 2006/11/18 14:44:47 stoess Exp $
 *                
 ********************************************************************/

// #include <config.h>   //%
#include <l4/kip.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kdebug.h>
#include <l4/l4io.h>

#define  printf l4printf
#define  DIRECTPRINT     set_l4print_direct(1)   //%%%

#define BUSY_WAITING  1

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

#define kbd_read_input() inb(KBD_DATA_REG)
#define kbd_read_status() inb(KBD_STATUS_REG)


static unsigned char keyb_layout[128] =
  "\000\0331234567890-+\177\t"                    /* 0x00 - 0x0f */
  "qwertyuiop[]\r\000as"                          /* 0x10 - 0x1f */
  "dfghjkl;'`\000\\zxcv"                          /* 0x20 - 0x2f */
  "bnm,./\000*\000 \000\201\202\203\204\205"      /* 0x30 - 0x3f */
  "\206\207\210\211\212\000\000789-456+1"         /* 0x40 - 0x4f */
  "230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
  "\r\000/";                                      /* 0x60 - 0x6f */


int getc()
{
  static unsigned char last_key = 0;
  char c;
  while(1) {
    unsigned char status = kbd_read_status();  
    // printf("%d ", status);
    while (status & KBD_STAT_OBF) {
      unsigned char scancode;
      scancode = kbd_read_input();
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
  }
}


#define  bool   int       //%
#define  true   1         //%
#define  false  0         //%

#define START_ADDR(func)	((L4_Word_t) func)

#define SMALLSPACE_SIZE		16

int Migrate = 0;
int Inter_AS = 1;
int Small_AS = 0;
int Lipc = 0;


L4_ThreadId_t master_tid, pager_tid, ping_tid, pong_tid, irq_tid;

L4_Word_t ping_stack[2048] __attribute__ ((aligned (16)));
L4_Word_t pong_stack[2048] __attribute__ ((aligned (16)));
L4_Word_t pager_stack[2048] __attribute__ ((aligned (16)));
L4_Word_t irq_stack[2048] __attribute__ ((aligned (16)));  //%

L4_Fpage_t kip_area, utcb_area;
L4_Word_t utcb_size;

#define UTCB(x) ((void*)(L4_Address(utcb_area) + (x) * utcb_size))
#define NOUTCB	((void*)-1)

static L4_Word_t page_bits;

void ping_thread (void);
void pong_thread (void);


////////////////////////////////////////////////////////////////
#include <l4/arch.h>

#define HAVE_HANDLE_ARCH_PAGEFAULT

#define L4_REQUEST_MASK		( ~((~0UL) >> ((sizeof (L4_Word_t) * 8) - 20)))

#define UTCB_ADDRESS	(0x00800000UL)
#define KIP_ADDRESS	(0x00C00000UL)

#define L4_IO_PAGEFAULT		(-8UL << 20)
#define L4_IO_PORT_START	(0)
#define L4_IO_PORT_END		(1<<16)

extern L4_Word_t __L4_syscalls_start;
extern L4_Word_t __L4_syscalls_end;
extern void __L4_copy_syscalls_in (L4_Word_t dest);

char syscall_stubs[4096] __attribute__ ((aligned (4096)));


L4_Fpage_t handle_arch_pagefault (L4_MsgTag_t tag, L4_Word_t faddr, L4_Word_t fip, L4_Word_t log2size)
{
#if defined(HAVE_HANDLE_ARCH_PAGEFAULT) //--------------------------------
    // If pagefault is in the syscall stubs, create a copy of the
    // original stubs and map in this copy.
    if (faddr >= (L4_Word_t) &__L4_syscalls_start &&
	faddr <  (L4_Word_t) &__L4_syscalls_end)
    {
	__L4_copy_syscalls_in ((L4_Word_t) syscall_stubs);
	return L4_FpageLog2 ( (L4_Word_t) syscall_stubs, log2size);
    }

    // If pagefault is an IO-Page, return that IO Fpage 
    L4_Fpage_t fp;
    fp.raw = faddr;
    if ((tag.raw & L4_REQUEST_MASK) == L4_IO_PAGEFAULT && L4_IsIoFpage(fp))
    {
	return fp;
    }
#endif //-------------------------------------------	
    return L4_FpageLog2 (faddr, log2size);
}


L4_INLINE L4_Word_t read_cycles (void)
{
    L4_Word_t ret;
    asm volatile ("rdtsc" :"=a" (ret) : :"edx");
    return ret;
}
///////////////////////////////////////////////


static inline void rdpmc (int no, L4_Word64_t* res)
{ 
    L4_Word32_t __eax, __edx, dummy;

    __asm__ __volatile__ (
	"rdpmc	\n\t"
	: "=a"(__eax), "=d"(__edx), "=c"(dummy)
	: "c"(no)
	: "memory");
    
    *res = ( (((L4_Word64_t) __edx) << 32) | ( (L4_Word64_t) __eax));	     
}


void pong_thread (void)
{
    L4_MsgTag_t  tag;
    printf("Pong-thread<%X> starting ...\n", L4_Myself().raw);

    if (Lipc) {
        L4_ThreadId_t ping_ltid; //%
        L4_ThreadId_t src; //%
	ping_ltid = L4_LocalIdOf (ping_tid);
	tag = L4_Wait(&src);
	for (;;) {
	    L4_LoadMR(0, 0);
            tag = L4_LreplyWait(ping_ltid,  &src);
            if (L4_IpcFailed(tag))
               printf("pong ipc failed\n");
	}
    }
    else {
        L4_ThreadId_t src; //%
	tag = L4_Wait(&src);
         for (;;){
            L4_LoadMR(0, 0);
            tag = L4_ReplyWait(ping_tid,  &src);
            if (L4_IpcFailed(tag))
	      printf("ping ipc failed\n");
	}
    }
}


#define ROUNDS		(1000)

void ping_thread (void)
{
    L4_Word_t cycles1, cycles2;
    L4_Clock_t usec1, usec2;
    L4_Word_t instrs1, instrs2;
    L4_Word64_t cnt0,cnt1;
    bool go = true;
    L4_ThreadId_t src;

    printf("Ping-thread<%X> starting ...\n", L4_Myself().raw);

    L4_ThreadId_t pong_ltid = L4_LocalIdOf (pong_tid);

    while (go) {
        int  j;
	printf("Benchmarking %s IPC...\n",
	       Migrate  ? "XCPU" :
	       Inter_AS ? "Inter-AS" : "Intra-AS");

	for (j = 0; j < 16; j++) {
	    L4_Word_t i = ROUNDS;

	    //%	    rdpmc (0,&cnt0);

	    cycles1 = read_cycles ();
	    usec1 = L4_SystemClock ();

	    if (Lipc) {
	        pong_ltid = L4_LocalIdOf (pong_tid);
		for (i=0; i<ROUNDS; i++) {   
		    L4_MsgTag_t  tag;
		    L4_LoadMR(0, 0);
		    tag = L4_Lcall(pong_ltid);
		    if (L4_IpcFailed(tag))
		        printf(" ping ipc failed\n");
		}
	    }
	    else {
                for (i=0; i<ROUNDS; i++) {
                    L4_MsgTag_t  tag;
                    L4_LoadMR(0, 0);
                    tag = L4_Call(pong_tid);
                    if (L4_IpcFailed(tag))
		      printf(" ping ipc failed\n");
		}
	    }
	    
	    cycles2 = read_cycles ();
	    usec2 = L4_SystemClock ();

	    //%	    rdpmc (0,&cnt1);
	    //%	    printf ("rdpmc(0) = %ld\n", cnt0);
	    //%	    printf ("rdpmc(1) = %ld\n", cnt1);
	    //%	    printf ("events: %ld.%02ld\n",
	    //%		    (((long) (cnt1-cnt0) )/(ROUNDS*2)),
	    //%		    (((long) (cnt1-cnt0)*100)/(ROUNDS*2))%100);

	    printf ("IPC (%2d MRs): %ld.%02ld cycles, %ld.%02ldus \n", j,
                    ((long) (cycles2-cycles1))/(ROUNDS*2),
                    (((long) (cycles2-cycles1))*100/(ROUNDS*2))%100,
                    ((long) (usec2.raw-usec1.raw))/(ROUNDS*2),       //%
                    (((long) (usec2.raw-usec1.raw))*100/(ROUNDS*2))%100 );  //%
	}

	bool nomenu = false;
	for (;;) {
	    if (! nomenu)
		printf ("\nWhat now?\n"
			"    g - Continue\n"
			"    q - Quit/New measurement\n"
			"  ESC - Enter KDB\n");
	    nomenu = true;
	    char c = getc ();
	    if ( c == 'g' ) { break; }
	    if ( c == 'q' ) { go = 0; break; }
	    if ( c == '\e' ) { L4_KDB_Enter( "enter kdb" ); nomenu = false; }
	}
    }

    // Tell master that we're finished
    L4_Set_MsgTag (L4_Niltag);
    L4_Send (master_tid);

    for (;;)
	L4_Sleep (L4_Never);

    /* NOTREACHED */
}

static void send_startup_ipc (L4_ThreadId_t tid, L4_Word_t ip, L4_Word_t sp)
{
    L4_Msg_t msg;
    L4_MsgTag_t tag;
    L4_MsgClear (&msg);
    L4_MsgAppendWord (&msg, ip); //%
    L4_MsgAppendWord (&msg, sp); //%
    L4_MsgLoad (&msg);  //%
    tag = L4_Send (tid);
    //    printf ("sent startup message to %lX, (ip=%lx, sp=%lx)%d\n",
    //	    (long) tid.raw, (long) ip, (long) sp, L4_IpcFailed(tag));
}


void pager (void)
{
    L4_ThreadId_t tid;
    L4_MsgTag_t tag;
    L4_Msg_t msg;
    printf("Pager<%x> \n", L4_Myself());
    
    for (;;) {
	tag = L4_Wait (&tid);

	for (;;) {
	    L4_MsgStore (tag, &msg);  //%

	    if (L4_GlobalIdOf (tid).raw == master_tid.raw)  //% 
	    {
		// Startup notification, start ping and pong thread
	        send_startup_ipc (pong_tid, START_ADDR(pong_thread), (L4_Word_t)&pong_stack[2000]);
	        send_startup_ipc (ping_tid, START_ADDR(ping_thread), (L4_Word_t)&ping_stack[2000]); 
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
    int rc;

    DIRECTPRINT; //%

    L4_KernelInterfacePage_t * kip =
         (L4_KernelInterfacePage_t *) L4_GetKernelInterface (); //%

    /* Find smallest supported page size. There's better at least one
     * bit set. */
    for (page_bits = 0;  
         !((1 << page_bits) & L4_PageSizeMask(kip)); 
         page_bits++);

    // Size for one UTCB
    utcb_size = L4_UtcbSize (kip);

    // Put kip in different location (e.g., to allow for small spaces).
    kip_area = L4_FpageLog2 (KIP_ADDRESS, L4_KipAreaSizeLog2 (kip));

    // We need a maximum of 3<-two threads per task
    utcb_area = L4_FpageLog2 ((L4_Word_t) UTCB_ADDRESS,
			      L4_UtcbAreaSizeLog2 (kip) + 2);  //% 2<-1
    printf ("kip_area = %lx, utcb_area = %lx, utcb_size = %lx\n", 
	    kip_area.raw, utcb_area.raw, utcb_size);

    // Touch the memory to make sure we never get pagefaults
    extern L4_Word_t _end, _start;
    L4_Word_t * x; //%
    for (x = (&_start); x < &_end; x += 1024)
    {
	volatile L4_Word_t q;
	q = *(volatile L4_Word_t*) x;
    }
    
    // Create pager
    master_tid = L4_Myself ();
    pager_tid = L4_GlobalId (L4_ThreadNo (master_tid) + 1, 2);
    ping_tid = L4_GlobalId (L4_ThreadNo (master_tid) + 2, 2);
    pong_tid = L4_GlobalId (L4_ThreadNo (master_tid) + 3, 2);
    irq_tid  = L4_GlobalId (L4_ThreadNo (master_tid) + 4, 2);  //%%

    // VU: calculate UTCB address -- this has to be revised
    L4_Word_t pager_utcb = L4_MyLocalId().raw;
    pager_utcb = (pager_utcb & ~(utcb_size - 1)) + utcb_size;
    printf("local id = %lx, pager UTCB = %lx\n", L4_MyLocalId().raw,
	   pager_utcb);

    L4_ThreadControl (pager_tid, master_tid, master_tid, 
		      master_tid, (void*)pager_utcb);
    L4_Start_SpIp (pager_tid, (L4_Word_t) &pager_stack[2040],
	      START_ADDR (pager));


    for (;;) {
	bool printmenu = true;
	for (;;) {
	    if (printmenu) {
		printf ("\nPlease select ipc type:\n");
		printf ("\r\n"
			"1: INTER-AS\r\n"
			"2: INTRA-AS (IPC)\r\n"
			"3: INTRA-AS (Lipc)\r\n"
			"4: XCPU\r\n"
			"5: INTER-AS (small)\r\n"
		    );
	    }
	    printmenu = false;

	    char c = getc ();
	    if (c == '1') { Inter_AS=1; Migrate=0; Small_AS=0; Lipc=0; break; }
	    if (c == '2') { Inter_AS=0; Migrate=0; Small_AS=0; Lipc=0; break; }
	    if (c == '3') { Inter_AS=0; Migrate=0; Small_AS=0; Lipc=1; break; }
	    if (c == '4') { Inter_AS=0; Migrate=1; Small_AS=0; Lipc=0; break; }
	    if (c == '5') { Inter_AS=1; Migrate=0; Small_AS=1; Lipc=0; break; }
	    if (c == '\e') { L4_KDB_Enter ("enter kdb"); printmenu = true;}
	}

	if (Inter_AS) {
	    rc = L4_ThreadControl (ping_tid, ping_tid, master_tid,
			      L4_nilthread, UTCB(0));
	    rc = L4_ThreadControl (pong_tid, pong_tid, master_tid, 
			      L4_nilthread, UTCB(1));
	    rc = L4_SpaceControl (ping_tid, 0, kip_area, utcb_area, L4_nilthread,
			     &control);
	    rc = L4_SpaceControl (pong_tid, 0, kip_area, utcb_area, L4_nilthread,
			     &control);
	    rc = L4_ThreadControl (ping_tid, ping_tid, master_tid, pager_tid, 
			      NOUTCB);
	    rc = L4_ThreadControl (pong_tid, pong_tid, master_tid, pager_tid, 
			      NOUTCB);

	    if (Small_AS) {
		rc = L4_SpaceControl (ping_tid, (1UL << 31) |
				 L4_SmallSpace (0, SMALLSPACE_SIZE), 
				 L4_Nilpage, L4_Nilpage, L4_nilthread,
				 &control);
		rc = L4_SpaceControl (pong_tid, (1UL << 31) | 
				 L4_SmallSpace (SMALLSPACE_SIZE, SMALLSPACE_SIZE), 
				 L4_Nilpage, L4_Nilpage, L4_nilthread,
				 &control);
	    }
	}
	else {
	    // Intra-as -- put both into the same space
	    rc = L4_ThreadControl (ping_tid, ping_tid, master_tid, L4_nilthread, 
			      UTCB(0));
	    rc = L4_SpaceControl (ping_tid, 0, kip_area, utcb_area, L4_nilthread,
			     &control);
	    rc = L4_ThreadControl (ping_tid, ping_tid, master_tid, pager_tid, 
			      NOUTCB);
	    L4_ThreadControl (pong_tid, ping_tid, master_tid, pager_tid, 
			      UTCB(1));
	}

	if (Migrate)
	    L4_Set_ProcessorNo (pong_tid, (L4_ProcessorNo() + 1) % 2);

	// Send message to notify pager to startup both threads
	L4_MsgClear (&msg);
	L4_MsgAppendWord (&msg, START_ADDR (ping_thread));  //%
	L4_MsgAppendWord (&msg, START_ADDR (pong_thread));  //%
	L4_MsgLoad (&msg);  //%
	L4_Send (pager_tid);

	L4_Receive (ping_tid);

	// Kill both threads
	L4_ThreadControl (ping_tid, L4_nilthread, L4_nilthread, 
			  L4_nilthread, NOUTCB);
	L4_ThreadControl (pong_tid, L4_nilthread, L4_nilthread, 
			  L4_nilthread, NOUTCB);
	printf("#8  ");
    }

    for (;;)
	L4_KDB_Enter ("EOW");
}
