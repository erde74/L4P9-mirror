/*****************************************************
 *   l4-support.c                                    *
 *   2005 (c) nii : Shall be refined                 *
 *****************************************************/

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN   l4printf_b 

//typedef unsigned int uint;

#if 0
static int get_th_num( )
{
    static  int  next_th_num = 1;
    return  next_th_num ++; 
}
#endif

int get_new_tid_and_utcb (L4_ThreadId_t *tid, uint *utcb)
{
    static uint   th_num_base ;
    static uint   version ;
    static int    th_num_indx = 0; // WHY?? In GCC4.x, someone resets this value. 
    static uint   utcb_size;
    static uint   utcb_base;
    static void * kipage = 0;
    static L4_ThreadId_t  myself; 
    L4_ThreadId_t   tt;
    uint            uu;

    // Debugging dump -----------------------
    if ((tid == (L4_ThreadId_t*)0) ||  (utcb == (uint*)0)) {
        if (th_num_indx == 0) {
	  l4printf_b(">>new_tid(indx=%d kip=%x utcbbase=%x utcbsz=%d thnumbase=%x)\n", 
		     th_num_indx, kipage, utcb_base, utcb_size, th_num_base);
	  L4_KDB_Enter("th_num_indx");
	  return  -1;
	}
      return  1;
    }

    if (kipage == 0){
        myself = L4_Myself();
	kipage = L4_GetKernelInterface();
	utcb_size = L4_UtcbSize(kipage);
	utcb_base = L4_MyLocalId().raw & ~(utcb_size - 1);
	th_num_base = L4_ThreadNo(myself);
	version = L4_Version(myself);
	l4printf_g("==== <utcb_size=%d utcb_base=%x thnum_base=%x >====\n", 
	    utcb_size, utcb_base, th_num_base);
	// L4_KDB_Enter("");
    }

#if  1 
    th_num_indx++;
#else
    th_num_indx = get_th_num();
#endif

    if (th_num_indx >= 32)  {
        L4_KDB_Enter("Num of threads >= 32: to be extended");
	return  0;
    }

    tt = L4_GlobalId (th_num_base + th_num_indx, version);
    uu = utcb_base + utcb_size * th_num_indx;

    *tid = tt;
    *utcb = uu;

    DBGPRN("<<new_tid(indx=%d tid=%x utcb=%x) \n", th_num_indx, tt, uu);
    //L4_KDB_Enter("");

    return 1;
}


