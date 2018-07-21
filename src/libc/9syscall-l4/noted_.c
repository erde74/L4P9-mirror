#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# noted
int      noted(int arg1)			// i
{   
    return syscall_i(NOTED, arg1); 
}


