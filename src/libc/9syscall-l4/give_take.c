#include  "sys.h"
#include  <u.h>
#include  <libc.h>

#include  <l4all.h>
#include  <lp49/lp49.h>

#include  "ipc-l4.h"

extern  L4_ThreadId_t  SrvManager;

//==================  give()/take() ======================================
int give(int fd,  char* buf,  int  length)
{
    L4_MapItem_t  mapitem;
    int          rc;

    mapitem = covering_fpage_map(buf, length, L4_Readable|L4_Writable);

    rc = syscall_iiim(GIVE, fd, (uint)buf, length, mapitem);

    return  rc;
}


int take(int fd,  char* buf,  int  length)
{
    L4_MapItem_t  mapitem;
    int          rc;

    mapitem = covering_fpage_map(buf, length, L4_Readable|L4_Writable);

    rc = syscall_iiim(TAKE, fd, (uint)buf, length, mapitem);
    return  rc;
}

