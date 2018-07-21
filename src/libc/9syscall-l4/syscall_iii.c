
#include  <u.h>
#include  <l4all.h>
#include  <l4/l4io.h>
#include "../port/_dbg.h" 


extern L4_ThreadId_t  SrvManager;
static   L4_ThreadId_t  _server;
#define  L4_Call(to)  L4_ReplyWait(to, &_server)


//----- iii ----------------------------------  
int syscall_iii(int syscallnr, int arg0, int arg1, int arg2)
{
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x111; //  pattern("iii");

    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, syscallnr);
    L4_MsgAppendWord(&msgreg, patrn);        
    L4_MsgAppendWord(&msgreg, arg0);  //arg0 i
    L4_MsgAppendWord(&msgreg, arg1);  //arg1 i
    L4_MsgAppendWord(&msgreg, arg2);  //arg1 i
 
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX

    L4_MsgStore(tag, &msgreg);
    result = L4_MsgWord(&msgreg, 0); 
    return result;
}


