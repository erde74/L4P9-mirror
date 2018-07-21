//%%%%%% lock-l4.c %%%%%%%%%%
/*
 *  (C) KM@nii
 */

#include <u.h>
#include <libc.h>

#include  <l4all.h>
#include  <l4/l4io.h>
#include  "_dbg.h"

void
lock(Lock *lk)
{
	int i;

	/* once fast */
	if(!_tas(&lk->val))
		return;
	/* a thousand times pretty fast */
	for(i=0; i<1000; i++){
		if(!_tas(&lk->val))
			return;
		L4_Yield();    //%  sleep(0);
	}
	/* now nice and slow */
	for(i=0; i<1000; i++){
		if(!_tas(&lk->val))
			return;
		L4_Sleep(L4_TimePeriod(100));  //% unit:To be checked  sleep(100);
	}
	/* take your time */
	while(_tas(&lk->val))
	  L4_Sleep(L4_TimePeriod(1000));  //% sleep(1000);
}

int
canlock(Lock *lk)
{
	if(_tas(&lk->val))
		return 0;
	return 1;
}

void
unlock(Lock *lk)
{
	lk->val = 0;
}
