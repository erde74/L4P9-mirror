#include  <l4all.h>
#include  <lp49/lp49.h>
#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b

typedef  unsigned int uint;


typedef struct atcb atcb;
struct atcb{
    char    state;
    char    malloced;
    uint    *stackbase;
    int     stacksize;
    char    _x;
    char    _y;
    atcb*  next; // is not used.
    L4_ThreadId_t myid;
};


static struct {    
    uint   version ;
    uint   utcb_size;
    uint   utcb_base;
    void * kip ;
    L4_ThreadId_t  root_thd;
    L4_ThreadId_t  killer_thd;
    uint   th_num_base;
    int    thnum_max;
    L4_ThreadId_t   tid[256];
} 
thd_tbl = { 0, 0, 0, 0, L4_nilthread, L4_nilthread,
	    0, 32, { [0]= L4_anythread,   // root thread of this space
		     [1 ... 255]= L4_nilthread }
};


// Private functions
void killer(int argc, char **argv)
{
    L4_MsgTag_t  tag;
    L4_ThreadId_t  src;
    int ret;
  
    while(1) {
        tag = L4_Wait(&src);
        if(L4_IpcFailed(tag)) return -1;
	if(!l4thread_kill(src)) return -1;
	PRN("killer: thread %x killed\n",src);
    }

    // Unreachable
    L4_Sleep(L4_Never);
}

int init_threadlib() // FIXME: no-reentrant --> thread unsafe
{
        unsigned int ssize = 1024;
        char stk[ssize];


        // who call me will be master thread.

        thd_tbl.root_thd = L4_Myself();
	thd_tbl.kip = L4_GetKernelInterface();
	thd_tbl.utcb_size = L4_UtcbSize(thd_tbl.kip);
	thd_tbl.utcb_base = L4_MyLocalId().raw & ~(thd_tbl.utcb_size - 1);
	thd_tbl.th_num_base = L4_ThreadNo(thd_tbl.root_thd);
	thd_tbl.version = L4_Version(thd_tbl.root_thd);

	// killer thread
	l4thread_create_start(&(thd_tbl.killer_thd),stk,ssize,
			      killer, 0, 0);
	if (L4_IsNilThread(thd_tbl.killer_thd)) return -1;
	return 0;
}

int fini_threadlib()
{
        l4thread_killall();
        return 0;
}

int assign_new_tid_and_utcb (L4_ThreadId_t *tid, uint *utcb) // FIXME: no-reentrant --> thread unsafe
{
    int  i;

    if (thd_tbl.kip == 0) init_threadlib(); // DANGER!!: implicit recursion

    for (i = 1; i < thd_tbl.thnum_max; i++) 
       if (L4_IsNilThread(thd_tbl.tid[i])) {
	  *tid = thd_tbl.tid[i] = L4_GlobalId (thd_tbl.th_num_base + i, thd_tbl.version);
	  *utcb = thd_tbl.utcb_base + thd_tbl.utcb_size * i;
	  return 1;
       }

    return  -1;
}

int free_tid(L4_ThreadId_t tid) // FIXME: no-reentrant --> thread unsafe
{
    int i;

    for (i = 1; i < thd_tbl.thnum_max; i++) 
        if (!L4_IsNilThread(thd_tbl.tid[i]) &&
	     L4_SameThreads(tid, thd_tbl.tid[i])){
	    thd_tbl.tid[i] = L4_nilthread;
	    return 1;
	}

    return 0;
}


// Public functions
int l4thread_create_start(L4_ThreadId_t *newtid,  uint *stackbase, int stacksize,
			  void (*func)(void *arg0, void *arg1), void* arg0, void *arg1)
// arg0 = argc; arg1 = argv;
{
    uint  *stk;
    uint  *sp;
    L4_ThreadId_t tid; 
    atcb  *ap;

    if (stackbase) {
        stk = stackbase;

	sp = ((&(stk[stacksize/4 - 2])) - ((uint*)sizeof(atcb)));
	ap = (atcb*)sp;
	ap->state = 1;
	ap->stackbase = stk;
	ap->stacksize = stacksize;

	ap->malloced = 0;
    }
    else {
        stk = (uint*)malloc(stacksize);

	sp = ((&(stk[stacksize/4 - 2])) - ((uint*)sizeof(atcb)));
	ap = (atcb*)sp;
	ap->state = 1;
	ap->stackbase = stk;
	ap->stacksize = stacksize;

	ap->malloced = 1;
    }

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

      L4_Set_UserDefinedHandleOf(tid, (L4_Word_t)ap); // place atcb at UserDefineHandleOf

      // l4printf_g(" thread<%x> created. ", newtid);
      L4_Start_SpIp(tid, (L4_Word_t)sp, (L4_Word_t)func);
    }

    *newtid = tid;
    return  tid.raw;
}

// Ex. rc = l4send_nint(tid, LABEL, 3, i, j, k);
int  l4send_nint(L4_ThreadId_t dest, int label, int num, ...)
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
int  l4call_nint(L4_ThreadId_t dest, int *reply, int label, int num, ...)
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
int  l4receivefrom_nint(L4_ThreadId_t from, int *label, int num, ...)
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

// Ex. rc = l4receive_nint(&tid, &lbl, 2, &x, &y);
int  l4receive_nint(L4_ThreadId_t *from, int *label, int num, ...)
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


// Ex. rc = l4send_str_nint(tid, LABEL, buf, len, 3, i, j, k);
int  l4send_str_nint(L4_ThreadId_t dest, int label, char *buf, int len, int num, ...)
{
    L4_MsgTag_t  tag;
    int  *args = (int*)&num;
    L4_StringItem_t stritem = L4_StringItem(len, buf);

    L4_LoadMRs(1, num, (L4_Word_t*)&args[1]);
    L4_LoadMRs(num+1, 2, (L4_Word_t*)&stritem);

    L4_LoadMR(0, label<<16 | 2<<6 | num);  //  [label|0|2|num]
    tag = L4_Send(dest);
    if (L4_IpcFailed(tag)){
        PRN("l4send_str_nint<%x>\n", L4_ErrorCode()); 
        return -1;
    }
    return  num;
}


// Ex. rc = l4receive_str_nint(&tid, &lbl, 2, &x, &y);
int  l4receive_str_nint(L4_ThreadId_t *from, int *label, char *buf, int len, int num, ...)
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
        PRN("l4recv_str_nint<%x>\n", L4_ErrorCode()); 
	return  -1;
    }

    u = tag.X.u;
    if (u > num)  u = num;

    for (i=1; i <= u; i++){
        L4_StoreMR(i, &x);
	*args[i] = x;
    }

    *from = src;
    *label = L4_Label(tag);
    return  u;
}

// Ex. rc = l4receive_str_nint(&tid, &lbl, 2, &x, &y);
int  l4receivefrom_str_nint(L4_ThreadId_t from, int *label, char *buf, int len, int num, ...)
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
        PRN("l4recv_str_nint<%x>\n", L4_ErrorCode()); 
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
  // Phase 1
  // free all resources
  // exept for stack and text segment
  // nothing todo now!!

  { // Phase 2
    // send kill request to master
    // and wait master to kill me!!
    L4_MsgTag_t  tag;

    PRN("exits: now terminating...\n");
    tag = L4_Send(thd_tbl.killer_thd);
    if (L4_IpcFailed(tag)) return;

    // Wait forever
    L4_Sleep(L4_Never);
  }
}


int l4thread_kill(L4_ThreadId_t tid)
{
    L4_ThreadId_t  myself = L4_Myself();
    L4_ThreadId_t  mypager = L4_Pager();
    atcb *ap = (atcb*)L4_UserDefinedHandleOf(tid);
    uint  utcb;

    L4_Stop(tid);
    //..............

    if (ap->malloced) 
      free(ap->stackbase);  // stackarea

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
	  if (L4_IpcFailed(tag)) return 0;
	  if (!free_tid(tid)) return 0;
	  return 1;
    }
}


void  l4thread_killall()
{
    int  i;
    L4_ThreadId_t  tid;

    for (i = 1; i<thd_tbl.thnum_max; i++){
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

 

#ifdef TESTMAIN
#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

#define  l4printf print

void test_thread_loop(int argc, void *argv[])
{
  L4_ThreadId_t  myTid = L4_Myself();
  int   lc;
  int   tasknr, localnr, version;
  char  msg[100]={0};
  register void *stkptr   asm("%esp");
  register void *frameptr asm("%ebp");
  L4_Time_t wait;
  int thread_number = argv[0];
  atcb *ap = (atcb*)L4_UserDefinedHandleOf(myTid);
  char *malloced = "not malloced";
  if (ap->malloced) malloced = "malloced";

  l4printf_r ("%d'th thread start!! [%x-%x] sp=%x fp=%x func=%x\n", 
	      thread_number, myTid, &myTid, stkptr, frameptr, test_thread_loop);

  tasknr = TID2PROCNR(myTid);
  localnr =  TID2LOCALNR(myTid);
  version = L4_Version(myTid);

  wait.period.m = 1;
  wait.period.e = 20; //10*(2^20)
  wait.period.a = 0; // this is period

  for (lc = 0;;lc++){
    if (lc%3 == 0)
      l4printf_r("[%d-%d-%d] %d'th %s thread %d'th awake!!",
		 tasknr, localnr, version,
		 thread_number, malloced, lc);
    else if (lc%3 == 1)
      l4printf_g("[%d-%d-%d] %d'th %s thread %d'th awake!!",
		 tasknr, localnr, version,
		 thread_number, malloced, lc);
    else
      l4printf_b("[%d-%d-%d] %d'th %s thread %d'th awake!!",
		 tasknr, localnr, version,
		 thread_number, malloced, lc);

    L4_Sleep(wait);
  }

  // Never Reach
  //	L4_KDB_Enter ("Hello debugger!");
  L4_Yield();
  L4_Sleep (L4_Never);
}

void test_loop() // passed
{
  L4_ThreadId_t newtid;
  int  stacksize = 1024;
  char stack0[stacksize];
  unsigned int lc = 0;
  int iargc;char *iargv[4];
  L4_Time_t wait;
  wait.period.m = 10;
  wait.period.e = 20; //10*(2^20)
  wait.period.a = 0; // this is period

  for(lc = 0; lc < 10; lc++){
    iargc = 1; iargv[0] = lc;
    l4printf_r("master: %dth thread create start!!", lc);
    if (lc%2 == 0)
    l4printf_r("tid.raw is %d\n",
	       l4thread_create_start(&newtid, stack0, stacksize,
				     test_thread_loop, iargc, iargv));
    else
    l4printf_r("tid.raw is %d\n",
	       l4thread_create_start(&newtid, 0, stacksize,
				     test_thread_loop, iargc, iargv));

    // wait and kill
    L4_Sleep(wait);
    l4thread_kill(newtid);
    l4printf_r("\nmaster: %dth thread was killed?\n", lc);
  }
}

void main(int argc, char *argv[])
{
  test_loop();

  //	L4_KDB_Enter ("Hello debugger!");
  L4_Yield();
  L4_Sleep (L4_Never);
};
#endif // TESTMAIN
