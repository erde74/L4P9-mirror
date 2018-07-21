#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

// ------[1] MM (Mem-proc) Server ----------------
#define  MM_SERVER    0xC8001        // [0, 50, 1]
#define  MX_PAGER     0xCC001        // [0, 51, 1]


int main()
{
    char tobe[] = "To be or not to be, that is a question";
    char pi[] ="3.1415926535 8979323846";
    int  tasknr, localnr, version, i;
    L4_Msg_t msgreg;
    L4_MsgTag_t tag;
    L4_ThreadId_t  memory_manager; 

    l4printf_r ("==== Guten morgen! ====\n");
    L4_ThreadId_t  myTid = L4_Myself();
    memory_manager.raw = MM_SERVER;

    tasknr = TID2PROCNR(myTid);
    localnr = TID2LOCALNR(myTid);
    version = L4_Version(myTid);

        for (i = 0; i<100; i++) {
	    L4_MsgClear(&msgreg);
	    L4_Set_MsgLabel(&msgreg, 7);
	    L4_MsgAppendWord(&msgreg, tasknr % 3);   //arg0
	    L4_MsgAppendWord(&msgreg, i );//arg1
	    
	    L4_MsgAppendSimpleStringItem(&msgreg, 
				   L4_StringItem(48, (void*)tobe));
	    L4_MsgAppendSimpleStringItem(&msgreg, 
				   L4_StringItem(30, (void*)pi));

	    L4_MsgLoad(&msgreg);
	    tag = L4_Send(memory_manager);

	    L4_Sleep(L4_TimePeriod(1000000));
	}
	l4printf_r("\n");

	L4_KDB_Enter ("Hello debugger!");
	for (;;);
};
