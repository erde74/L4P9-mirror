#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# chdir
int      chdir(char* dirname)		// s
{   
    return syscall_s(CHDIR, dirname); 
}

