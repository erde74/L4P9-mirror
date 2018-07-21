/*
 * A user-level program.
 */

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

#define  l4printf print

void main(int argc, char *argv[])
{
  L4_ThreadId_t  myTid = L4_Myself();
  int   i;
  int   tasknr, localnr, version;
  char  msg[100]={0};
  register void *stkptr   asm("%esp");
  register void *frameptr asm("%ebp");

  l4printf_r ("==== Guten morgen! [%x-%x] sp=%x fp=%x====\n", 
	      myTid, &myTid, stkptr, frameptr);

  for(i = 0; i<argc; i++) {
      strcat(msg, " : "); 
      strcat(msg, argv[i]);
  }

  l4printf("argc=%d &argc=%x &argv=%x\n", argc, &argc, &argv);
  for(i = 0; i<argc; i++)
      l4printf("argv[%d] %x %x =%s\n", i, &argv[i], argv[i], argv[i]);


  tasknr = TID2PROCNR(myTid);
  localnr =  TID2LOCALNR(myTid);
  version = L4_Version(myTid);

	for (i = 0; i<12; i++) {
	  if (i%3 ==0)
	    l4printf_r("[%d-%d-%d] (^_^) %s\n", tasknr, localnr, version, msg);
	  else if(i%3 == 1)
	      l4printf_g("[%d-%d-%d] (~_~)\n", tasknr, localnr, version);
	  else if (i%3 == 2)
	      l4printf_b("[%d-%d-%d] (!_!)\n", tasknr, localnr, version);

	  L4_Yield();
	  L4_Sleep(L4_TimePeriod(100000));
	}
	l4printf_r("\n");

	//	L4_KDB_Enter ("Hello debugger!");
	L4_Yield();
	L4_Sleep (L4_Never);
};
