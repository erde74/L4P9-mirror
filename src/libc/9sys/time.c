//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
#include <u.h>
#include <libc.h>

//%------------------
#define  _DBGFLG  1     
#include  <l4/_dbgpr.h>

#include  <l4all.h>
extern  L4_Clock_t  L4_SystemClock();
//%-----------------

/*
 *  After a fork with fd's copied, both fd's are pointing to
 *  the same Chan structure.  Since the offset is kept in the Chan
 *  structure, the seek's and read's in the two processes can
 *  compete at moving the offset around.  Hence the unusual loop
 *  in the middle of this routine.
 */
static long
oldtime(long *tp)
{
	char b[20];
	static int f = -1;
	int i, retries;
	long t;

	memset(b, 0, sizeof(b));
	for(retries = 0; retries < 100; retries++){
		if(f < 0)
			f = open("/dev/time", OREAD|OCEXEC);
		if(f < 0)
			break;
		if(seek(f, 0, 0) < 0 || (i = read(f, b, sizeof(b))) < 0){
			close(f);
			f = -1;
		} else {
			if(i != 0)
				break;
		}
	}
	t = atol(b);
	if(tp)
		*tp = t;
	return t;
}

long
time(long *tp)
{
	vlong t;
	L4_Clock_t  clk;

	clk = L4_SystemClock();  // micro-sec

	t = clk.raw / 1000000LL;
	//% plan9	t = nsec()/1000000000LL;

	if(t == 0)
		t = oldtime(0);
	if(tp != nil)
		*tp = t;
	return t;
}
