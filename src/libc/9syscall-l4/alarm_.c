#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# alarm
long     alarm(ulong arg1)		// i
{   
    return syscall_i(ALARM, arg1); 
}

