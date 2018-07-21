#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# close
int      close(int arg1)		// i
{   
    return syscall_i(CLOSE, arg1); 
}

