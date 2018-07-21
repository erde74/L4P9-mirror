#include  "sys.h"
#include  <u.h>
#include  <libc.h>

#include  <l4all.h>
#include  <lp49/lp49.h>

/*  
 *  This page mapping algorithm is poor and tentative. It must be refined. 
 *  Fpage(adrs, size) can become very large for some unfortunate address.
 */

#include  "ipc-l4.h"

extern  L4_ThreadId_t  SrvManager;

int  pagetouch; // To avoid code-elimination

//================== COVERING_FPAGE_MAP() ================================
L4_Fpage_t  covering_fpage(void *adrs, uint size)
{
    uint  fpagesize;
    uint  fpgmask;
    uint  fpagebase;
    L4_Fpage_t  fpage;

    for(fpagesize = 4096; fpagesize <= 0x200000; fpagesize *= 2) {
        if (fpagesize < size)  continue;

	fpgmask = ~(fpagesize - 1);
	fpagebase = (uint)adrs & fpgmask;

	if (((uint)adrs+size) <= (fpagebase + fpagesize)) {
	    fpage = L4_Fpage(fpagebase, fpagesize);
	    return fpage;
	}
    }
    return  L4_Nilpage;
}


L4_MapItem_t covering_fpage_map(void *adrs, uint size, uint perm)
{
    L4_Fpage_t  fpage;
    uint        pg;
    fpage = covering_fpage(adrs, size);
    //    if (L4_IsNilFpage(fpage)) ERR;
    fpage = L4_FpageAddRights(fpage, perm);

    //for (pg = (uint)adrs; pg < (uint)adrs + size; pg += 4096)
    for (pg = L4_Address(fpage); pg < L4_Address(fpage)+L4_Size(fpage); pg += 4096)
        pagetouch = *(int*)pg;

    return  L4_MapItem(fpage, 0);
}


//=============== debugging =================================
long     _d(char *arg1, void *arg2)	// ss
{
    return syscall_ss(_D, arg1, arg2);   
}

