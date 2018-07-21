//%%% SYSSRV-L4 %%%%

/*  
 * Library: libl4io
 */

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

#define  _DBGFLG  0
#include "../port/_dbgpr.h"   


extern L4_ThreadId_t  L4Print_Server;

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
    int   label, len, procnr, i;   char *cp;
    static int lastprocnr = 0;

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

	  procnr = TID2PROCNR(client);
	  if (procnr != lastprocnr) {
	    l4putc('>'); l4putc('>'); 
	    lastprocnr = procnr;
	  }

	  cp = strbuf;
	  for (i=0; (i<len) && cp; i++)
	    l4putc(*cp++);
 
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
    L4_ThreadId_t  myself, L4PRINT_SRV;
    myself = L4_Myself();
    static unsigned stack[256];

    get_new_tid_and_utcb(&L4PRINT_SRV, &utcb);
    L4_ThreadControl(L4PRINT_SRV, myself, myself, L4_Pager(), (void*)utcb);
    L4_Start_SpIp(L4PRINT_SRV, (L4_Word_t) &stack[250], (L4_Word_t)l4print_body);
    return L4PRINT_SRV;
} 

