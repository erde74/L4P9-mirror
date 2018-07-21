#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _errstr
int      _errstr(char* arg1)		// s
{   
    return syscall_s(_ERRSTR, arg1);   
}

