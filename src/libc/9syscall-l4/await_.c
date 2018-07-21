#include  "sys.h" 
#include  <u.h> 
#include  <libc.h> 
#include  "ipc-l4.h" 
#define  USE_MAPPING  1 

//# await
int      await(char* buf, int len)		// ai
{   
//l4printf_b(">await(%x %x)\n", buf, len);
#if  USE_MAPPING //--------------------------------------
    L4_MapItem_t  map;
    int  rc;
    map = covering_fpage_map(buf, len, L4_Readable|L4_Writable);
    rc = syscall_aim(AWAIT, buf, len, map); 
//l4printf_b("<await(%x %x)\n", buf, len);
    return rc;
#else 
    return syscall_si(AWAIT, buf, len);     //NG
#endif //-----------------------------------------------
}

