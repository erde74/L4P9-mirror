#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# segfree
int      segfree(void* arg1, ulong arg2)	// ai
{   
    return syscall_ai(SEGFREE, arg1, arg2); 
}

