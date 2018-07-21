#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _stat
int      _stat(char* arg1, char* arg2)	// ss
{   
    return syscall_ss(_STAT, arg1, arg2); 
}

