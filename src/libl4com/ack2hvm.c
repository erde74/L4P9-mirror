#include  <l4all.h>
#include  <lp49/lp49.h>

#define  LOCAL_BITS    10
#define  GET_PROCNR(tid)   ((tid.raw) >> (LOCAL_BITS + 14))

int l4printf_g(const char *fmt, ...);

//------------------------------------------------------
void ack2hvm(int t)
{
    L4_ThreadId_t   hvm_proc;
    L4_MsgTag_t     tag;
    L4_Time_t       timeout;
    int             procnr;
    int             errcode; 

    hvm_proc.raw = HVM_PROC; 
    if (t == 0) timeout = L4_Never;
    else  timeout = L4_TimePeriod(t * 1000000);

    L4_LoadMR(0, 0);
    tag = L4_Send_Timeout(hvm_proc, timeout);
    if (L4_IpcFailed(tag)) {
      errcode = L4_ErrorCode();
      l4printf_g(" Err in ack2hvm():%d\n", errcode);
    }
}

//--------------------------------------------------------
void wait_ack(L4_ThreadId_t from, int t)
{
    L4_Time_t       timeout;
    L4_MsgTag_t     tag;
    int             errcode;

    if (t == 0) timeout = L4_Never;
    else  timeout = L4_TimePeriod(t * 1000000);

    tag = L4_Receive_Timeout(from, timeout);
    if (L4_IpcFailed(tag)){
      errcode = L4_ErrorCode();
      l4printf_g("wait_preproc():%d\n", errcode);
    }
}


