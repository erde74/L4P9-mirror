#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# segbrk
void*    segbrk(void* arg1, void* arg2)	// aa
{   
    return (void*)syscall_aa(SEGBRK, arg1, arg2); 
}

