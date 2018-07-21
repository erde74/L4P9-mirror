#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# fauth
int      fauth(int fd, char* aname)	// is
{   
    return syscall_is(FAUTH, fd, aname); 
}

