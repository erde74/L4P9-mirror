#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# fd2path
int      fd2path(int fd, char* buf, int nbuf)	// iai
{   
    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, nbuf, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(FD2PATH, fd, buf, nbuf, map); 
#else
    return syscall_iai(FD2PATH, fd, buf, nbuf); 
#endif //-----------------------------------------
}


