#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# notify
int      notify(void(*arg1)(void*, char*))	// a
{   
    return syscall_a(NOTIFY, arg1); 
}

