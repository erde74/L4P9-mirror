#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _fsession
int      _fsession(char* arg1, void* arg2, int arg3)	// sai
{   
    return syscall_sai(_FSESSION, arg1, arg2, arg3); 
}

