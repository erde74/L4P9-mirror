#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//#  bind
int      bind(char* name, char* old, int flag)	// ssi
{   
    return syscall_ssi(BIND, name, old, flag); 
}

