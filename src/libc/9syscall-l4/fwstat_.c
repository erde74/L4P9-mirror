#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# fwstat
int      fwstat(int fd, uchar* edir, int nedir)	// iai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(edir, nedir, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(FWSTAT, fd, (char*)edir, nedir, map); 
#else
    return syscall_iai(FWSTAT, fd, (char*)edir, nedir);  
#endif //------------------------------------------
}   

