#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# open
int      open(char* file, int omode)	// si
{   
    return syscall_si(OPEN, file, omode); 
}

