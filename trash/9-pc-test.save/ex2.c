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

    l4printf_r ("==== Guten morgen! [%x-%x] sp=%x fp=%x====\n", 
		myTid, &myTid);
    for (i = 0; i<argc; i++) 
        l4printf("[%d] %s \n", i, argv[i]);

    if (argc >= 2)  m = atoi(argv[1]);
    if (argc >= 3)  n = atoi(argv[2]);

    for (i = 0; i < m; i++) {

        for (j = 0; j<n; j++) {
	    l4_set_print_color(j);
	    l4printf("%3d", j % 256);
	}

	L4_Sleep(L4_TimePeriod(100000));
	l4printf_r("\n");

    }
    L4_Yield();
    exits("By");
	  //L4_Sleep (L4_Never);
};
