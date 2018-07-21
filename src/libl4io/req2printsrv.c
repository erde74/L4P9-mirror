#include <l4all.h>
#include <lp49/lp49.h>


static L4_ThreadId_t L4Print_Server = { .raw = L4PRINT_SERVER };
//  defined in include/lp49/lp49.h

static int strlen(const char *s)
{	
    int n;
    for ( n = 0; *s++ != '\0'; n++ ) ;
    return n;
}


void req2printsrv(char *s, int len, int color)
{
      // TO BE EXTENDED to suppor > 4K bytes. It's your task.
      if (len == 0) {
	  len = strlen(s) ;
	  if (len == 0) return;
      }

      if  (color == 0) 
	color = 14;
      L4_Msg_t     _MRs;
      L4_MsgTag_t  tag;
      L4_StringItem_t  sitem;

      if (len > 4096) {
	len = 4096;
	s[4095] = 0;
	L4_KDB_Enter("req2printsrv(): >4KB support is your job"); 
      }

      sitem = L4_StringItem(len, s);
      L4_MsgClear(&_MRs);
      L4_Set_MsgLabel(&_MRs, L4PRINT_MSG);
      L4_MsgAppendWord(&_MRs, len);
      L4_MsgAppendWord(&_MRs, color);
      L4_MsgAppendStringItem(&_MRs, &sitem);

      L4_MsgLoad(&_MRs);
      tag = L4_Call(L4Print_Server);
      if (L4_IpcFailed(tag)) L4_KDB_Enter("req2printsrv()");
}

