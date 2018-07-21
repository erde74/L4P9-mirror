#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# pwrite
long     pwrite(int fd, void* buf, long nbytes, vlong offset)	// iaiii
{
    union {
      vlong   v;
      ulong   u[2];
    } uv;
    uv.v = offset;   

    L4_MapItem_t  map; 
    map = covering_fpage_map(buf, nbytes, L4_Readable|L4_Writable);

    // l4printf("> pwrite(%d %x %x %x-%x %Lx)\n", 
    //	     fd, buf, nbytes, uv.u[0], uv.u[1], offset);
#if USE_MAPPING //--------------------------------------
    return syscall_iaiiim(PWRITE, fd, buf, nbytes, uv.u[0], uv.u[1], map);  
#else
    return syscall_iaiii(PWRITE, fd, buf, nbytes, uv.u[0], uv.u[1]);    // VLONG ?
#endif //--------------------------------------
}



