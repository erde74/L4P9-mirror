//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
 *  (m)  KM@nii
*/
#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../port/error.h"
#include "../port/edf.h"

#if 1 //%------------------------------------------------------
#include  <l4all.h>
#include  <l4/l4io.h>
                                                                                
#define   _DBGFLG  1
#include  <l4/_dbgpr.h>
#endif //%---------------------------------------------------

/*  defined in pc/dat.h. <--> libc.h for APL 
 *  struct Lock
 *  {
 *    ulong   key;
 *    ulong   sr;
 *    ulong   pc;
 *    Proc    *p;
 *    Mach    *m;
 *    ushort  isilock;
 *  };
 */


struct
{
	ulong	locks;
	ulong	glare;
	ulong	inglare;
} lockstats;

static void inccnt(Ref *r)
{
	_xinc(&r->ref);
}

static int deccnt(Ref *r)
{
	int x;

	x = _xdec(&r->ref);
	if(x < 0)
	  L4_KDB_Enter("deccnt pc");  //%
	return x;
}


static int localtas(void *adrs)
{
          int  tmp;
	  int  *intp = (int*)adrs;
          tmp = *intp;
          *intp = 0xdeaddead;
	  //	  l4printf("%c", (tmp)?'#':'.');
          return  tmp;
}


int lock(Lock *l)
{
	int i;

	if(tas(&l->key) == 0){
		l->isilock = 0;   
		return 0;
	}

	for(i=0; i<1000; i++) {
		if(tas(&l->key) == 0){
			l->isilock = 0;
			return 1;
		}
		L4_Yield();
	}

	while( tas(&l->key) )
	        L4_Sleep(L4_TimePeriod(100));

	l->isilock = 0;
	return 0;	
}

void ilock(Lock *l)
{

	if(tas(&l->key) != 0){

		if(!l->isilock){
		        l4printf_g("!isilock ");
			_backtrace_();
		}

		for(;;){
			while(l->key)
				;
			if(tas(&l->key) == 0)
				goto acquire;
		}
	}
acquire:

	l->isilock = 1;
}

int canlock(Lock *l)
{
	if(tas(&l->key)){
		return 0;
	}

	l->isilock = 0;
	return 1;
}

void unlock(Lock *l)
{
	l->key = 0;

	L4_Yield(); //%%%%
}

void iunlock(Lock *l)
{
	if(l->key == 0)
	        l4printf_g("iunlock: not locked: \n");  //%

	l->key = 0;
}
