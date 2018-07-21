/*
 * A user-level program.
 */

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

void main(int argc, char *argv[])
{
    L4_ThreadId_t  myTid = L4_Myself();
    int   i, j;
    int   m = 1, n = 256;

    l4printf_r ("==== Kernel Debugger [%x-%x] sp=%x fp=%x====\n", 
		myTid, &myTid);

    for (i=0; i<4; i++) {
      print(" going to sleep...\n");
      sleep(2000);
    }
    L4_KDB_Enter("hit 'g' to continue");

    L4_Yield();
    exits(0);
    //L4_Sleep (L4_Never);
};
