#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# dup
int      dup(int oldfd, int newfd)	// ii
{   
    return syscall_ii(DUP, oldfd, newfd); 
}

