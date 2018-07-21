#include  <l4all.h>

#define  LOCAL_BITS    10
#define  GET_PROCNR(tid)   ((tid.raw) >> (LOCAL_BITS + 14))

int l4printf_g(const char *fmt, ...);

//------------------------------------------------------
void post_nextproc(int t)
{
  L4_ThreadId_t   next_proc;
  L4_MsgTag_t     tag;
  L4_Time_t       timeout;
  int             procnr;
  int             errcode; 

  if (t == 0) timeout = L4_Never;
  else  timeout = L4_TimePeriod(t * 1000000);

  procnr =  GET_PROCNR(L4_Myself()) + 1;
  next_proc.raw =  (procnr << (LOCAL_BITS + 14)) + 2;

  L4_LoadMR(0, 0);
  tag = L4_Send_Timeout(next_proc, timeout);
  if (L4_IpcFailed(tag)) {
    errcode = L4_ErrorCode();
    l4printf_g("post_nextproc():%d\n", errcode);
  }
}

//--------------------------------------------------------
void wait_preproc(int t)
{
  L4_ThreadId_t   pre_proc;
  L4_Time_t       timeout;
  L4_MsgTag_t     tag;
  int             errcode;

  if (t == 0) timeout = L4_Never;
  else  timeout = L4_TimePeriod(t * 1000000);

  tag = L4_Wait_Timeout(timeout, &pre_proc);
  if (L4_IpcFailed(tag)){
    errcode = L4_ErrorCode();
    l4printf_g("wait_preproc():%d\n", errcode);
  }
}


