#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# segdetach
int      segdetach(void* arg1)			// a
{   
    return syscall_a(SEGDETACH, arg1); 
}

