#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# seek
vlong    seek(int fd, vlong n, int type)	// iii
{   
    union {
      vlong   v;
      ulong   u[2];
    } uv;
    uv.v = n;
    return syscall_iiii_v(SEEK, fd, uv.u[0], uv.u[1], type); 
}

