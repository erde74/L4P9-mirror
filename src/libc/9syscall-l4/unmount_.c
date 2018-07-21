#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# unmount
int      unmount(char* name, char* old)		// ss
{   
    return syscall_ss(UNMOUNT, name, old); 
}

