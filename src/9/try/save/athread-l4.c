#include  <l4all.h>
#include  <lp49/lp49.h>
#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b

#include  <u.h>
#include  <libc.h>


/* Which type ?
 *
 *    +---------+-----------++----------------------------------------+-+
 *    |  Atcb   |   udata   ||                           stack  <---- | |        
 *    +---------+-----------++----------------------------------------+-+
 *    :                      :                                        :
 *                         stkbase                                   stktop
 *
 *    
 */


typedef struct Atcb Atcb;
struct   Atcb{
    L4_ThreadId_t  tid;
    uint    stkbase;
    uint    stktop;
    char    state;
    char    malloced;
    char    _x;
    char    _y;
};


enum {
  L4thread_none   = 0,
  L4TH_ASIGND  = 1,
  L4thread_killes = 3
}; 


#if 0 //------------------------------------------------------

void*  thread_malloc(int dsize, int stksize, void **  stkbase)
{
    void  *p, *q;

    int  dsize2, stksize2;
    dsize2 = ((dsize + 15)/16) * 16 ;
    stksize2 = ((stksize + 15)/16) * 16;
    totoalsize = dsize2 + stksize2;
    // Shall we alignen it at page boundary ? -- Future task

    p = malloc(totalsize);
    q = (void*)((uint)p + (uint)stksize2);

    *stkbase = p;
    return  q;
}

/*    strcut{ 
 *       int   stk[1000];
 *       Atcb  _a;
 *       int    x;
 *       int    y;
 *       ....
 *     } foo_area = { _a:{ .stackbase= & foo_area.stk[0],
 *                         .stacktop= & foo_area.stk[1000] }p 
 *                  };
 *    Atcb  *foo_atcb - & foo_area._a;
 *
 */


#define L4THREAD_MEM(name, dtypename, stksize, stkbase)   \
     struct {                          \
         uint       stk[stksize/4];    \
         dtypename  _a;                \
     }  name ## _area;                 \
     dtypename * name = & (name ## _area)._a ;

#endif //--------------------------------------------------------- 

int stkmargin(uint stkbase)
{
    register void *stkptr   asm("%esp");
    return  (int)stkptr - stkbase;
}

int  STKMARGIN()  
{ 
    register void *stkptr   asm("%esp");
    Atcb  * atcb = (Atcb*)L4_UserDefinedHandle();
    return   (int)stkptr - atcb->stkbase;
}


static void quietsleep()
{
    print(">>> quietsleep ...\n");
    L4_Sleep(L4_Never);
}

//-------------------------------------
static struct {    
    uint   version ;
    uint   utcb_size;
    uint   utcb_base;
    void  *kip ;
    L4_ThreadId_t  root_tid;
    L4_ThreadId_t  pager_tid;

    uint   th_num_base;
    int    thnum_max;
    int    thcnt;  // total number of threads
    char   used[256];
}  thd_tbl ;


static void init_thread_tbl() // FIXME: no-reentrant --> thread unsafe
{
    int  i;
    if (thd_tbl.kip != 0)   return;
                                                                                        
    thd_tbl.root_tid =  L4_Myself();
    thd_tbl.pager_tid = L4_Pager();
    thd_tbl.kip = L4_GetKernelInterface();
    thd_tbl.utcb_size = L4_UtcbSize(thd_tbl.kip);
    thd_tbl.utcb_base = L4_MyLocalId().raw & ~(thd_tbl.utcb_size - 1);
    thd_tbl.th_num_base = L4_ThreadNo(thd_tbl.root_tid);
    thd_tbl.version = L4_Version(thd_tbl.root_tid);
    thd_tbl.thnum_max = 63; //%%%
    thd_tbl.thcnt = 0;

    thd_tbl.used[0]= 1;
    for (i=1; i<256; i++)
        thd_tbl.used[i] = 0;
}


// no-reentrant --> thread unsafe --> Lock must be applied.
static int assign_tid_and_utcb (L4_ThreadId_t *tid, L4_Word_t  *utcb)
{
    int  i;

    if (thd_tbl.kip == 0)   init_thread_tbl();

    for (i = 0; i < thd_tbl.thnum_max; i++) {
       if (thd_tbl.used[i] == 0) {
          thd_tbl.thcnt ++;
	  thd_tbl.used[i] = 1;
	  *tid =  L4_GlobalId (thd_tbl.th_num_base + i, thd_tbl.version);
	  *utcb = thd_tbl.utcb_base + thd_tbl.utcb_size * i;
	  return 1;
       }
    }
    return  0;
}


#define TID2LOCALNUM(tid)  ((tid.raw>>14)& 0x3FF)

static void  free_tid(L4_ThreadId_t tid)
{
    thd_tbl.used[TID2LOCALNUM(tid)] = 0;
    thd_tbl.thcnt --;
}


Atcb* l4thread_create0(Atcb *ap, uint stkbase, uint *sp, int malloced,
		       void (*func)())
{
    L4_ThreadId_t  tid; 
    L4_Word_t      utcb;

    //print(">%s(%x %d %x %d %x)\n", __FUNCTION__, ap, stkbase, sp, malloced, func);

    *(--sp) = (uint)quietsleep;  // 0; // Return address area
    
    if (assign_tid_and_utcb(&tid, &utcb) != 1)
        return nil;

    ap->tid = tid;
    ap->stkbase = stkbase;
    ap->stktop  = (uint)sp;
    ap->malloced = malloced;
    ap->state = L4TH_ASIGND;

    {  	// requestThreadControl(tid, myself, myself, mypager, utcb);
	L4_MsgTag_t  tag;

	L4_LoadMR(1, (L4_Word_t)tid.raw);  //_MR1
	L4_LoadMR(2, (L4_Word_t)thd_tbl.root_tid.raw); //_MR2
	L4_LoadMR(3, (L4_Word_t)thd_tbl.root_tid.raw);
	L4_LoadMR(4, (L4_Word_t)thd_tbl.pager_tid.raw);
	L4_LoadMR(5, (L4_Word_t)utcb);
	L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	tag = L4_Call(thd_tbl.pager_tid);
	if (L4_IpcFailed(tag))  {
	    free(ap);
	    // free tid
	    return  nil;
	}

	L4_Set_UserDefinedHandleOf(tid, (L4_Word_t)ap);

	// l4printf_g(" thread<%x> created. ", newtid);
	L4_Start_SpIp(tid, (L4_Word_t)sp, (L4_Word_t)func);
    }

    return   ap;
}


Atcb* l4thread_create_udata(int stacksize,  void (*func)(), int udsize)
{
    uint          *sp;
    L4_ThreadId_t  tid;
    Atcb          *ap;
    L4_Word_t      utcb;
    uint  totalsize, stkbase;

    if (udsize == 0)
      udsize = sizeof (Atcb);

    udsize = ((udsize + 15)/16)  * 16 ;
    stacksize = ((stacksize + 15)/16) * 16;
    totalsize = udsize + stacksize;

    ap = (Atcb*)malloc(totalsize);
    if (ap == nil)   return  nil;

    stkbase = (uint)ap + (uint)udsize + 8;
    sp  = (uint*) ((uint)ap + (uint)totalsize - 16);

    return l4thread_create0(ap, stkbase, sp, 1, func);
}


Atcb* l4thread_create(int stacksize,  void (*func)(), int argcnt, ... )
{
    uint          *sp;
    L4_ThreadId_t  tid;
    Atcb          *ap;
    L4_Word_t      utcb;
    uint        totalsize, stkbase, atcbsize;
    int         i;
    int          *arg;

//    print(">%s(%d %x %d)\n", __FUNCTION__, stacksize, func, argcnt);

    atcbsize = ((sizeof (Atcb)  + 15)/16) * 16;
    stacksize = ((stacksize + 15)/16) * 16;
    totalsize = atcbsize + stacksize;

    ap = (Atcb*)malloc(totalsize);
    if (ap == nil)   return  nil;

    stkbase = (uint)ap + (uint)atcbsize + 8;
    sp  = (uint*) ((uint)ap + (uint)totalsize - 16);

    arg = &argcnt + 1 ;  // 
    *(--sp) = 0; // no meaning

    for (i = argcnt-1; i >= 0; i--){
      *(--sp) = arg[i];  
    }

    return l4thread_create0(ap, stkbase, sp, 1, func);
}


#if 0 //----------------------------------------
Atcb* l4thread_create_start(int  udsize, int stacksize,
			  void (*func)(void *arg0, void *arg1), void* arg0, void *arg1)
{
    uint          *sp;
    L4_ThreadId_t  tid; 
    Atcb          *ap;
    L4_Word_t      utcb;
    uint  totalsize, stkbase;

    if (udsize == 0) 
        udsize = sizeof (Atcb);

    udsize = ((udsize + 15)/16)  * 16 ;
    stacksize = ((stacksize + 15)/16) * 16;
    totalsize = udsize + stacksize;

    ap = (Atcb*)malloc(totalsize);
    if (ap == nil)   return  nil;

    stkbase = (uint)ap + (uint)udsize + 8;
    sp  = (uint*) ((uint)ap + (uint)totalsize - 16);

    *(--sp) = 0; // no meaning
    *(--sp) = (unsigned)arg1;
    *(--sp) = (unsigned)arg0;
    *(--sp) = (uint)quietsleep;  // 0; // Return address area
    
    if (assign_tid_and_utcb(&tid, &utcb) != 1)
        return nil;

    ap->tid = tid;
    ap->stkbase = stkbase;
    ap->stktop  = (uint)sp;
    ap->malloced = 1;
    ap->state = L4TH_ASIGND;

    {  	// requestThreadControl(tid, myself, myself, mypager, utcb);
	L4_MsgTag_t  tag;

	L4_LoadMR(1, (L4_Word_t)tid.raw);  //_MR1
	L4_LoadMR(2, (L4_Word_t)thd_tbl.root_tid.raw); //_MR2
	L4_LoadMR(3, (L4_Word_t)thd_tbl.root_tid.raw);
	L4_LoadMR(4, (L4_Word_t)thd_tbl.pager_tid.raw);
	L4_LoadMR(5, (L4_Word_t)utcb);
	L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	tag = L4_Call(thd_tbl.pager_tid);
	if (L4_IpcFailed(tag))  {
	    free(ap);
	    // free tid
	    return  nil;
	}

	L4_Set_UserDefinedHandleOf(tid, (L4_Word_t)ap);

	// l4printf_g(" thread<%x> created. ", newtid);
	L4_Start_SpIp(tid, (L4_Word_t)sp, (L4_Word_t)func);
    }

    return   ap;
}
#endif //----------------------------


int  l4thread_kill(L4_ThreadId_t  tid)
{
    L4_ThreadState_t  state;  
    Atcb  * atcb;

    if (thd_tbl.used[TID2LOCALNUM(tid)])
      {
	    state = L4_AbortIpc_and_stop(tid);
	    if (!L4_ThreadWasHalted(state))
	      print("thread-kill-err:%d\n", state);
    
	    // requestThreadControl(tid, myself, myself, mypager, utcb);
	    L4_MsgTag_t  tag;

	    L4_LoadMR(1, (L4_Word_t)tid.raw);  //_MR1
	    L4_LoadMR(2, (L4_Word_t)L4_nilthread.raw); //_MR2
	    L4_LoadMR(3, (L4_Word_t)L4_nilthread.raw);
	    L4_LoadMR(4, (L4_Word_t)L4_nilthread.raw);
	    L4_LoadMR(5, (L4_Word_t)-1);
	    L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	    tag = L4_Call(thd_tbl.pager_tid);
	    if (L4_IpcFailed(tag))  {
	      //  free(atcb);
	      //  free_tid 
	      return  0;
	    }

	    atcb = (Atcb*)L4_UserDefinedHandleOf(tid);
	    free(atcb);
	    free_tid(tid);
	    return  1;
      }

    return  0;
}


void l4thread_exits(char *exitstr)
{
    uint  utcb;


    //.............
    print("l4thread_exits: \n");

    { // rc = requestThreadControl(tid, myself, myself, mypager, utcb);
	  L4_MsgTag_t  tag;

	  L4_LoadMR(1, (L4_Word_t)thd_tbl.root_tid.raw); 
	  L4_LoadMR(2, (L4_Word_t)L4_nilthread.raw); 
	  L4_LoadMR(3, (L4_Word_t)thd_tbl.root_tid.raw);
	  L4_LoadMR(4, (L4_Word_t)thd_tbl.root_tid.raw);
	  L4_LoadMR(5, (L4_Word_t)utcb);
	  L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	  tag = L4_Call(thd_tbl.pager_tid);
	  if (L4_IpcFailed(tag)) return;
    }

    //.......

    L4_Sleep(L4_Never);
}


void  l4thread_killall()
{
    int  i;
    L4_ThreadId_t  tid;

    for (i = 1; i<=thd_tbl.thnum_max; i++)
      if ( thd_tbl.used[i]) {
	  tid = L4_GlobalId (thd_tbl.th_num_base + i, thd_tbl.version);
	  l4thread_kill(tid);
      }
}


void threadexitsall(char *exitstr)
{
    print("threadexitsall: %s\n", exitstr);
    L4_Sleep(L4_Never);
}

#define  yield   L4_Yield



//-----------------------------------------------------------------

// Ex. rc = l4send_intn(tid, LABEL, 3, i, j, k);
int  l4send_intn(L4_ThreadId_t dest, int label, int argcnt, ...)
{
    L4_MsgTag_t  tag;
    int  *args = (int*)&argcnt; // args[0]:argcnt  args[1..]: ...
    
    L4_LoadMRs(1, argcnt, (L4_Word_t*)&args[1]);
    //eq.  for(i=1; i<=argcnt; i++) L4_LoadMR(i, args[i]); 

    L4_LoadMR(0, label<<16 | argcnt);
    tag = L4_Send(dest);
    if (L4_IpcFailed(tag)) 
        return -1;
    return  argcnt;
}


// Ex. rc = l4call_intn(tid, &r, LABEL, 3, i, j, k);
int  l4call_intn(L4_ThreadId_t dest, int *reply, int label, int argcnt, ...)
{
    int  i, u;
    L4_Word_t  x;
    L4_MsgTag_t  tag;
    int  *argp = (int*)&argcnt;
    argp++; 
    for (i=1; i<=argcnt; i++)
        L4_LoadMR(i, *(argp++));

    L4_LoadMR(0, label<<16 | argcnt);
    tag = L4_Call(dest);
    if (L4_IpcFailed(tag)) 
        return -1;

    u = tag.X.u;
    L4_StoreMR(1, &x);

    *reply = x;
    return  tag.X.label;
}


// Ex. rc = l4receivefrom(tid, &lbl, 2, &x, &y);
int  l4receivefrom_intn(L4_ThreadId_t from, int *label, int argcnt, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_Word_t    x;
    int  **args = (int**)&argcnt;

    tag = L4_Receive(from);
    if (L4_IpcFailed(tag)) 
        return  -1;

    u = tag.X.u;
    if (u>argcnt)  u = argcnt;

    for (i=1; i<= u; i++) {
        L4_StoreMR(i, &x);
	*args[i] = x;
    }

    *label = L4_Label(tag);
    return  u;
}

// Ex. rc = l4receive_intn(&tid, &lbl, 2, &x, &y);
int  l4receive_intn(L4_ThreadId_t *from, int *label, int argcnt, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  src;
    L4_Word_t    x;
    int  **args = (int**)&argcnt;  // args[0]: --  args[1..]: addrof ...

    tag = L4_Wait(&src);
    if (L4_IpcFailed(tag)) 
      return  -1;

    u = tag.X.u;

    if (u>argcnt)  u = argcnt;
    for (i=1; i <= u; i++){
        L4_StoreMR(i, &x);	// l4print_g("R[%d]:%d  ", i, x);
	*args[i] = x;
    }

    *from = src;
    *label = L4_Label(tag);
    return  u;
}


// Ex. rc = l4send_str_intn(tid, LABEL, buf, len, 3, i, j, k);
int  l4send_str_intn(L4_ThreadId_t dest, int label, char *buf, int len, int argcnt, ...)
{
    L4_MsgTag_t  tag;
    int  *args = (int*)&argcnt;
    L4_StringItem_t stritem = L4_StringItem(len, buf);

    L4_LoadMRs(1, argcnt, (L4_Word_t*)&args[1]);
    L4_LoadMRs(argcnt+1, 2, (L4_Word_t*)&stritem);

    L4_LoadMR(0, label<<16 | 2<<6 | argcnt);  //  [label|0|2|argcnt]
    tag = L4_Send(dest);
    if (L4_IpcFailed(tag)){
        print("l4send_str_intn<%x>\n", L4_ErrorCode()); 
        return -1;
    }
    return  argcnt;
}


// Ex. rc = l4receive_str_intn(&tid, &lbl, 2, &x, &y);
int  l4receive_str_intn(L4_ThreadId_t *from, int *label, char *buf, int len, int argcnt, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  src;
    L4_Word_t    x;
    int  **args = (int**)&argcnt;
    L4_StringItem_t stritem = L4_StringItem(len, buf);
    L4_LoadBRs(1, 2, (L4_Word_t*)&stritem);
    L4_LoadBR(0, 1);               // BR[0] StringItem 

    tag = L4_Wait(&src);
    if (L4_IpcFailed(tag)) {
        print("l4recv_str_intn<%x>\n", L4_ErrorCode()); 
	return  -1;
    }

    u = tag.X.u;
    if (u > argcnt)  u = argcnt;

    for (i=1; i <= u; i++){
        L4_StoreMR(i, &x);
	*args[i] = x;
    }

    *from = src;
    *label = L4_Label(tag);
    return  u;
}


// Ex. rc = l4receive_str_intn(&tid, &lbl, 2, &x, &y);
int  l4receivefrom_str_intn(L4_ThreadId_t from, int *label, char *buf, int len, int argcnt, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_Word_t    x;
    int  **args = (int**)&argcnt;
    L4_StringItem_t stritem = L4_StringItem(len, buf);
    L4_LoadBRs(1, 2, (L4_Word_t*)&stritem);
    L4_LoadBR(0, 1);               // BR[0] StringItem 

    tag = L4_Receive(from);
    if (L4_IpcFailed(tag)) {
        print("l4recv_str_intn<%x>\n", L4_ErrorCode()); 
	return  -1;
    }

    u = tag.X.u;
    if (u > argcnt)  u = argcnt;

    for (i=1; i <= u; i++){
        L4_StoreMR(i, &x);
	*args[i] = x;
    }

    *label = L4_Label(tag);
    return  u;
}


/************************************************************
 *
 *      main-thread           alpha                 beta
 *    []---------+         []---------+          []--------+
 *     |         |          |         |           |        |
 *     |       ------------>|         |           |        |
 *     |         |(int,int) |        ------------>|        |
 *     |         |          |         |(str,int)  |        |
 *     |         |          |         |           |        |
 *     +---------+          +---------+           +--------+
 *
 ************************************************************/

#define MILISEC 1000

L4_ThreadId_t  alpha_tid, beta_tid;


void alpha(void *arg1, void *arg2)
{
    int  label, x, y;
    L4_ThreadId_t  src;
    Atcb  *atcb = (Atcb*)L4_UserDefinedHandle();

    print(">>>> alpha(%d %d)  [%x]<%d> <<<< \n", 
	  (int)arg1, (int)arg2, L4_Myself(), stkmargin(atcb->stkbase));
    for(;;){
        l4receive_intn(&src, &label, 2 , &x, &y);
        print("alpha<%d %d %d>  ", label, x, y);
	l4send_str_intn(beta_tid, label, "abcdef", 8, 1, x+y);
    }
}

void beta(void *arg1, void *arg2)
{
    char  buf[64];
    int   label, u, v;
    L4_ThreadId_t  src;
    Atcb  *atcb = (Atcb*)L4_UserDefinedHandle();
    print(">>>> beta(%d %d)  [%x]<%d> <<<< \n", 
	  (int)arg1, (int)arg2, L4_Myself(), STKMARGIN());
    for(;;){
	l4receive_str_intn(&src, &label, buf, 64, 1 , &u);
	print("beta{%d \'%s\' %d}    \n", label, buf, u);
    }
}

void gamma(int x, int y, int z )
{
    print(">>>> gamma(%d %d %d) [%x] <<<< \n", x, y, z, L4_Myself().raw);
    L4_Sleep(L4_TimePeriod(200*MILISEC));
}


void main() 
{
    Atcb    *alphap, *betap;
    int  i;

    print("<><><><><><> Athread [%X] <><><><> \n", L4_Myself());
    alphap = l4thread_create(2000, alpha, 2, 10, 20);
    if (alphap == nil) print("<><><> thread not creates <><><>\n");
    alpha_tid = alphap->tid;

    betap = l4thread_create(1000, beta, 2, (void*)70, (void*)80);
    if (betap == nil) print("<><><> thread not creates <><><>\n");
    beta_tid = betap->tid;

    L4_Sleep(L4_TimePeriod(500*MILISEC));    

    for (i=0; i<10; i++) {
       l4send_intn(alpha_tid, i, 2, i*10, i*20); 
       L4_Sleep(L4_TimePeriod(500*MILISEC));    
    }
    
    l4thread_create(1000, gamma, 3, 111, 222, 333);
    

    L4_Sleep(L4_TimePeriod(2000*MILISEC));
    l4thread_killall();
    print("<><><><><><> Athread By [%X] <><><><> \n", L4_Myself());

    L4_Sleep(L4_Never);
}
