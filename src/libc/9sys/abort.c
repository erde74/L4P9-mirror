//%-----------------------

#include <u.h>
#include <libc.h>

#include  <l4all.h>
void
abort(void)
{
#if 1 //%----------------------------------
        print(">>> abort >>>\n");
	_BACKTRACE(); 
	sleep(1000); 
	L4_Sleep(L4_Never);
	while(1) ;
#else //original--------------------------
	while(*(int*)0)
		;
#endif //%--------------------------------
}
