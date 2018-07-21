#include  "sys.h"
#include  <u.h>
#include  <libc.h>

#if 1 //-------------------
#include  "ipc-l4.h"
#else //--------------------
  #include  <l4all.h>

  int syscall_(int syscallnr);
  int syscall_a(int syscallnr, void* arg0);
  int syscall_i(int syscallnr, int arg0);
#endif //-------------------


#if 1  // <--> sysproc-l4.c : sysr1( )
long     sysr1(ulong* arg1)		// a
{   return syscall_a(SYSR1, arg1);   }
#endif


int      sleep(long arg1)			// i
{   
    if (arg1 == 0) {
        L4_Yield();
	return 0;
    }
    else {
        vlong millisec = arg1;
	vlong microsec = millisec * 1000LL;
	L4_Sleep(L4_TimePeriod(microsec));              
	return 0;
    }
}

//  int      sleep(long arg1)                       // i
//  {   return syscall_i(SLEEP, arg1); }


