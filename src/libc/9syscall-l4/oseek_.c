#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# oseek
long     oseek(int arg1, long arg2, int arg3)		// iii
{   
    return syscall_iii(OSEEK, arg1, arg2, arg3); 
}

