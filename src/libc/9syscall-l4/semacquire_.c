#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# semacquire
int      semacquire(long* arg1, int arg2)	// ai
{   
    return syscall_ai(SEMACQUIRE, arg1, arg2); 
}

