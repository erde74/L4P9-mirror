#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# exits
void     exits(char* msg)		// s
{   
    syscall_s(EXITS, msg); 
}

