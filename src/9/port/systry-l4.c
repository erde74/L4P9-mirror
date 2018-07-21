//%%%%%%%%%%%% systry-l4.c %%%%%%%%%%%%%%%%%%%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

#include        <l4all.h>
#include        <l4/l4io.h>
#include        <lp49/lp49.h>

#include  "../errhandler-l4.h"

#define   _DBGFLG  0 
#include  <l4/_dbgpr.h>

#define  pprint l4printf
#define  PRN    l4printf_b

extern L4_ThreadId_t  SrvManager; // = (L4_ThreadId_t){ raw: SRV_MANAGER };

extern Proc * tid2pcb(L4_ThreadId_t tid);
extern int proc2proc_copy(unsigned, unsigned, unsigned, unsigned, unsigned);
void dbg_time(int which, char *msg) ;

extern L4_ThreadId_t  client;
extern int core_process_nr;


//%======================================================================
#define   XBUFSZ   12288     //2048
static char    xxxbuf[XBUFSZ] ;

//%======================================================================

#define CHR(i)  ((i%128<'!')?('.'):((i%128>'}')?('~'):(i%128)))


long sysgive(ulong *arg, Clerkjob_t *myjob)
{
    uint    mapbase;
    uint    srvradrs;
    uint    fd = arg[0];
    uint    cltadrs = arg[1];
    uint    len = arg[2];
    int     i;

    DBGPRN("sysgive(%d %x %d)\n", arg[0], cltadrs, len);

    mapbase = L4_Address(myjob->mappee);
    srvradrs = (cltadrs - L4_Address(myjob->mapper)) + L4_Address(myjob->mappee);

    l4printf_b("sysgive mapper<%x-%x,%x,%x> mappee<%x-%x,%x,%x>\n", 
	       cltadrs,  cltadrs + len - 1,
	       L4_Address(myjob->mapper), L4_Size(myjob->mapper), 
	       srvradrs, srvradrs + len - 1,
	       L4_Address(myjob->mappee), L4_Size(myjob->mappee));
    for(i=0; i<XBUFSZ;i++) xxxbuf[i] = 0;

    if (len>XBUFSZ) len = XBUFSZ;
    memcpy(xxxbuf, (char*)srvradrs, len);

    //    L4_KDB_Enter("sysgive2"); 

    if (fd > 7){
	print("data-coreproc-received\n");
	for(i=0; i<len; i+=300) 
	  print("<%c%c>\t", xxxbuf[i], CHR(i));
    }

    // error-check
    myjob->mapper = L4_Nilpage;

    return len;
}


long systake(ulong *arg, Clerkjob_t *myjob)
{
    uint    mapbase;
    uint    fd = arg[0];
    uint    cltadrs = arg[1];
    uint    len = arg[2];
    uint    srvradrs;

    DBGPRN("systake(%d %x %d)\n", arg[0], cltadrs, len);
    mapbase = L4_Address(myjob->mappee);
    srvradrs = (cltadrs - L4_Address(myjob->mapper)) + L4_Address(myjob->mappee);

    l4printf_b("systake mapper<%x-%x,%x,%x> mappee<%x-%x,%x,%x>\n", 
	       cltadrs,  cltadrs + len - 1,
	       L4_Address(myjob->mapper), L4_Size(myjob->mapper), 
	       srvradrs, srvradrs + len - 1,
	       L4_Address(myjob->mappee), L4_Size(myjob->mappee));

    // L4_KDB_Enter("systake1"); 

    if (len>XBUFSZ) len = XBUFSZ;
    memcpy((char*)srvradrs, xxxbuf, len);

    //    L4_KDB_Enter("systake2"); 

    if (fd > 8){  int i;
	print("data-coreproc-sent\n");
	for(i=0; i<len; i+=300) 
	  print("<%c%c>\t", xxxbuf[i], CHR(i));
    }

    myjob->mapper = L4_Nilpage;
    return len;
}



//%===================================================================
int l4_copyin(L4_ThreadId_t clienttid, int adrsclient, char* adrssrvr, int length)
{
  L4_MsgTag_t     tag;
  L4_StringItem_t ss;

  DBGPRN(">%s(%x, %x, %x, %d) ", __FUNCTION__, clienttid, adrsclient, adrssrvr, length);
  
  L4_Set_VirtualSender(SrvManager);
  L4_LoadMR(0, MSGTAG(IPC_COPYIN, 1, 0, 2)); 
  L4_LoadMR(1, (uint)adrsclient); 
  L4_LoadMR(2, length);           

  ss = L4_StringItem(length, (void*)adrssrvr);
  L4_LoadBR(0, 1);   // L4_MsgBufferAppendSimpleRcvString(&_BRs, ss);
  L4_LoadBR(1, ss.raw[0]); // L4_AcceptStrings(L4_StringItemsAcceptor, &_BRs);
  L4_LoadBR(2, ss.raw[1]); 

  tag = L4_Call(clienttid); //MR{IPC_COPYIN; adrsclient; length} BR{...}
  if (L4_IpcFailed(tag))
    return -1;
  DBGPRN("<%s(\"%s\") ", __FUNCTION__, adrssrvr);
  return  1;
}


long sys_put(ulong *arg, Clerkjob_t *myjob)
{
        L4_ThreadId_t  client = myjob->client;  //% 
        DBGPRN(">%s(%x %x) from %x  ", __FUNCTION__, arg[1], arg[2], client);
	ulong  adrs = arg[1];
	ulong  len  = arg[2];

	if (len >= sizeof(xxxbuf)) 
	  len =  sizeof(xxxbuf);

#if 1 //------------------
	l4_copyin(client, adrs, xxxbuf, len);
#else //-----------------
	proc2proc_copy(TID2PROCNR(client), adrs, core_process_nr, (int)xxxbuf, len);
	//	DBGBRK("SYS_WRITE \n%s \n", xxxbuf);
#endif //------------------------
	return len;
}

//====================================================
#define XXX if(1)l4printf_g

int l4_copyout(L4_ThreadId_t clienttid, int adrssrvr, int adrsclient, int len)
{
#if 1 //---------------------------
    L4_MsgTag_t     tag;
    L4_StringItem_t ss;
    DBGPRN(">%s(%x %x %x %d) ", __FUNCTION__, clienttid, adrssrvr, adrsclient, len);

    L4_Set_VirtualSender(SrvManager);
    L4_LoadMR(0, MSGTAG(IPC_COPYOUT1, 1, 0, 2)); // L4_Set_MsgLabel(&_MRs, IPC_COPYOUT1);
    L4_LoadMR(1, adrsclient);
    L4_LoadMR(2, len);  

    tag = L4_Send(clienttid); // {IPC_COPYOUT1; adrsclient; len }
    if (L4_IpcFailed(tag)){
      l4printf_r("copyout-err-1 ");
      return -1;
    }  

    ss = L4_StringItem(len, (void*)adrssrvr);
    L4_Set_VirtualSender(SrvManager);
    L4_LoadMR(0, MSGTAG(IPC_COPYOUT2, 1, 2, 0));
    L4_LoadMR(1, ss.raw[0]);
    L4_LoadMR(2, ss.raw[1]);  

    tag = L4_Send(clienttid); //{IPC_COPYOUT2; BufCpy<adrssrvr,len>}
    if (L4_IpcFailed(tag)) {
      l4printf_r("copyout-err-2 ");
      return -1;
    }  
    
    L4_Sleep(L4_TimePeriod(1)); //!! This time delay is necessary.
    return  len;

#else //--------------------------------------
    L4_MsgTag_t    tag;
    L4_Msg_t  _MRs;
    L4_StringItem_t ss;
    L4_MsgClear(&_MRs);
    L4_Set_MsgLabel(&_MRs, IPC_COPYOUT1);
    L4_MsgAppendWord(&_MRs, adrsclient);
    L4_MsgAppendWord(&_MRs, len);
    L4_MsgLoad(&_MRs);
    tag = L4_Send(clienttid);
    if (L4_IpcFailed(tag)){
      l4printf_r("copyout-err-1 ");    return -1;
    }  
    L4_MsgClear(&_MRs);
    L4_Set_MsgLabel(&_MRs, IPC_COPYOUT2);
    ss = L4_StringItem(len, (void*)adrssrvr);
    L4_MsgAppendSimpleStringItem(&_MRs, ss);
    L4_MsgLoad(&_MRs);
    tag = L4_Send(clienttid);
    if (L4_IpcFailed(tag)){
      l4printf_r("copyout-err-2 ");    return -1;
    }  
    return  len;
#endif //--------------------------
}


long sys_get(ulong *arg, Clerkjob_t *myjob)
{
        L4_ThreadId_t  client = myjob->client;  //% 
        DBGPRN(">%s(%x %d) \n", __FUNCTION__, arg[1], arg[2]);
	ulong  adrs = arg[1];
	ulong  len  = arg[2];
	ulong  sz = strlen(xxxbuf);
	if (len > sz) 
	    len = sz;
#if 1
	l4_copyout(client, (uint)xxxbuf, adrs, len);
#else
	proc2proc_copy(core_process_nr, (int)xxxbuf, TID2PROCNR(client), adrs, len);
#endif
	return  len;
}


