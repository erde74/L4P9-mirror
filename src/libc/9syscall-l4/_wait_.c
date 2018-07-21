#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# _wait
Waitmsg* _wait(void)				//
{
   return (Waitmsg*)syscall_(_WAIT); 
}


