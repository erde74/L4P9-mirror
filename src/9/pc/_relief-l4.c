//%%%%%%%%%%%%%   
/******************************************
 *      _relief-l4.c                      *
 *   Low level functions to run on L4     *
 ******************************************/
  
#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

#include  "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../port/error.h"
// #include "../hvm/mm.h"

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
/* TBD is defined as:  tobedefined(__FUNCTION__) */
int  printTBD = 1;  //For debugging  


#define snprint  l4snprintf

extern void p9_register_irq_handler(int irq, void (*func)(Ureg*, void*),
				    void* arg, char *name);
extern void p9_unregister_irq_handler(int irq);
extern L4_ThreadId_t p9_irq_init();

//-----------------------------
#define ASIZE  512

void tobedefined(char *name)
{
  static int  nextx = 0;
  static char fname[ASIZE];
  int  i;
  
  if (printTBD == 0)  return;

  if (strcmp(name, fname) == 0) return ;
  for (i=0; i <= nextx; i++) {   
    if (fname[i] == 0){
      if (strcmp(name, &fname[i+1]) == 0) return ;
    }
  }

  if (nextx > ASIZE - 32) {
    l4printf_r("to-be-defined fname table overflow\n");
    return ;
  }

  l4printf_r("! %s(): to be defined\n", name); 
  strcpy(&fname[nextx], name);
  nextx += strlen(name) + 1;
}

#if 0//------------------------------------
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
#endif //---------------------------

void abort( )
{  TBD;  L4_KDB_Enter("abort");   }


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


int request_alloc_for_dma(unsigned long size,
			  unsigned long logical_addr,
			  unsigned long *physical_addr)
{
  L4_Msg_t msgreg;
  L4_MsgTag_t tag;
  L4_Fpage_t fpg;
  L4_ThreadId_t mx_pager;
  mx_pager.raw = MX_PAGER;
  L4_Word_t arg[2];

  L4_MsgClear(&msgreg);
  L4_MsgAppendWord(&msgreg, (L4_Word_t)size);
  L4_MsgAppendWord(&msgreg, (L4_Word_t)logical_addr);
  L4_Set_MsgLabel(&msgreg, ALLOC_DMA);
  L4_MsgLoad(&msgreg);

  fpg = L4_Fpage((L4_Word_t)logical_addr, size);
  L4_Set_Rights(&fpg, L4_FullyAccessible);
  L4_Accept(L4_MapGrantItems(fpg));
  tag = L4_Call(mx_pager);

  if(L4_IpcFailed(tag)){
	L4_KDB_Enter("error on alloc_dma IPC");
  }

  L4_MsgStore(tag, &msgreg);
  L4_StoreMRs(1, 2, arg);

  if(arg[0] == 0){
	/* success */
	*physical_addr = arg[1];
	return 0;
  }else{
	/* fail */
	*physical_addr = 0x00;
	return -1;
  }

  return 0;
}


// Followings 6 functions are referenced in port/cache.c 

KMap*      kmap(Page* p)
{  TBD;  return 0; }

void       kunmap(KMap* p)
{  TBD; }

Page*      lookpage(Image* _x, ulong _y)
{  TBD;  return 0; }

void         putpage(Page* p)
{  TBD; }

Page*         auxpage(void)
{  TBD;  return 0; }

void          cachepage(Page* _x, Image* _y)
{  TBD;  }

                                                                                       
