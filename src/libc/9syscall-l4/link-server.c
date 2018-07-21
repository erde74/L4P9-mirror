#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>
// #include  <l4/_dbgpr.h> 


L4_ThreadId_t  ProcManager = ((L4_ThreadId_t){raw: PROC_MANAGER}); // 0X1000002
L4_ThreadId_t  SrvManager = ((L4_ThreadId_t){raw: SRV_MANAGER});   // 0X1000002


void linkProcManager( )
{
  ProcManager.raw = 0X1000002;
}

