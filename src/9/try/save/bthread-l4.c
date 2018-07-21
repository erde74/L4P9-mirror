#include  <l4all.h>
#include  <lp49/lp49.h>
#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b

typedef  unsigned int uint;

/* Which type ?
 *    
 *    +-------------------------------------------+--------+-----------+
 *    |                         stack  <--        |  Atcb  |   udata   |        
 *    +-------------------------------------------+--------+-----------+
 *    :                                           :
 *    stkbase                                   stktop
 *
 *    +---------+-----------+-------------------------------------------+
 *    |  Atcb   |   udata   |                         stack     <-      |        
 *    +---------+-----------+-------------------------------------------+
 *    :                     :                                           :
 *                         stkbase                                   stktop
 *
 */


typedef struct atcb Atcb;
struct   Atcb{
    L4_ThreadId_t  myid;
    uint    stkbase;
    uint    stktop;
    char    state;
    char    malloced;
    char    _x;
    char    _y;
    Atcb*  next;
};


// Example 
typedef  struct {
    Atcb   _a;
    //-----------
    int    x;
    int    y;


}  my_work;



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



#define ATHREAD_MEM(name, dtypename, stksize, stkbase)   \
     struct {                          \
         uint       stk[stksize/4];    \
         dtypename  _a;                \
     }  name ## _area;                 \
     dtypename * name = & (name ## _area)._a ;


int stkmargin(uint stkbase)
{
   register void *stkptr   asm("%esp");
   return  (int)stkptr - stkbase;
}

#define STKMARGIN(stkbase)  { register void *stkptr   asm("%esp"); \
                               (int)stkptr - stkbase }
 

//-------------------------------------
static struct {    
    uint   version ;
    uint   utcb_size;
    uint   utcb_base;
    void * kip ;
    L4_ThreadId_t  root_thd;
    uint   th_num_base;
    int    thnum_max;
    L4_ThreadId_t   tid[256];
} 
thd_tbl = { 0, 0, 0, 0, L4_nilthread, 0, 32, 
	    { [0]= L4_anythread,   // root thread of this space
	      [1 ... 255]= L4_nilthread }
};


int init_thread_tbl() // FIXME: no-reentrant --> thread unsafe
{
    if (thd_tbl.root_thd.raw != 0)
        return;
                                                                                        
    thd_tbl.root_thd = L4_Myself();
    thd_tbl.kip = L4_GetKernelInterface();
    thd_tbl.utcb_size = L4_UtcbSize(thd_tbl.kip);
    thd_tbl.utcb_base = L4_MyLocalId().raw & ~(thd_tbl.utcb_size - 1);
    thd_tbl.th_num_base = L4_ThreadNo(thd_tbl.root_thd);
    thd_tbl.version = L4_Version(thd_tbl.root_thd);
    thd_tbl.thnum_max = 256; //%%%
}



// no-reentrant --> thread unsafe
int assign_tid_and_utcb (L4_ThreadId_t *tid, uint *utcb)
{
    int  i;

    if (thd_tbl.kip == 0) 
        init_thread_tbl();

    for (i = 1; i < thd_tbl.thnum_max; i++) 
       if (L4_IsNilThread(thd_tbl.tid[i])) {
	  *tid = thd_tbl.tid[i] = L4_GlobalId (thd_tbl.th_num_base + i, thd_tbl.version);
	  *utcb = thd_tbl.utcb_base + thd_tbl.utcb_size * i;
	  return 1;
       }

    return  -1;
}


void  free_tid(L4_ThreadId_t tid)
{


}


int athread_create_start(L4_ThreadId_t *newtid,  uint *stackbase, int stacksize,
			  void (*func)(void *arg0, void *arg1), void* arg0, void *arg1)
{
    uint  *stk;
    uint  *sp;
    L4_ThreadId_t tid; 
    atcb  *ap;

    if (stackbase) {
        stk = stackbase;
	ap = (atcb*)stk;
	ap->state = 1;
	ap->malloced = 0;
    }
    else {
        stk = (uint*)malloc(stacksize);
	ap = (atcb*)stk;
	ap->state = 1;
	ap->malloced = 1;
    }

    sp = & stk[stacksize/4 - 2];

    *(--sp) = 0; // no meaning
    *(--sp) = (unsigned)arg1;
    *(--sp) = (unsigned)arg0;
    *(--sp) = 0; // Return address area
    
    {  // tid = create_start_thread((unsigned)func, (unsigned)sp);
      L4_ThreadId_t  myself = L4_Myself();
      L4_Word_t  utcb;

      assign_new_tid_and_utcb(& tid, (void*)& utcb);

      { // requestThreadControl(tid, myself, myself, mypager, utcb);
	  L4_MsgTag_t  tag;
	  L4_ThreadId_t  mx_pager;
	  mx_pager.raw = MX_PAGER;

	  L4_LoadMR(1, (L4_Word_t)tid.raw);  //_MR1
	  L4_LoadMR(2, (L4_Word_t)myself.raw); //_MR2
	  L4_LoadMR(3, (L4_Word_t)myself.raw);
	  L4_LoadMR(4, (L4_Word_t)mx_pager.raw);
	  L4_LoadMR(5, (L4_Word_t)utcb);
	  L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	  tag = L4_Call(mx_pager);
	  if (L4_IpcFailed(tag)) return -1;
      }

      L4_Set_UserDefinedHandleOf(tid, (L4_Word_t)stk);

      // l4printf_g(" thread<%x> created. ", newtid);
      L4_Start_SpIp(tid, (L4_Word_t)sp, (L4_Word_t)func);
    }

    *newtid = tid;
    return  tid.raw;
}


// Ex. rc = l4send_nint(tid, LABEL, 3, i, j, k);
int  l4send_intn(L4_ThreadId_t dest, int label, int num, ...)
{
    L4_MsgTag_t  tag;
    int  *args = (int*)&num; // args[0]:num  args[1..]: ...
    
    L4_LoadMRs(1, num, (L4_Word_t*)&args[1]);
    //eq.  for(i=1; i<=num; i++) L4_LoadMR(i, args[i]); 

    L4_LoadMR(0, label<<16 | num);
    tag = L4_Send(dest);
    if (L4_IpcFailed(tag)) 
        return -1;
    return  num;
}


// Ex. rc = l4call_nint(tid, &r, LABEL, 3, i, j, k);
int  l4call_intn(L4_ThreadId_t dest, int *reply, int label, int num, ...)
{
    int  i, u;
    L4_Word_t  x;
    L4_MsgTag_t  tag;
    int  *argp = (int*)&num;
    argp++; 
    for (i=1; i<=num; i++)
        L4_LoadMR(i, *(argp++));

    L4_LoadMR(0, label<<16 | num);
    tag = L4_Call(dest);
    if (L4_IpcFailed(tag)) 
        return -1;

    u = tag.X.u;
    L4_StoreMR(1, &x);

    *reply = x;
    return  tag.X.label;
}


// Ex. rc = l4receivefrom(tid, &lbl, 2, &x, &y);
int  l4receivefrom_intn(L4_ThreadId_t from, int *label, int num, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_Word_t    x;
    int  **args = (int**)&num;

    tag = L4_Receive(from);
    if (L4_IpcFailed(tag)) 
        return  -1;

    u = tag.X.u;
    if (u>num)  u = num;

    for (i=1; i<= u; i++) {
        L4_StoreMR(i, &x);
	*args[i] = x;
    }

    *label = L4_Label(tag);
    return  u;
}

// Ex. rc = l4receive_intn(&tid, &lbl, 2, &x, &y);
int  l4receive_intn(L4_ThreadId_t *from, int *label, int num, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  src;
    L4_Word_t    x;
    int  **args = (int**)&num;  // args[0]: --  args[1..]: addrof ...

    tag = L4_Wait(&src);
    if (L4_IpcFailed(tag)) 
      return  -1;

    u = tag.X.u;

    if (u>num)  u = num;
    for (i=1; i <= u; i++){
        L4_StoreMR(i, &x);	// l4printf_g("R[%d]:%d  ", i, x);
	*args[i] = x;
    }

    *from = src;
    *label = L4_Label(tag);
    return  u;
}


// Ex. rc = l4send_str_intn(tid, LABEL, buf, len, 3, i, j, k);
int  l4send_str_intn(L4_ThreadId_t dest, int label, char *buf, int len, int num, ...)
{
    L4_MsgTag_t  tag;
    int  *args = (int*)&num;
    L4_StringItem_t stritem = L4_StringItem(len, buf);

    L4_LoadMRs(1, num, (L4_Word_t*)&args[1]);
    L4_LoadMRs(num+1, 2, (L4_Word_t*)&stritem);

    L4_LoadMR(0, label<<16 | 2<<6 | num);  //  [label|0|2|num]
    tag = L4_Send(dest);
    if (L4_IpcFailed(tag)){
        PRN("l4send_str_intn<%x>\n", L4_ErrorCode()); 
        return -1;
    }
    return  num;
}


// Ex. rc = l4receive_str_intn(&tid, &lbl, 2, &x, &y);
int  l4receive_str_intn(L4_ThreadId_t *from, int *label, char *buf, int len, int num, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  src;
    L4_Word_t    x;
    int  **args = (int**)&num;
    L4_StringItem_t stritem = L4_StringItem(len, buf);
    L4_LoadBRs(1, 2, (L4_Word_t*)&stritem);
    L4_LoadBR(0, 1);               // BR[0] StringItem 

    tag = L4_Wait(&src);
    if (L4_IpcFailed(tag)) {
        PRN("l4recv_str_intn<%x>\n", L4_ErrorCode()); 
	return  -1;
    }

    u = tag.X.u;
    if (u > num)  u = num;

    for (i=1; i <= u; i++){
        L4_StoreMR(i, &x);
	*args[i] = x;
    }

    *from = src;
    return  u;
}

// Ex. rc = l4receive_str_intn(&tid, &lbl, 2, &x, &y);
int  l4receivefrom_str_intn(L4_ThreadId_t from, int *label, char *buf, int len, int num, ...)
{
    int  i, u;
    L4_MsgTag_t  tag;
    L4_Word_t    x;
    int  **args = (int**)&num;
    L4_StringItem_t stritem = L4_StringItem(len, buf);
    L4_LoadBRs(1, 2, (L4_Word_t*)&stritem);
    L4_LoadBR(0, 1);               // BR[0] StringItem 

    tag = L4_Receive(from);
    if (L4_IpcFailed(tag)) {
        PRN("l4recv_str_intn<%x>\n", L4_ErrorCode()); 
	return  -1;
    }

    u = tag.X.u;
    if (u > num)  u = num;

    for (i=1; i <= u; i++){
        L4_StoreMR(i, &x);
	*args[i] = x;
    }

    return  u;
}



void l4thread_exits(char *exitstr)
{
    L4_ThreadId_t  myself = L4_Myself();
    L4_ThreadId_t  mypager = L4_Pager();
    atcb *ap = (atcb*)L4_UserDefinedHandle();
    uint  utcb;


    //.............
    if (ap->malloced) 
        free(ap);

    fprint(2, "l4thread_exits: %s\n", exitstr);

    { // rc = requestThreadControl(tid, myself, myself, mypager, utcb);
	  L4_MsgTag_t  tag;
	  L4_ThreadId_t  mx_pager;
	  mx_pager.raw = MX_PAGER;

	  L4_LoadMR(1, (L4_Word_t)myself.raw); 
	  L4_LoadMR(2, (L4_Word_t)L4_nilthread.raw); 
	  L4_LoadMR(3, (L4_Word_t)myself.raw);
	  L4_LoadMR(4, (L4_Word_t)mypager.raw);
	  L4_LoadMR(5, (L4_Word_t)utcb);
	  L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	  tag = L4_Call(mx_pager);
	  if (L4_IpcFailed(tag)) return;
    }

    //.......

    L4_Sleep(L4_Never);
}


void l4thread_kill(L4_ThreadId_t tid)
{
    L4_ThreadId_t  myself = L4_Myself();
    L4_ThreadId_t  mypager = L4_Pager();
    atcb *ap = (atcb*)L4_UserDefinedHandleOf(tid);
    uint  utcb;

    L4_Stop(tid);
    //..............

    if (ap->malloced) 
      free(ap);  // stackarea

    { // requestThreadControl(tid, myself, myself, mypager, utcb);
	  L4_MsgTag_t  tag;
	  L4_ThreadId_t  mx_pager;
	  mx_pager.raw = MX_PAGER;

	  L4_LoadMR(1, (L4_Word_t)tid.raw);  
	  L4_LoadMR(2, (L4_Word_t)L4_nilthread.raw); 
	  L4_LoadMR(3, 0);
	  L4_LoadMR(4, 0);
	  L4_LoadMR(5, 0);
	  L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	  tag = L4_Call(mx_pager);
	  if (L4_IpcFailed(tag)) 
	      return;
    }
}


void  l4thread_killall()
{
    int  i;
    L4_ThreadId_t  tid;

    for (i = 1; i<=thd_tbl.thnum_max; i++){
        tid = thd_tbl.tid[i];
	if (! L4_IsNilThread(tid))
	    l4thread_kill(tid);
    }
}


void threadexitsall(char *exitstr)
{
  fprint(2, "threadexitsall: %s\n", exitstr);
  L4_Sleep(L4_Never);
}

#define  yield   L4_Yield

 
