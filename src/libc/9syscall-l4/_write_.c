#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _write
long     _write(int fd, void* buf, long len)	// iai
{ 
    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, len, L4_Readable|L4_Writable);
#if USE_MAPPING //--------------------------------------
    return syscall_iaim(_WRITE, fd, buf, len, map); 
#else
    return syscall_iai(_WRITE, fd, buf, len); 
#endif //---------------------------------------------
}

