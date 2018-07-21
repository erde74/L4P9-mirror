#include  <l4all.h>

//------------------------------------------------------
int activate_server(L4_ThreadId_t  srvtid, int ms)
{
    L4_Msg_t     msg;
    L4_MsgTag_t  tag;
    L4_Time_t    timeout;
    int          errcode; 

    L4_Accept(L4_UntypedWordsAcceptor);
    if (ms == 0) timeout = L4_Never;
    else  timeout = L4_TimePeriod(ms * 1000000);

    L4_MsgClear(&msg);
    L4_MsgLoad(&msg);
    tag = L4_Call_Timeouts(srvtid, timeout, timeout);
    if (L4_IpcFailed(tag)) {
        errcode = L4_ErrorCode();
	print("! activate_server(%x):%d\n", srvtid.raw, errcode);
	return  0;
    }
    return  1;
}

//--------------------------------------------------------
L4_ThreadId_t wait_activation(int ms)
{
    L4_ThreadId_t   mate;
    L4_Time_t       timeout;
    L4_MsgTag_t     tag;
    int             errcode;

    L4_Accept(L4_UntypedWordsAcceptor);
    if (ms == 0) timeout = L4_Never;
    else  timeout = L4_TimePeriod(ms * 1000000);
    tag = L4_Wait_Timeout(timeout, &mate);
    if (L4_IpcFailed(tag)){
        errcode = L4_ErrorCode();
	print("! wait_activation from %x: %d\n", mate.raw, errcode);
	return L4_nilthread;
    }
    return  mate;
}

//--------------------------------------------------------
int  post_activation(L4_ThreadId_t  tid)
{
    L4_Msg_t     msg;
    L4_MsgTag_t     tag;
    int             errcode;

    L4_MsgClear(&msg);
    L4_MsgLoad(&msg);
    tag = L4_Reply(tid);
    if (L4_IpcFailed(tag)){
        errcode = L4_ErrorCode();
	print("! post_activation to %x: %d\n", tid.raw, errcode);
	return 0;
    }
    return  1;
}



