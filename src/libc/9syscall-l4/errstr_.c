#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# errstr
int      errstr(char* err, uint nerr)		// si
     // {   return syscall_si(ERRSTR, arg1, arg2); }
{   
    return syscall_ai(ERRSTR, err, nerr); 
}  

