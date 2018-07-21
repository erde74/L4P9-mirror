#include  "sys.h"
#include  <u.h>
#include  <libc.h>

#include  <l4all.h>
#include  <lp49/lp49.h>

#include  "ipc-l4.h"

extern  L4_ThreadId_t  SrvManager;

#if 1 //====================================================

/*  COPYIN : CLIENT -> SERVER 
 *
 *    Client                       Server-gate     Server-clerk
 *
 *    Call   ----- PUT[adrs, len] ------>
 *     :                                 ----------> Wait
 *     :
 *     :                                           BR=<toadrs, len1>  :
 *    **     <--- COPYIN[adrs, len1] -------------- Reply/Send       :
 *                                                                    : copyin()
 *                                                                    :
 *    Call   ---- COPYIN[BufCopy<adrs,len1>]-------> Recv             :
 *     :
 *     :                                           BR=<toadrs2, len2> :
 *    **     <--- COPYIN[adrs2, len2] ------------ Reply/Send       :
 *                                                                    : copyin()
 *                                                                    :
 *    Call   ---- COPYIN[BufCopy<adrs2,len2>]------> Recv             :
 *     :
 *     :
 *    **     <----FIN[rseult]----------------------- Reply/Send
 *
 *    return  result;
 *
 */
 
int _put(int fd,  char* bufadrs,  int  length)
{
  L4_Msg_t       _MRs;
  L4_MsgTag_t    tag;
  L4_ThreadId_t  mate = SrvManager;
  L4_ThreadId_t  _server;
  L4_StringItem_t ss;
  int          result;
  unsigned     pattern = 0x121; // pattern("iai");
  int          cmnd, fromadrs, len;

  L4_MsgClear(&_MRs);
  L4_Set_MsgLabel(&_MRs, _PUT);
  L4_MsgAppendWord(&_MRs, pattern);
  L4_MsgAppendWord(&_MRs, fd);             //arg0 i
  L4_MsgAppendWord(&_MRs, (uint)bufadrs);  //arg1 a
  L4_MsgAppendWord(&_MRs, length);         //arg1 i

  while(1) {
    L4_LoadBR(0, 0);
    L4_MsgLoad(&_MRs);

    tag = L4_Call(mate);
    if (L4_IpcFailed(tag))
      return  -1;

    if (L4_IpcPropagated(tag))
      mate = L4_ActualSender(); 
    else
      mate = _server; 

    L4_MsgStore(tag, &_MRs);
    cmnd = L4_MsgLabel(&_MRs);   // MR[0]

    switch(cmnd) {
    case IPC_COPYIN:   // MR={ IPC_COPYIN | fromadrs | len }
      fromadrs = L4_MsgWord(&_MRs, 0);  // MR[1]
      len   = L4_MsgWord(&_MRs, 1);     // MR[2]

      L4_MsgClear(&_MRs);
      L4_Set_MsgLabel(&_MRs, IPC_COPYIN);
      ss = L4_StringItem(len, (void*)fromadrs);
      L4_MsgAppendSimpleStringItem(&_MRs, ss);
      continue;

    case IPC_FINISH:      // MR={ IPC_FINISH | result }
      result = L4_MsgWord(&_MRs, 0);  // MR[1]
      return  result;

    default:
      return  -1;
    }
  }
  return 0;  // for compiler
}

/*=====  COPYOUT : SRVR -> CLIENT ===============================================
 *
 *    Client                      Server-gate         Server-clerk
 *
 *    BR=<adrs>
 *    Call   ---GET[adrs, len] ---------->
 *                                        --------------> Wait
 *
 *    **  <---- COPYOUT1 [adrs, len1] ------------------- Reply/Send :
 *    BR=<fromadrs, len1>                                            : copyout()
 *                                                                   :
 *    Recv <-- COPYOUT2 [BufCopy<fromadrs,len1>] -------- Reply/Send :
 *
 *    Recv<---- COPYOUT1 [fromadrs2, len21] ------------- Reply/Send :
 *    BR=<fromadrs2, len2>                                           : copyout()
 *                                                                   :
 *    Recv <-- COPYOUT2 [BufCopy<fromadrs2,len1>] -----  Reply/Send :
 *
 *    Recv <---------- FINISH [result] ----------------- Send
 *
 *    return  result;
 *
 */

#define XXX if(0)print
#define YYY(t) \
  if(0)print("tag<%d %x %d %d> \n", t.raw>>16, (t.raw>>12)&0xf, (t.raw>>6)&0x3f, t.raw&0x3f)

int _get(int fd,  char* bufadrs,  int  length)
{
  L4_Msg_t        _MRs;
  L4_MsgBuffer_t  _BRs;
  L4_StringItem_t ss;
  L4_MsgTag_t     tag;
  L4_ThreadId_t   mate;
  L4_ThreadId_t   srvr;
  int          result;
  unsigned     pattern = 0x121; //  pattern("iai");
  uint         cmnd, subadrs,  sublen;

  L4_MsgClear(&_MRs);
  L4_Set_MsgLabel(&_MRs, _GET);
  L4_MsgAppendWord(&_MRs, pattern);
  L4_MsgAppendWord(&_MRs, fd);            //arg0 i
  L4_MsgAppendWord(&_MRs, (uint)bufadrs); //arg1 a
  L4_MsgAppendWord(&_MRs, length);        //arg1 i

  ss = L4_StringItem(length, (void*)bufadrs);
#if 0
  L4_LoadBR(0, 1);
  L4_LoadBR(1, ss.raw[0]);
  L4_LoadBR(2, ss.raw[1]);
#else
  L4_MsgBufferClear(&_BRs);
  L4_MsgBufferAppendSimpleRcvString(&_BRs, ss);
  L4_AcceptStrings(L4_StringItemsAcceptor, &_BRs);
#endif 

  L4_MsgLoad(&_MRs);
  tag = L4_Call(SrvManager); //
  if (L4_IpcFailed(tag)){
    print("get-Err 0 ");
    return -1;
  }
  L4_MsgStore(tag, &_MRs);

  cmnd = L4_MsgLabel(&_MRs);   // MR[0]

  if (L4_IpcPropagated(tag))
      mate = L4_ActualSender();
  else 
      mate = L4_nilthread;
  XXX("get(actsender=%x)  ", mate);

  while(1){
    switch(cmnd) {
    case IPC_COPYOUT1:  // MR={IPC_COPYOUT1; subadrs; sublen}
      subadrs = L4_MsgWord(&_MRs, 0);  // MR[1]
      sublen  = L4_MsgWord(&_MRs, 1);  // MR[2]

      L4_MsgClear(&_MRs);
      ss = L4_StringItem(sublen, (void*)subadrs);
#if 0
      L4_LoadBR(0, 1);
      L4_LoadBR(1, ss.raw[0]);
      L4_LoadBR(2, ss.raw[1]);
#else
      L4_MsgBufferClear(&_BRs);
      L4_MsgBufferAppendSimpleRcvString(&_BRs, ss);
      L4_AcceptStrings(L4_StringItemsAcceptor, &_BRs);
#endif 
      tag = L4_Wait(&srvr); // L4_Receive(SrvManager); --- NG. Why?
      //  MR={IPC_COPYOUT2; bufcpy<subadrs, sublen> }
      if (L4_IpcFailed(tag)) {
	  print("get-err-2 ");
	  return -1;
      }
      L4_MsgStore(tag, &_MRs);

      cmnd = L4_MsgLabel(&_MRs);   // MR[0]
      if (cmnd != IPC_COPYOUT2) 
	  print("get:copyout-err\n");
      
      tag = L4_Receive(SrvManager); //L4_Wait(&srvr); // 
      if (L4_IpcFailed(tag)) {
	  print("get-Err-3 ");
	  return -1;
      }
      L4_MsgStore(tag, &_MRs);
      cmnd = L4_MsgLabel(&_MRs);   // MR[0]
      continue;

    case IPC_FINISH:
      result = L4_MsgWord(&_MRs, 0); // MR[1]
      return result;

    default:
      return  -1;
    }
  }//while
  return 0;  // for compiler
}

#else //======================================================
long   _put(int arg1, void* arg2, long arg3)	// iai
{   
    return syscall_iai(_PUT, arg1, arg2, arg3); 
}


long   _get(int arg1, void* arg2, long arg3)	// iai
{   
    return syscall_iai(_GET, arg1, arg2, arg3); 
}
#endif //=================================================


