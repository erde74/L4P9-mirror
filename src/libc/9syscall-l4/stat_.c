#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# stat
int      stat(char* name, uchar* edir, int nedir)	// s?i
{  
    L4_MapItem_t  map; 
    map = covering_fpage_map(edir, nedir, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_saim(STAT, name, edir, nedir, map); 
#else
    return syscall_sai(STAT, name, edir, nedir); 
#endif //-------------------------------------------
}  

