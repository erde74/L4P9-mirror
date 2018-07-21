#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# wstat
int      wstat(char* name, uchar* edir, int nedir)	// sai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(edir, nedir, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_saim(WSTAT, name, edir, nedir, map); 
#else
    return syscall_sai(WSTAT, name, edir, nedir); 
#endif //----------------------------------------
}   


