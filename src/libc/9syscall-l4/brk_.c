#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# brk_
int      brk_(void* arg1)	// a
{   
    return syscall_a(BRK_, arg1); 
}


