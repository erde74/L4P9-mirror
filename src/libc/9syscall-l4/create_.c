#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# create
int      create(char* file, int omode, ulong perm)	// sii
{   
    return syscall_sii(CREATE, file, omode, perm); 
}

