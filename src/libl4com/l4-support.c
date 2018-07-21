/*****************************************************
 *   l4-support.c                                    *
 *   2005 (c) nii : Shall be refined                 *
 *****************************************************/

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN   l4printf_r 

// typedef unsigned int uint;

#if 0 //-----------------------------
int get_new_tid_and_utcb (L4_ThreadId_t *tid, uint *utcb)
{
    static uint   th_num_base ;
    static uint   version ;
    static uint   th_num_indx = 0;
    static uint   utcb_size;
    static uint   utcb_base;
    static void * kinterfacep = 0;
    static L4_ThreadId_t  myself; 
    L4_ThreadId_t   tt;
    uint            uu;

    PRN("===<indx=%d kip=%x utcbbase=%x utcbsz=%d version=%d thnumbase=%x>===\n", 
	th_num_indx, kinterfacep, utcb_base, utcb_size, version, th_num_base);

    if (kinterfacep == 0){
        myself = L4_Myself();
	kinterfacep = L4_GetKernelInterface();
	utcb_size = L4_UtcbSize(kinterfacep);
	utcb_base = L4_MyLocalId().raw & ~(utcb_size - 1);
	th_num_base = L4_ThreadNo(myself);
	version = L4_Version(myself);
	PRN("==== <utcbsize=%d utcbbase=%x thnumbase=%x >====\n", 
	    utcb_size, utcb_base, th_num_base);
	L4_KDB_Enter("");
    }

    th_num_indx++;
    if (th_num_indx >= 32)  {
        L4_KDB_Enter("Num of threads >= 16: to be extended");
	return  0;
    }

    tt = L4_GlobalId (th_num_base + th_num_indx, version);
    *tid = tt;
    uu = utcb_base + utcb_size * th_num_indx;
    *utcb = uu;

    PRN("NewThread<indx=%d tid=%x utcb=%x> \n", th_num_indx, tt, uu);
    L4_KDB_Enter("");

    return 1;
}
#endif //--------------------------------

//------------------------------------------------
int requestThreadControl(L4_ThreadId_t dest, L4_ThreadId_t space,
		   L4_ThreadId_t sched, L4_ThreadId_t pager, L4_Word_t utcb )
{
    L4_Msg_t    _MRs;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  mx_pager;

    mx_pager.raw = MX_PAGER;
    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)dest.raw);  //_MR1
    L4_MsgAppendWord(&_MRs, (L4_Word_t)space.raw); //_MR2
    L4_MsgAppendWord(&_MRs, (L4_Word_t)sched.raw);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)pager.raw);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)utcb);
    L4_Set_MsgLabel(&_MRs, THREAD_CONTROL);
    L4_MsgLoad(&_MRs);
    tag = L4_Call(mx_pager);
    //  L4_MsgStore(tag, &_MRs);
    return L4_Label(tag);
}

//------------------------------------------------
int requestSpaceControl( L4_ThreadId_t dest, L4_Word_t control,
		   L4_Fpage_t kipFpage, L4_Fpage_t utcbFpage, L4_ThreadId_t redirect)
{
    L4_Msg_t    _MRs;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  mx_pager;

    mx_pager.raw = MX_PAGER;
    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)dest.raw);  //_MR1
    L4_MsgAppendWord(&_MRs, control);              //_MR2
    L4_MsgAppendWord(&_MRs, (L4_Word_t)kipFpage.raw);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)utcbFpage.raw);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)redirect.raw);
    L4_Set_MsgLabel(&_MRs, SPACE_CONTROL);
    L4_MsgLoad(&_MRs);
    tag = L4_Call(mx_pager);
    //  L4_MsgStore(tag, &_MRs);
    return L4_Label(tag);
}

//-----------------------------------
int requestAssociateInterrupt( L4_ThreadId_t irq_tid, L4_ThreadId_t handler_tid )
{
    L4_Msg_t    _MRs;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  mx_pager;

DBGPRN("<%s() \n", __FUNCTION__);

    mx_pager.raw = MX_PAGER;
    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)irq_tid.raw);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)handler_tid.raw);
    L4_Set_MsgLabel(&_MRs, ASSOCIATE_INTR);
    L4_MsgLoad(&_MRs);
    tag = L4_Call(mx_pager);
    //  L4_MsgStore(tag, &_MRs);
    return L4_Label(tag);
}

//--------------------------------------------------
int requestDeassociateInterrupt( L4_ThreadId_t irq_tid )
{
    L4_Msg_t    _MRs;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  mx_pager;
DBGPRN("<%s() \n", __FUNCTION__);

    mx_pager.raw = MX_PAGER;
    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)irq_tid.raw);
    L4_Set_MsgLabel(&_MRs, DEASSOCIATE_INTR);
    L4_MsgLoad(&_MRs);
    tag = L4_Call(mx_pager);
    //  L4_MsgStore(tag, &_MRs);
    return L4_Label(tag);
}

//------------------------------------------------
int proc2proc_copy(unsigned fromproc, unsigned fromadrs,
		   unsigned toproc, unsigned toadrs,
		   int  size)
{
    L4_Msg_t    _MRs;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  mx_pager;
DBGPRN("<%s() \n", __FUNCTION__);

    mx_pager.raw = MX_PAGER;
    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)fromproc);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)fromadrs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)toproc);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)toadrs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)size);
    L4_Set_MsgLabel(&_MRs, PROC2PROC_COPY);
    L4_MsgLoad(&_MRs);
    tag = L4_Call(mx_pager);
    //  L4_MsgStore(tag, &_MRs);
    return L4_Label(tag);
}


//------------------------------------------------
int  proc_memset(unsigned procnr, unsigned adrs,
		   int  value,  int size)
{
    L4_Msg_t    _MRs;
    L4_MsgTag_t  tag;
    L4_ThreadId_t  mx_pager;

DBGPRN("<%s() \n", __FUNCTION__);

    mx_pager.raw = MX_PAGER;
    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)procnr);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)adrs);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)value);
    L4_MsgAppendWord(&_MRs, (L4_Word_t)size);
    L4_Set_MsgLabel(&_MRs, PROC_MEMSET);
    L4_MsgLoad(&_MRs);
    tag = L4_Call(mx_pager);
    //  L4_MsgStore(tag, &_MRs);
    return L4_Label(tag);   //  
}


//--------------------------------------
void start_via_pager(L4_ThreadId_t pager, L4_ThreadId_t target,
		L4_Word_t  sp,  L4_Word_t  ip)
{
    L4_Msg_t  _MRs;

DBGPRN("<%s() \n", __FUNCTION__);

    L4_MsgClear(&_MRs);
    L4_MsgAppendWord(&_MRs, target.raw);
    L4_MsgAppendWord(&_MRs, sp);
    L4_MsgAppendWord(&_MRs, ip);
    L4_Set_MsgLabel(&_MRs, START_VIA_PAGER);
    L4_MsgLoad(&_MRs);   // msg=[START_VIA_PAGER, 0, 3 | target | sp | ip ]
    L4_Call(pager);
}


//--------------------------------------
void start_thread(L4_ThreadId_t target, void *ip,  void *sp, L4_ThreadId_t pager)
{
    L4_Msg_t  _MRs;
    L4_MsgTag_t  tag;

DBGPRN(".start_thread( TID=%X IP=%X SP=%X PAGER=%X)\n",  target, ip, sp, pager.raw);
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
int create_start_task(L4_ThreadId_t task_tid, int num_tid, 
		      L4_Word_t kip, L4_Word_t utcb_base, 
		      L4_ThreadId_t  pager,  L4_Word_t sp, L4_Word_t ip)
{ 
    int    rc;
    L4_Fpage_t kip_fp, utcb_fp;

DBGPRN("<%s() \n", __FUNCTION__);

    kip_fp = L4_Fpage((unsigned int)kip, 4096);    
    int utcb_size = (num_tid * 0x200 + 4095) & 0xFFFFF000;
    utcb_fp = L4_Fpage(utcb_base, utcb_size);


    rc = requestThreadControl(task_tid, task_tid, L4_Myself(), L4_nilthread, (~0UL));
    if (rc == 0) return 0;

    rc = requestSpaceControl(task_tid, 0, kip_fp, utcb_fp, L4_nilthread);
    DBGPRN("  spacectl utcb=%X  kip=%X  rc=%d  errorcode=%d  \n",  
    	   L4_Address(utcb_fp), L4_Address(kip_fp), rc, L4_ErrorCode());
    if (rc == 0) return 0;

    rc = requestThreadControl(task_tid, task_tid, L4_Myself(), pager, utcb_base);
    DBGPRN(" thredcnt utcb=%X  rc=%d \n",  utcb_base, rc);
    if (rc == 0) return 0;

    start_via_pager(pager, task_tid, sp, ip); //----
 
    l4printf_g("  Start_via_pager(%X, %X, %X)  errcode=%d  \n\n", 
	   task_tid.raw, sp, ip, L4_ErrorCode());
    return 1;
}

//---------------------------------------------------------------------
int create_task(L4_ThreadId_t task_tid, int num_tid, 
		L4_Word_t kip, L4_Word_t utcb_base, L4_ThreadId_t  pager)
{ 
    int    rc;
    L4_Fpage_t kip_fp, utcb_fp;
    DBGPRN("<create_task(tid:%x num:%d kip:%x utcb:%x pager:%x)\n",
	   task_tid, num_tid, kip, utcb_base, pager);

    kip_fp = L4_Fpage((unsigned int)kip, 4096);    
    int utcb_size = (num_tid * 0x200 + 4095) & 0xFFFFF000;
    utcb_fp = L4_Fpage(utcb_base, utcb_size);

    rc = requestThreadControl(task_tid, task_tid, L4_Myself(), L4_nilthread, (~0UL));
    DBGPRN(" threadctl tid=%x utcb=%x  rc=%d \n", 
	   task_tid.raw, utcb_base, rc);
    if (rc == 0) goto errLabel;

    rc = requestSpaceControl(task_tid, 0, kip_fp, utcb_fp, L4_nilthread);
    DBGPRN(" spacectl tid=%x utcb=%X kip=%X rc=%d\n",  
    	   task_tid, L4_Address(utcb_fp), L4_Address(kip_fp), rc);
    if (rc == 0) goto errLabel;

    rc = requestThreadControl(task_tid, task_tid, L4_Myself(), pager, utcb_base);
    DBGPRN(" threadctl tid=%x utcb=%x rc=%d\n",  task_tid, utcb_base, rc);
    if (rc == 0) goto errLabel;

    return 1;

 errLabel:
    l4printf_r("! l4-support:create_task failed, errcode=%d\n", L4_ErrorCode());
    return 0;
}

//------------------------------------------------------------------
L4_ThreadId_t create_start_thread(unsigned pc, unsigned stacktop)
{
    int   rc;
    L4_ThreadId_t  myself = L4_Myself();
    L4_ThreadId_t  mypager = L4_Pager();
    L4_ThreadId_t  newtid;
    L4_Word_t  utcb;
             
DBGPRN("<%s() \n", __FUNCTION__);
                                                                  
    get_new_tid_and_utcb(& newtid, (void*)& utcb);
                                                                               
    rc = requestThreadControl(newtid, myself, myself,
			      mypager, utcb);
    // l4printf_g(" thread<%x> created. ", newtid);
    L4_Start_SpIp(newtid, (L4_Word_t)stacktop, (L4_Word_t)pc);

    return newtid;
}

//------------------------------------------------------------------
L4_ThreadId_t create_thread(unsigned pc, unsigned stacktop)
{
    int   rc;
    L4_ThreadId_t  myself = L4_Myself();
    L4_ThreadId_t  mypager = L4_Pager();
    L4_ThreadId_t  newtid;
    L4_Word_t  utcb;
DBGPRN("<%s() \n", __FUNCTION__);
                                                                               
    get_new_tid_and_utcb(& newtid, (void*)& utcb);
                                                                               
    rc = requestThreadControl(newtid, myself, myself,
			      mypager, utcb);
    // l4printf_g(" thread<%X> created. ", newtid);

    return newtid;
}


#if 0 //------------------------------------------------
void print_tid (char *name, L4_ThreadId_t tid) 
{
  uint  x = tid.raw;
  l4printf_r(" %s<%X:%X-%X-%X> ", 
	 name, x, x>>(10+14), (x>>14) & 0xFFF, x & 0x3FFF);
}

//------------------------------------------------------------
void print_mem(void * kip)
{
  int  i;
  for (i = 0; i<L4_NumMemoryDescriptors(kip); i++)
    { unsigned int x,y;
    L4_MemoryDesc_t memdesc = *L4_MemoryDesc(kip, i);
    x = memdesc.raw[0] & 0xFFFFFC00; y = memdesc.raw[1]+1024;
    l4printf_r("Low=%8X High=%8X-1 v=%X  t=%X type=%X size=%dK\n",
	   x, y,   (memdesc.raw[0] & 0x3FF) >> 9,
	   (memdesc.raw[0] & 0xF0) >> 4, memdesc.raw[0] & 0xF, (y-x)/1024);
    }
  L4_KDB_Enter("memory");
}
#endif //------------------------------------------------------

