#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# segflush
int      segflush(void* arg1, ulong arg2)	// ai
{   
    return syscall_ai(SEGFLUSH, arg1, arg2); 
}

