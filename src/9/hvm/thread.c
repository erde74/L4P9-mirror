/*****************************************************
 *   L4minix/mm/l4-support.c  for pistachio.c        *
 *   2005 (c) NII KM                                 *
 *****************************************************/

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

// typedef unsigned int uint;


//----------------------------------------------
int get_new_tid_and_utcb (L4_ThreadId_t *tid, uint *utcb)
{
  static uint   th_num_base ;
  static uint   version ;
  static uint   th_indx = 0;
  static uint   utcb_size;
  static uint   utcb_base;
  static void * kip = 0;
  static L4_ThreadId_t  myself; 

  if (kip == 0){
    myself = L4_Myself();
    kip = L4_GetKernelInterface();
    utcb_size = L4_UtcbSize(kip);
    utcb_base = L4_MyLocalId().raw & ~(utcb_size - 1);
    th_num_base = L4_ThreadNo(myself);
    version = L4_Version(myself);
  }

  if (th_indx ++ > 32)  return 0;
  *tid = L4_GlobalId (th_num_base + th_indx, version);
  *utcb = utcb_base + utcb_size * th_indx;
  return 1;
}


//----------------------------------------------
int get_taskid(int tasknr, int localnr,
	       L4_ThreadId_t *taskid, unsigned *kip, unsigned *utcb)
{
  *taskid = PROCNR2TID(tasknr, localnr, VERSION);
  *kip = KIP_BASE;
  *utcb = UTCB_BASE + localnr * 0x200;
  return  1;
}

//--------------------------------------
void mm_start_thread(L4_ThreadId_t target, void *ip,  void *sp, L4_ThreadId_t pager)
{
  L4_Msg_t  _MRs;
  L4_MsgTag_t  tag;
  //  l4printf("start_thread TID=%X IP=%X SP=%X PAGER=%X\n",
  //	  target, ip, sp, pager.raw);
  L4_MsgClear(&_MRs);
  L4_MsgAppendWord(&_MRs, (L4_Word_t)ip);
  L4_MsgAppendWord(&_MRs, (L4_Word_t)sp);
  L4_Set_Propagation(&_MRs.tag);
  L4_Set_VirtualSender(pager);
  L4_MsgLoad(&_MRs);   // _MRs=[ 0, 2| ip| sp ]
  tag = L4_Send(target);
  if (L4_IpcFailed(tag)) L4_KDB_Enter("ERR in start_thread");
}

//=============================================================
int mm_create_start_task(L4_ThreadId_t task_tid, int max_thd_num, 
		      L4_Word_t kip, L4_Word_t utcb_base, 
		      L4_ThreadId_t  pager,  void  *ip, void *sp)
{ 
    int    rc;
    L4_Fpage_t kip_fp, utcb_fp;
    L4_Word_t  oldcntl;

    kip_fp = L4_Fpage((unsigned int)kip, 4096);    
    int utcb_size = (max_thd_num * 0x200 + 4095) & 0xFFFFF000;
    utcb_fp = L4_Fpage(utcb_base, utcb_size);
    //    l4printf("KIP=%X UTCB_BASE=%X PAGER=%X SP=%X IP=%X\n", 
    //	   kip, utcb_base, pager.raw, sp, ip);
    //    l4printf("KIP_FP=%X   UDP_FP=%X \n", kip_fp, utcb_fp);

    rc = L4_ThreadControl(task_tid, task_tid, L4_Myself(), L4_nilthread, (void*)(~0UL));
    //l4printf(" UTCB=%X  RC=%d \n", utcb_base, rc);
    if (rc == 0) return 0;

    rc = L4_SpaceControl(task_tid, 0, kip_fp, utcb_fp, L4_nilthread, &oldcntl);
    //    l4printf("  SpaceCtl UTCB=%X  KIP=%X  RC=%d  ErrorCode=%d  \n",  
    //	   L4_Address(utcb_fp), L4_Address(kip_fp), rc, L4_ErrorCode());
    if (rc == 0) return 0;

    rc = L4_ThreadControl(task_tid, task_tid, L4_Myself(), pager, (void*)utcb_base);
    //    l4printf(" ThdCnt UTCB=%X  RC=%d \n",  utcb_base, rc);
    if (rc == 0) return 0;

    mm_start_thread(task_tid, ip, sp, pager); //----
 
    //    l4printf(" mm_start_thread(%X, %X, %X)\n", task_tid.raw, ip, sp);
    return 1;
}

//---------------------------------------------------------------------
int mm_create_task(L4_ThreadId_t task_tid, int max_thd_num, 
		L4_Word_t kip, L4_Word_t utcb_base, L4_ThreadId_t  pager)
{ 
    int    rc;
    L4_Fpage_t kip_fp, utcb_fp;
    L4_Word_t  oldcntl;

    kip_fp = L4_Fpage((unsigned int)kip, 4096);    
    int utcb_size = (max_thd_num * 0x200 + 4095) & 0xFFFFF000;
    utcb_fp = L4_Fpage(utcb_base, utcb_size);

    //    l4printf("> create_task TaskID=%X KIP=%X UTCB=%X PAGER=%X  \n", 
    //	   task_tid, kip, utcb_base, pager.raw);

    rc = L4_ThreadControl(task_tid, task_tid, L4_Myself(), L4_nilthread, (void*)(~0UL));
    if (rc == 0) return 0;

    rc = L4_SpaceControl(task_tid, 0, kip_fp, utcb_fp, L4_nilthread, &oldcntl);
    //    printf("  SpaceCtl UTCB=%X  KIP=%X  RC=%d  ErrorCode=%d  \n",  
    //	   L4_Address(utcb_fp), L4_Address(kip_fp), rc, L4_ErrorCode());
    if (rc == 0) return 0;

    rc = L4_ThreadControl(task_tid, task_tid, L4_Myself(), pager, (void*)utcb_base);
    //    printf(" ThdCnt UTCB=%X  RC=%d \n",  utcb_base, rc);
    if (rc == 0) return 0;

    return 1;
}


//=================================================
extern int clone_space(int parent_procnr,  int child_procnr);

int mm_fork(L4_ThreadId_t parent_tid, L4_ThreadId_t  child_tid, int th_max, 
	    L4_ThreadId_t pager,  void *ip, void *sp)
{
    L4_Word_t    kip = KIP_BASE;
    L4_Word_t    utcb_base = UTCB_BASE;
    int  parent_procnr, child_procnr;
    int   rc;

    parent_procnr = TID2PROCNR(parent_tid);
    child_procnr  = TID2PROCNR(child_tid);

    rc = clone_space(parent_procnr, child_procnr);
    if (rc == 0)  return 0;

    rc = mm_create_start_task(child_tid, th_max, kip, utcb_base, pager, 
			      ip, sp);
    if (rc == 0)  return 0;

    return 1;
}


#if 0 //-----------------------------------------------
void print_mem(void * kip)
{
  int  i;
  for (i = 0; i<L4_NumMemoryDescriptors(kip); i++)
    { unsigned int x,y;
    L4_MemoryDesc_t memdesc = *L4_MemoryDesc(kip, i);
    x = memdesc.raw[0] & 0xFFFFFC00; y = memdesc.raw[1]+1024;
    l4printf("Low=%8X High=%8X-1 v=%X  t=%X type=%X size=%dK\n",
	   x, y,   (memdesc.raw[0] & 0x3FF) >> 9,
	   (memdesc.raw[0] & 0xF0) >> 4, memdesc.raw[0] & 0xF, (y-x)/1024);
    }
}
#endif //-----------------------------------------

