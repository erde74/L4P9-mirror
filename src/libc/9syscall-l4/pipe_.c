#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# pipe
int      pipe(int fd[2])	// 
{
#if 1 //------------------------------
    L4_Msg_t     msgreg;
    L4_MsgTag_t  tag;
    int          result;
    unsigned     patrn = 0x0; //  pattern("");
                                                                                
    L4_MsgClear(&msgreg);
    L4_Set_MsgLabel(&msgreg, PIPE);
    L4_MsgAppendWord(&msgreg, patrn);
                                                                                
    L4_MsgLoad(&msgreg);
    tag = L4_Call(SrvManager);
    if (L4_IpcFailed(tag))  return -1; //XXX
                                                                                
    L4_MsgStore(tag, &msgreg);
    result= L4_MsgWord(&msgreg, 0);  //MR1 
    fd[0] = L4_MsgWord(&msgreg, 1);  //MR2 carries fd[0]
    fd[1] = L4_MsgWord(&msgreg, 2);  //MR3 carries fd[1]
    //  l4printf_r("pipe:<%d %d>\n", fd[0], fd[1]);
    return  result;
#else //------------------------------
    return syscall_a(PIPE, fd); 
#endif //--------------------------------
}


