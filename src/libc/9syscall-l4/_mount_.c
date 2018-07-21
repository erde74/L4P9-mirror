#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _mount
int      _mount(int arg1, char* arg2, int arg3, char* arg4)	// isis
{   
    return syscall_isis(_MOUNT, arg1, arg2, arg3, arg4); 
}

