#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _fwstat
int      _fwstat(int arg1, char* arg2)		// is
{   
    return syscall_is(_FWSTAT, arg1, arg2); 
}

