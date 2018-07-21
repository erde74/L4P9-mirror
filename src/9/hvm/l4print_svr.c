//%%% SYSSRV-L4 %%%%

/*  
 * Library: libl4io
 */

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

#define  _DBGFLG  0
#include  <l4/_dbgpr.h>   


L4_ThreadId_t  L4Print_Server;
extern char __COLOR;

//-------------- LIKE THIS FEELING  --------------------------------

#define BUFLEN 4096
static char   strbuf[BUFLEN];

void l4print_body()
{
    L4_ThreadId_t  client;
    L4_MsgTag_t    tag;
    L4_Msg_t       _MRs;
    L4_MsgBuffer_t _BRs;  // to specify receive string
    L4_StringItem_t  si1;
    int   label, len, color, procnr, i;   char *cp;
    static int lastprocnr = 0;

    DBGBRK("PRINT_SRV= %X ...\n", L4_Myself().raw);
    while (1) {
        L4_MsgBufferClear(&_BRs);
	si1 = L4_StringItem(BUFLEN, strbuf);
	L4_MsgBufferAppendSimpleRcvString(&_BRs, si1);
	strbuf[0] = 0;
	L4_AcceptStrings(L4_StringItemsAcceptor, &_BRs);

	tag = L4_Wait(&client);

	L4_MsgStore(tag, &_MRs);
	label = L4_Label(tag);     // MR[0]
	if (label == L4PRINT_MSG) {
	  L4_StoreMR(1, (L4_Word_t*)&len);       // MR[1]
	  L4_StoreMR(2, (L4_Word_t*)&color);     // MR[2]

	  __COLOR = color;
	  procnr = TID2PROCNR(client);
	  if (procnr != lastprocnr) {
	    lastprocnr = procnr;
	  }

	  cp = strbuf;
	  for (i=0; (i<len) && cp; i++){

	      // if (*cp == '@') L4_KDB_Enter("hvm-prsvr"); //!!!

 	      l4putc(*cp++);
	  }

	  set_6845(12, 0);
	}

	L4_MsgClear(&_MRs);
	L4_LoadMR(0, 0); 
	tag = L4_Reply(client);
	// if (L4_IpcFailed(tag))  ; 
    }
}


L4_ThreadId_t create_l4print_srv( )
{
    unsigned  utcb;
    L4_ThreadId_t  myself;
    myself = L4_Myself();
    static unsigned stack[256];

    get_new_tid_and_utcb(&L4Print_Server, &utcb);

    if (L4Print_Server.raw != L4PRINT_SERVER)  // include/lp49/lp49.h
        L4_KDB_Enter("L4Print_Server: tid-mismatch"); 

    L4_ThreadControl(L4Print_Server, myself, myself, L4_Pager(), (void*)utcb);
    L4_Start_SpIp(L4Print_Server, (L4_Word_t) &stack[250], (L4_Word_t)l4print_body);
    return L4Print_Server;
} 

