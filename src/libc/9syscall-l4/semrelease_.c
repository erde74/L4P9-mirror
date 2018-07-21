#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# semrelease
long     semrelease(long* arg1, long arg2)	// ai
{   
    return syscall_ai(SEMRELEASE, arg1, arg2); 
}

