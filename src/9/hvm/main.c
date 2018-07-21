#include  "u.h"
#include  <l4all.h>
#include  <l4/l4io.h>

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>

//------------------------------------
#define  OK    0
#define  FALSE 0
#define  TRUE  1

typedef long Syscall(ulong*);  
Syscall *systab[64]; 

extern L4_ThreadId_t  L4Print_Server;
extern  L4_ThreadId_t   mx_pager;

extern  void  touch_pages();
extern  int   create_mxpager();
extern  L4_ThreadId_t create_l4print_srv( );
extern  int  startup( );
extern  int  bootinfo_init();

void reply(L4_ThreadId_t  to, int result);

int do_reply; 


/*===================================================================*
 *			main					     *
 *===================================================================*/
char parambuf1[256];
char parambuf2[256];

void main()
{
  int error;
  L4_ThreadId_t  client;
  L4_MsgTag_t      tag;
  L4_MsgBuffer_t   _BRS;
  L4_Msg_t         _MRS;
  //  L4_StringItem_t   si1 = L4_StringItem(256, parambuf1);
  //  L4_StringItem_t   si2 = L4_StringItem(256, parambuf2);
  int syscallx;
  int arg[8];

  set_l4print_direct(1);
  if (bootinfo_init() == 0) L4_KDB_Enter("bootinfo-init()");
  //  print_modules_list();    


  touch_pages();
  create_mxpager();
  DBGBRK("<> MX_PAGER= %X <> \n",  mx_pager.raw);
  L4_ThreadSwitch(mx_pager);


  create_l4print_srv( );  // l4print_srv() must be positioned after create_mxpager().
  DBGBRK("<> L4PRINT_SERVER= %X  \n", L4Print_Server.raw);

  startup( );   
  //  post_nexttask(0); 


  while (1) {
        L4_MsgBufferClear(&_BRS);
    //	L4_MsgBufferAppendSimpleRcvString(&_BRS, si1);
    //	L4_MsgBufferAppendSimpleRcvString(&_BRS, si2);
	parambuf1[0] = parambuf2[0] = 0;
	L4_AcceptStrings(L4_StringItemsAcceptor, &_BRS);
	tag = L4_Wait(&client);

        L4_MsgStore(tag, &_MRS);
	syscallx = L4_MsgLabel(&_MRS);
	L4_StoreMRs(1, 8, (L4_Word_t*)&arg);

	do_reply = 1;

	/* If the call number is valid, perform the call. */
	if (syscallx < 0 || syscallx >= 64)
	  error = -1;
	else
	  error = (*systab[syscallx])(& _MRS.raw[1]);

	if (! do_reply) continue;  // no reply for EXIT and WAIT 
//	if (syscallx == EXEC && error == OK) continue;

	reply(client, error);
  }
}


/*===========================================================*
 *		reply					     *
 *===========================================================*/

void reply(L4_ThreadId_t  to, int result)
{
  // TO BE DONE
}

