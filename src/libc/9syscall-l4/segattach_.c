#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# segattach
void* segattach(int arg1, char* arg2, void* arg3, ulong arg4) // isai
{   
    return (void*)syscall_isai(SEGATTACH, arg1, arg2, arg3, arg4); 
}

