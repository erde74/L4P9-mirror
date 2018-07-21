#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _fstat
int      _fstat(int arg1, char* arg2)	// is
{   
    return syscall_is(_FSTAT, arg1, arg2); 
}

