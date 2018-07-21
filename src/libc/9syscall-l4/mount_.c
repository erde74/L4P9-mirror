#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# mount
int      mount(int fd, int afd, char* old, int flag, char* aname) // iisis
{   
    return syscall_iisis(MOUNT, fd, afd, old, flag, aname); 
}

