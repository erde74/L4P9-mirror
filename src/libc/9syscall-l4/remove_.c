#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# remove
int      remove(char* arg1)	// s
{   
    return syscall_s(REMOVE, arg1); 
}

