#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# fversion
int      fversion(int arg1, int arg2, char* arg3, int arg4)	// iisi
{   
    return syscall_iisi(FVERSION, arg1, arg2, arg3, arg4); 
}


