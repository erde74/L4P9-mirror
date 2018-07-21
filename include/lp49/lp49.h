#ifndef __LP49_H__
#define __LP49_H__ 

#include  <u.h>
#include  <l4all.h> 

/*
 *  Thread IDs:
 *    sigma0:    0x000C0001
 *
 *    hvm:       0x000C8001 
 *      pager:   0x000CC001
 *      printsrv:0x000D0001
 *
 *    pc:        0x01000002
 *    qsh:       0x02000002
 *    dossrv:    0x03000002
 *    9660srv:   0x04000002
 *
 *
 */

//--------------------------------------
#define  HVM_PROC      0X000C8001      // hvm
#define  MX_PAGER      0X000CC001      // [0, 51, 1]
#define  PROC_MANAGER  0X01000002      // pc [1, 0, 2]
#define  SRV_MANAGER   0X01000002      // pc [1, 0, 2]



//-------------------------------
#define  KIP_BASE   0x100000
// #define  UTCB_BASE  0x102000
#define  UTCB_BASE  0x200000

#define  UTCB_LIMIT     64
#define  VERSION    2


#define  THREAD_CONTROL      11
#define  SPACE_CONTROL       12
#define  ASSOCIATE_INTR      13
#define  DEASSOCIATE_INTR    14
#define  PROC2PROC_COPY      15
#define  ALLOC_DMA           16
#define  GET_PADDR           17
#define  PROC_MEMSET         18
#define  PHYS_MEM_ALLOC      19   

#define  MM_FORK             30
#define  START_VIA_PAGER     31
#define  MM_FREESPACE        32

#define  CHK_PGMAP           40

#define  L4PRINT_MSG         20
#define  L4PRINT_COLOR_MSG   21

#define  L4PRINT_SERVER      0xD0001  //????

/* THREADID
   +---8----+--10-----+----14-----+  
   |proc_nr |local_nr | version   |
   +--------+---------+-----------+
*/

// Proc_nr created dynamically resize PROCNRMIN <= .. <= PROCNRMAX 
#define  PROCNRMIN       8
#define  PROCNRMAX     255  

#define  LOCAL_BITS    10
#define  TID2PROCNR(tid)     ((tid.raw) >> (LOCAL_BITS + 14))
#define  TID2LOCALNR(tid)    (((tid.raw) >> 14) & 0x3FF)

#define  PROCNR2TID(procnr, localnr, version) \
         L4_GlobalId(((procnr)<<LOCAL_BITS | localnr), version)

//----------------------------------------------
#define  MSGTAG(label, flag, t, u)  \
        ((label)<<16 | (flag)<<12 | (t)<<6 | (u))



#define  IPC_FINISH     1
#define  IPC_COPYIN     2
#define  IPC_COPYOUT1   3
#define  IPC_COPYOUT2   4



//-------------------------------------
extern int get_new_tid_and_utcb (L4_ThreadId_t *tid, unsigned int *utcb);
extern int get_taskid(int tasknr, int localnr, L4_ThreadId_t *taskid, 
		      unsigned *kip, unsigned *utcb);
extern void mm_start_thread(L4_ThreadId_t target, void *ip,  
			    void *sp, L4_ThreadId_t pager);
extern int mm_create_start_task(L4_ThreadId_t task_tid, int num_tid,
				L4_Word_t kip, L4_Word_t utcb_base,
				L4_ThreadId_t  pager,  void  *ip, void *sp);
extern int mm_create_task(L4_ThreadId_t task_tid, int num_tid,
			  L4_Word_t kip, L4_Word_t utcb_base, L4_ThreadId_t  pager);

extern int set2task(char val, int tasknr, unsigned  to, int size);

extern int copy2task(char *from, int tasknr, unsigned  to, int size);
                                                                             
extern int create_mxpager();

extern void set_mproc_tbl(L4_ThreadId_t  task_id,  char  *name);
                                                                             
extern  L4_Word_t get_module_total();

extern int get_module_info( L4_Word_t index, char** cmdline,
			    L4_Word_t *haddr_start, L4_Word_t *size );

extern void print_modules_list();
                                                                             
extern void load_segment(L4_ThreadId_t target, int toadrs,
			 int fromadrs,  int size);

extern int  startup( );

extern int   bootinfo_init();                                           
extern int requestThreadControl(L4_ThreadId_t dest, L4_ThreadId_t space,
			L4_ThreadId_t sched, L4_ThreadId_t pager, L4_Word_t utcb );

extern int requestSpaceControl( L4_ThreadId_t dest, L4_Word_t control,
				L4_Fpage_t kipFpage, L4_Fpage_t utcbFpage, 
				L4_ThreadId_t redirect);

extern int requestAssociateInterrupt( L4_ThreadId_t irq_tid, L4_ThreadId_t handler_tid );

extern int requestDeassociateInterrupt( L4_ThreadId_t irq_tid );


#endif // __LP49_H__ 
