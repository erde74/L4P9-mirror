/************************************************
 *      mm/mx-pager.c                           *
 *                                              *
 *   (c) 2000 University of Karlsruhe           *
 *   (c) 2006 NII  KM YS                        *
 *    2002/06/08                                *
 ************************************************/

/*
 * Very very dirty program  --> must be refined.
 *
 *  TO BE DONE:
 *  1) Rewriting nicely whole program.
 *  2) COW (Copy on write) support
 *  3) DMA page support
 *  4) Memory mapped resources
 *  ........
 *  "process" equals "task".
 */

/*                               2G
 *   -----------------------+----------------------------------------
 *                          |  available pages are mapped here .....
 *   -----------------------+----------------------------------------
 *
 *  Available physical pages are mappet from 2 giga in the HVM  space.
 *     logical-address-in-HVM == physical-address + 2Giga.
 *  memmap is a busy-idle page. 
 *
 */

#include <l4all.h>
#include <l4/l4io.h>
#include  <lp49/lp49.h>


#define NULL  0

#define  _DBGFLG  0 
#include <l4/_dbgpr.h>
#define  DBG  l4printgetc 

/*--------------------------------------*/
#define NR_PROCS  128
#define  MAXMEM       (236*1024*1024)
#define  MEMPOOL_ADRS  0x80000000
#define  MEMMAP_ADRS   0x70000000
#define  PAGER_STACK_SIZE  512

//--------------------------------------
typedef struct mproc  mproc_t;

struct mproc {
    L4_ThreadId_t  task_threadid;
    unsigned   pagelink;
    int    pages;
  //    char   pname[20];
  //    int    forked;     
} mproc[NR_PROCS];


//--------------------------------------
#define  INSIGMA0   127 
#define  RESERVED0  126 
#define  ALLOCATED    1 
#define  FREEPAGE     0 

static char memmap[MAXMEM / 4096]; // 236*1024/4 = 59*1024
//static char *memmap = (char *)MEMMAP_ADRS;     // at 1.75G

/*  memmap[pgx] corresponds
 *         Physical address:      pgx * 4096.
 *         Logical address in MM: pgx * 4096  + 2G. 
 *  The counter array is used instead of bit map to support Copy-On-Write.
 */

#define   INDEX_16M  4096   // 16 mega/4096
#define   INDEX_8M   2048   // 8 mega/4096
#define   INDEX_1M   256    // 1 mega/4096

typedef unsigned  pagemap_t [1024]; 

static int totalpages = 0;
static int index = INDEX_16M;
static int indexdma = INDEX_8M;		// INDEX_1M -> INDEX_8M  2006.10.02


//--------------------------------------
L4_ThreadId_t         mx_pager;

static L4_ThreadId_t  sigmaZero;
static L4_Word_t      pager_stack[PAGER_STACK_SIZE];
static unsigned char  BIOS_data[256];

//--------------------------------------------------
extern int mm_fork(L4_ThreadId_t parent_tid, L4_ThreadId_t  child_tid, 
		   int th_max, L4_ThreadId_t pager,  void *ip, void *sp);

static void pr_memmap();
static void print_BIOS_data(int offset, int bytes);
static int free_space(int procnr);
void dbg_dump_mem(char *title, unsigned char *start, unsigned size);
void dbg_pagemap(int procnr);

static void *_memset(void *s, int c, int n)
{
    char *s1 = s;
    if (n>0) {
        n++;
	while (--n > 0){ *s1++ = c; }
    }
    return s;
}

void *memcpy(void *s1, void *s2, int n)
{
    char *p1 = s1;
    const char *p2 = s2;
    {
        unsigned x = (unsigned)s1;
	unsigned y = (unsigned)s2;
	if ((x >= 0xe0000000) | (y >= 0xe0000000))
	    DBG("> memcpy(%x %x %d)\n", s1, s2, n); 
    }
    if (n) {
        n++;
	while (--n > 0) { *p1++ = *p2++;   }
    }
    return s1;
}

#if 0//---------------------------------------
void mm_init()
{
    _memset(mproc, 0, sizeof(mproc));
}
#endif 


//---------------------------------------------------------
int get_page_from_sigma0(L4_Word_t sigma0_adrs, L4_Word_t map_to)
{
    L4_Fpage_t  request_fp, result_fp, recv_fp;
    L4_MsgTag_t tag;
    L4_Msg_t msg;

    request_fp = L4_FpageLog2(sigma0_adrs, 12);
    recv_fp = L4_FpageLog2(map_to, 12);

    L4_MsgClear(&msg);
    L4_MsgAppendWord(&msg, request_fp.raw); //MR1
    L4_MsgAppendWord(&msg, 0);              //MR2 attribute
    L4_Set_MsgLabel(&msg, (L4_Word_t) -6UL << 4); //MR0-label
    L4_MsgLoad(&msg);
    L4_Accept(L4_MapGrantItems (recv_fp));

    tag = L4_Call(sigmaZero);
    if (L4_IpcFailed (tag)) {
        DBG("< get_page_from_sigma0: err tag=%x\n", tag.raw);
	return 0;
    }
    L4_MsgStore(tag, &msg);
  
    result_fp.raw = L4_MsgWord(&msg, 1); // L4_StoreMR(2, &result_fp.raw);
    //  l4printf("<%X %X %X %x> \n", L4_MsgWord(&msg, 0), L4_MsgWord(&msg, 1), 
    //  L4_MsgWord(&msg, 2), L4_IsNilFpage(result_fp));   L4_KDB_Enter("!");

    if( L4_IsNilFpage(result_fp) ) 
        return 0;
    else return 1;
}

/*---------------------------------------------------------------------
 * log2pages  0:1  1:2  2:4  3:8  4:16  5:32  6:64  7:128  8:256  9:512  10:1024   
 */
int get_pages_from_sigma0(L4_Word_t sigma0_adrs, L4_Word_t map_to, int log2pages)
{
    L4_Fpage_t  request_fp, result_fp, recv_fp;
    L4_MsgTag_t tag;
    L4_Msg_t msg;

    request_fp = L4_FpageLog2(sigma0_adrs, 12+log2pages);
    recv_fp = L4_FpageLog2(map_to, 12+log2pages);

    L4_Accept(L4_MapGrantItems (recv_fp));
    L4_MsgClear(&msg);
    L4_MsgAppendWord(&msg, request_fp.raw); //MR1
    L4_MsgAppendWord(&msg, 0);              //MR2 attribute
    L4_Set_MsgLabel(&msg, (L4_Word_t) -6UL << 4); //MR0-label
    L4_MsgLoad(&msg);

    tag = L4_Call(sigmaZero);
    if (L4_IpcFailed (tag)) {
        DBG("< get_page_from_sigma0: err tag=%x\n", tag.raw);
	return 0;
    }
    L4_MsgStore(tag, &msg);
  
    result_fp.raw = L4_MsgWord(&msg, 1); // L4_StoreMR(2, &result_fp.raw);
    //  l4printf("<%X %X %X %x> \n", L4_MsgWord(&msg, 0), L4_MsgWord(&msg, 1), 
    //  L4_MsgWord(&msg, 2), L4_IsNilFpage(result_fp));   L4_KDB_Enter("!");

    if( L4_IsNilFpage(result_fp) ) 
        return 0;
    else return 1;
}


#if 0 //---------------------------------------------------------
int get_anypage_from_sigma0(L4_Word_t map_to)
{
    L4_Fpage_t  request_fp, result_fp, recv_fp;
    L4_MsgTag_t tag;
    L4_Msg_t msg;
    request_fp = L4_FpageLog2( -1UL, 12);
    recv_fp = L4_FpageLog2(map_to, 12);

    L4_MsgClear (&msg);
    L4_MsgAppendWord (&msg, request_fp.raw); //MR1
    L4_MsgAppendWord (&msg, 0);              //MR2 attribute
    L4_Set_MsgLabel (&msg, (L4_Word_t) -6UL << 4); //MR0-label
    L4_MsgLoad (&msg);
    L4_Accept (L4_MapGrantItems (recv_fp));

    tag = L4_Call (sigmaZero);
    if (L4_IpcFailed (tag))  return 0;
    L4_MsgStore(tag, &msg);

    L4_StoreMR (2, &result_fp.raw);
    if( L4_IsNilFpage(result_fp) )   return 0;
    else  return 1;
}
#endif //----------------------------------------------


/*-----------------------------------------------------------*
 *        free page table                                    *
 *-----------------------------------------------------------*/

/*
 * Result  = 0:          Out of page
 *         = 4096*n + 2 Giga:   Logical page at (4096*n + 2 Giga) is allocated.  
 *                              Physical address is 4096*n. 
 */

unsigned  alloc_page(int procnr)
{
    unsigned  adrs;
    int i, j, r;

    for (i = index, j = 0; j < totalpages; i++, j++) {
        if (i == totalpages)
	  i = INDEX_16M;

	if (memmap[i] == INSIGMA0) {
	  r = get_pages_from_sigma0(i*4096, i*4096 + MEMPOOL_ADRS, 0);
	    if (r == 0) {
	        memmap[i] = RESERVED0;
		continue;
	    }
	    else 
	      break;
	}
	else if (memmap[i] == FREEPAGE)
	  break;
    }

    if (j >= totalpages -1){
        DBG("<alloc_page(process:%d): Out of memory\n", procnr);
	return 0;
    }

    memmap[i] = ALLOCATED;
    index = i+1;
    adrs  = ((i << 12) + MEMPOOL_ADRS);  // index*4096 + 2 Giga

    // l4printf("< alloc_page() adrs=%x ", adrs); 
    return  adrs;
}


/* 03-03-17 For INET DEBUG -----------------------------------------------
 * Result = 0:               Out of page
 *        = 4096*n + 2 Giga: Logical page at (4096*n + 2 Giga) is allocated.  
 *                       Physical address is 4096*n. 
 */

unsigned  alloc_phys_page(unsigned  p_adrs)
{
    unsigned  adrs;
    unsigned  pgx = p_adrs >> 12;

    if (memmap[pgx] == 0)  {
        memmap[pgx] = 1;
	adrs = ((pgx << 12) + MEMPOOL_ADRS);  // pgx*4096 + 2 Giga
	return adrs;
    }
    else
        return  0;
}


/*----------------------------------------------------------------
 * Result  = 0:   Out of page
 *        >  0:   Page at (4096*n + 2 Giga) is allocated.  
 *                Physical address is 4096*n. 
 */

unsigned  alloc_dma_page(int  pages)
{
    int       oldindex;
    unsigned  adrs;

    oldindex = indexdma;
    while (1)  {
        int  m, success;
	if (indexdma + pages >= INDEX_16M)  
	    indexdma = INDEX_8M;
	success = 1;
	for (m = 0; m < pages;  m++) {
	    if (memmap[indexdma + m] != 0) { // busy page
	        success = 0;
	        break;
	    }
	}
	//l4printf("<%d %d> ", pages, m); //--------
	if (success)	{
	    int  i;
	    adrs = ((indexdma << 12) + MEMPOOL_ADRS); //indexdma*4096 + 2 G
	    for (i = 0; i<pages; i++)
	        memmap[indexdma++] = 1;

	    if (indexdma == INDEX_16M)
	        indexdma = INDEX_8M;
	    //l4printf("adrs=%x ", adrs); L4_KDB_Enter("< alloc_dma_page"); 
	    return  adrs;
	}
	else	{
	    indexdma ++;
	    if (indexdma == oldindex)  return 0;
	    if (indexdma >=  INDEX_16M)  indexdma = INDEX_8M;
	}
    }
}


/*----------------------------------------------------------
 * adrs = 4096 * n + 2 Giga
 */

void free_page(unsigned  adrs)
{
    int  n;

    if (adrs < MEMPOOL_ADRS)   return;
    n = (adrs - MEMPOOL_ADRS) >> 12;
    if (n >= totalpages)   return ;  // error

    memmap[n] --;
    if ((n > INDEX_16M) && (memmap[n] == 0)) index = n;
    // if  (memmap[index] == 0) index = n;
}

#if 0//---------------------------------------------------------
void init_memmap( )
{
    int  i;
    for (i = 0; i < totalpages;  i++)
        memmap[i] = INSIGMA0;
}
#endif

//---------------------------------------------------------
int prepare_space(int procnr)
{
    unsigned    dirtblp;

    // allocate the page directory page
    dirtblp = alloc_page(procnr);

    if (dirtblp)  {
        mproc[procnr].pagelink = dirtblp;
	_memset((char*) dirtblp, 0, 4096);  // Clear all entries
	return  1;
    } 
    else  {
        L4_KDB_Enter("Out of memory");
	return 0;
    }
}

/**********************************************************************
          pagedir table         page table           Page

    p1 -> +---------+    p2 -> +----------+   p3 -> +---------+   Parent
          |         |          |          |         |         |
          |         |          |          |         |         |
          |         |          |          |         |         |
          |         |          |          |         |         |
          +---------+          +----------+         +---------+

    c1 -> +---------+    c2 -> +----------+   c3 -> +---------+   Child
          |         |          |          |         |         |
          |         |          |          |         |         |
          |         |          |          |         |         |
          |         |          |          |         |         |
          +---------+          +----------+         +---------+

**********************************************************************/
//typedef struct mproc  mproc_t ;
  
int clone_space(int parentprocnr,  int childprocnr)
{
    unsigned    **p1, **p2, **p3;
    unsigned    **c1, **c2, **c3;
    int         i, j;
    int         pagecnt;

    pagecnt = 0;

    // allocate the page directory page
    p1 = (unsigned **) mproc[parentprocnr].pagelink;
    c1 = (unsigned **) alloc_page(childprocnr);
    if (c1 == 0)  goto nomemory;
    pagecnt++; 

    mproc[childprocnr].pagelink = (unsigned) c1;
    _memset( (char*) c1, 0, 4096);

    // page table ------------------------------
    for ( i = 0; i < 1024; i++)  {
        p2 = (unsigned **) p1[i];

	if (p2) {
	    //  l4printf("p2=%x ", p2);   L4_KDB_Enter("clone 4");
	    c2 = (unsigned **) alloc_page(childprocnr);
	    if (c2 == 0)  goto nomemory;

	    pagecnt++;
	    _memset((char *) c2, 0, 4096);
	    //l4printf("<%d:%x>  %x -> %x ", 
	    //  i, i*4096*1024, &(c1[i]), c2); L4_KDB_Enter("5");

	    c1[i] = (unsigned *)c2; 

	    for (j = 0; j< 1024; j++) {
	        p3 = (unsigned**) p2[j];

		if (p3) {
		    //  l4printf("p3=%x ", p3);   L4_KDB_Enter("clone 6");
		    if (i==0 && j==0) continue;

		    c3 = (unsigned **)alloc_page(childprocnr);
		    if (c3 == 0)     goto  nomemory;
		    pagecnt++;
		    //  DBG("<%d,%d:%x>  %x->%x := %x", 
		    //     i, j, i*4096*1024+j*4096, & (c2[j]), c3, p3); 

		    memcpy((char *)c3, (char *)p3, 4096);
		    c2[j] =  (unsigned *)c3;
		}
		else {
		    c2[j] = (unsigned) 0;
		}
	    }
	}
    }

    mproc[childprocnr].pages = pagecnt;

    //  l4printf("[%d]  ", mproc[childprocnr].pages);  L4_KDB_Enter("< clone ");
    return  1;

 nomemory:
    free_space (childprocnr);
    return 0;
}


//---------------------------------------------------------
static int free_space(int procnr)
{
    unsigned  **p1, **p2, **p3;
    int         i, j;

    p1 = (unsigned **) mproc[procnr].pagelink;
    if (p1 == NULL)  return  0;

    for (i = 0; i<1024; i++) {
        p2 = (unsigned **)p1[i];
	if (p2 == NULL)	continue;

	for (j = 0; j<1024; j++) {
	    p3 = (unsigned **)p2[j];
	    if (p3 != 0 ) {
	        free_page((unsigned)p3);

		// DBG("FREE<%x> %x", i*4096*1024+j*4096, p3); 
	    }
	}
	free_page((unsigned)p2);
    }

    free_page((unsigned)p1);
    mproc[procnr].pagelink = 0;
    mproc[procnr].pages = 0;
    return  1;
}


/***********************************************************************
 *    mxpager_thread                                                   *
 ***********************************************************************/

char*  get_targetpage(int  tasknr, unsigned tgtadrs, unsigned ip)
{
    // "ip" is used only for DBGPRN(). No program logic is concerned.
    unsigned  targetpage;

    {
        pagemap_t  *dirtblp;
	pagemap_t  *pagetblp;
	      
	// Directory table -------------------------------------------
	if ((dirtblp = (pagemap_t *)mproc[tasknr].pagelink) == 0) {
	    if ( (dirtblp = (pagemap_t*)alloc_page(tasknr)) != 0)  {
	        mproc[tasknr].pagelink = (unsigned)dirtblp;
		mproc[tasknr].pages ++;
		_memset ((char*)dirtblp, 0, 4096);  // all clear
	    }
	    else  goto out_of_memory;
	}

	// Page table -----------------------------------------------
	if ((pagetblp = (pagemap_t *) (*dirtblp)[tgtadrs >> 22]) == 0)	{
	    if ((pagetblp = (pagemap_t*)alloc_page(tasknr)) != 0)   {
	        (*dirtblp)[tgtadrs >> 22] = (unsigned)pagetblp;
		mproc[tasknr].pages ++;
		_memset ((char*)pagetblp, 0, 4096);  // all clear
	    }
	    else  goto out_of_memory;
	}

	// Target page ---------------------------------------------
	if ((targetpage = (*pagetblp)[(tgtadrs >> 12) & 0x03FF]) == 0) 	{
	    targetpage = (unsigned)alloc_page(tasknr);

	    if (targetpage != 0)   {
	        (*pagetblp)[(tgtadrs >> 12) & 0x03FF] = targetpage;
		mproc[tasknr].pages ++;
	    }
	    else  {
	        l4printf ("TGTADRS=%x ", tgtadrs); 
		goto out_of_memory;
	    }
	}
	else  {  // cloned space page ------------------------
	    if (targetpage >= 0xe0000000) DBG("targetpage=%x\n", targetpage);
	}
    } //usual page mapping ------------------------------ 

    return  (char*)targetpage;

 out_of_memory:  
    L4_KDB_Enter("Pager: Out of page");
    return 0;
} 



void mxpager_thread(void) 
{
    L4_ThreadId_t  src;
    L4_Word_t      pfadrs, pf_page, ip;
    L4_Fpage_t     fpage;
    L4_MapItem_t   map;
    L4_Msg_t       _MRs;
    L4_MsgTag_t    tag;
    int            client_task, targettask, label, rc;  // client_task==targettask
    unsigned int   targetpage;
    //  L4_KDB_Enter("> mx-pager() starting");

    tag = L4_Wait(& src);  // wait for the first pagefault IPC to occur 
    targettask = TID2PROCNR(src);

    while (1) {
        if (L4_IpcFailed(tag))
	    L4_KDB_Enter("error on pagefault IPC");

	L4_MsgStore(tag, &_MRs);

	label = L4_MsgLabel(&_MRs); 
	//--- MR1: dest, MR2: space, MR3: sched, MR4: pager, MR5: utcbloc -----
	if (label == THREAD_CONTROL)
	{ 
	    L4_ThreadId_t  dest, space, sched, pager;
	    void  * utcbloc;
	    dest.raw = L4_MsgWord(&_MRs, 0);
	    space.raw = L4_MsgWord(&_MRs, 1);
	    sched.raw = L4_MsgWord(&_MRs, 2);
	    pager.raw = L4_MsgWord(&_MRs, 3);
	    utcbloc = (void*) L4_MsgWord(&_MRs, 4);
	    rc = L4_ThreadControl(dest, space, sched, pager, utcbloc);
	    if (rc == 0) 
	        l4printf_r("mx_pager:ThreadControl:%d-%d src:%x dst:%x spc:%x u:%x\n", 
			   rc, L4_ErrorCode(), src.raw, dest.raw, space.raw, utcbloc);

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, rc);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//-- MR1: spcae, MR2: control, MR3: kip, MR4: utcb, MR5 redirector -----
	else if (label == SPACE_CONTROL) 
	{
            L4_ThreadId_t  space, redirector;
	    L4_Word_t      control, oldcntl;
	    L4_Fpage_t     kipFpage, utcbFpage;

	    space.raw = L4_MsgWord(&_MRs, 0);
	    control = L4_MsgWord(&_MRs, 1);
	    kipFpage.raw = L4_MsgWord(&_MRs, 2);
	    utcbFpage.raw = L4_MsgWord(&_MRs, 3);
	    redirector.raw =  L4_MsgWord(&_MRs, 4);
	    rc = L4_SpaceControl(space, control, kipFpage, utcbFpage, redirector, &oldcntl);
	    if (rc == 0) 
	        l4printf_r("mx_pager:SpaceControl;%d-%d\n", rc, L4_ErrorCode());

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, rc);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//--- MR1: intThread, MR2: intHandler -------------
	else if (label == ASSOCIATE_INTR)
	{
	    L4_ThreadId_t  intrThread, intrHandler;
	    intrThread.raw = L4_MsgWord(&_MRs, 0);
	    intrHandler.raw = L4_MsgWord(&_MRs, 1);
	    rc = L4_AssociateInterrupt(intrThread, intrHandler);

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, rc);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//--- MR1: intThread ------------------------
	else if (label == DEASSOCIATE_INTR)
	{
            L4_ThreadId_t  intrThread;
	    intrThread.raw = L4_MsgWord(&_MRs, 0);
	    rc = L4_DeassociateInterrupt(intrThread); 
	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, rc);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//--- MR1: fromproc, MR2: fromaddr, MR3: toproc, MR4: toaddr, MR5: size ------
	else if (label == PROC2PROC_COPY)
	{
	    unsigned  fromproc, fromadrs;
	    unsigned  toproc,   toadrs;
	    int       size;

	    fromproc = L4_MsgWord(&_MRs, 0);
	    fromadrs = L4_MsgWord(&_MRs, 1);
	    toproc = L4_MsgWord(&_MRs, 2);
	    toadrs = L4_MsgWord(&_MRs, 3);
	    size   = L4_MsgWord(&_MRs, 4);

	    extern  int proc2proc_copy(int fromproc, unsigned fromadrs,
				       int toproc,   unsigned toadrs,
				       int size);
	  
	    proc2proc_copy(fromproc, fromadrs, toproc, toadrs, size);

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, 1);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//-- MR1: procnr, MR2: adrs, MR3: value, MR4: size --------------
	else if (label == PROC_MEMSET)
	{
	    unsigned  procnr;
	    unsigned  adrs;
	    int       value, size;
	    void*     rc; 

	    procnr  = L4_MsgWord(&_MRs, 0);
	    adrs    = L4_MsgWord(&_MRs, 1);
	    value   = L4_MsgWord(&_MRs, 2);
	    size    = L4_MsgWord(&_MRs, 3);

	    extern  void* proc_memset(int procnr, unsigned adrs,
				      int value,  int size);
	  
	    rc = proc_memset(procnr, adrs, value, size);

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, 1);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//-- MR1: parent, MR2: child, MR3: th_max, MR4: pager, MR5: ip, MR6: sp -------
	else if (label == MM_FORK)
	{
	    L4_ThreadId_t  parent_tid;
	    L4_ThreadId_t  child_tid;
	    int  th_max;
	    L4_ThreadId_t  pager;
	    void  *ip;
	    void  *sp;
	    int   rc;

	    parent_tid = (L4_ThreadId_t)L4_MsgWord(&_MRs, 0);
	    child_tid = (L4_ThreadId_t)L4_MsgWord(&_MRs, 1);
	    th_max = L4_MsgWord(&_MRs, 2);
	    pager = (L4_ThreadId_t)L4_MsgWord(&_MRs, 3);
	    ip   = (void*)L4_MsgWord(&_MRs, 4);
	    sp   = (void*)L4_MsgWord(&_MRs, 5);

	    rc = mm_fork(parent_tid, child_tid, th_max, pager, ip, sp);

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, rc);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//-- MR1: procnr -------
	else if (label == CHK_PGMAP)
	{
	    int  procnr;
	    unsigned int  adrs;
	    int  len;

	    procnr = L4_MsgWord(&_MRs, 0);
	    if (procnr>128) procnr = 1;
	    adrs = L4_MsgWord(&_MRs, 1);
	    len = L4_MsgWord(&_MRs, 2);
	    if (len == 0) len =64;

	    if (adrs == 0)  
	      dbg_pagemap(procnr);
	    else {
	        unsigned int  mxadrs;
		mxadrs = (unsigned int)get_targetpage(procnr, adrs, 0) + (adrs & 0x00000FFF); 
		// crossing page not supported.
		dbg_dump_mem("", (unsigned char*)mxadrs, len);
	    }

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, 0);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

        //--- MR1: target, MR2: sp, MR3: ip ------------------
	else if (label == START_VIA_PAGER)
        {
            L4_ThreadId_t  target;
	    unsigned  sp;
	    unsigned  ip;

	    target = (L4_ThreadId_t)L4_MsgWord(&_MRs, 0); //_MR1
	    sp   = L4_MsgWord(&_MRs, 1);  //_MR2
	    ip   = L4_MsgWord(&_MRs, 2);  //_MR3

	    DBGPRN("start_via_pager(tid=%x sp=%x ip=%x)\n", 
		   target.raw, sp, ip);

	    L4_MsgClear(&_MRs);
	    L4_MsgAppendWord(&_MRs, ip); //_MR1
	    L4_MsgAppendWord(&_MRs, sp); //_MR2
	    L4_MsgLoad(&_MRs); 
	    tag = L4_Send(target);
	    if (L4_IpcFailed(tag)) 
	        l4printf_r("start_via_pager tag=%x\n", tag.raw);

	    L4_MsgClear(&_MRs);
	    L4_MsgLoad(&_MRs); 
	    tag = L4_ReplyWait(src,  &src);
	    continue;
        }

	//---------------------------------------------------------
	else if (label == MM_FREESPACE)
	{
	    unsigned  procnr;
	    int    rc; 

	    procnr  = L4_MsgWord(&_MRs, 0);
	    rc = free_space(procnr);

	    L4_MsgClear(&_MRs);
	    L4_Set_MsgLabel(&_MRs, 1);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//--- MR1: size, MR2: l_addr, ----------------------------
	else if (label == ALLOC_DMA)
	{
	    L4_Word_t size, logical_addr, physical_addr, hvm_addr;
	    L4_Word_t logical_page, tgt_logical_page;
	    L4_Fpage_t fpage;
	    L4_MapItem_t map;
	    int targettask;
	    int pages;
	    int i;

	    unsigned **dirtblp, **pagetblp, *targetpage;

	    size = L4_MsgWord(&_MRs, 0);
	    logical_addr = L4_MsgWord(&_MRs, 1);
	    pages = ((logical_addr + size - 1) >> 12) - (logical_addr >> 12) + 1;

	    //l4printf_r("ALLOC_DMA size=%d, logical_addr=0x%x, pages=%d\n", size, logical_addr, pages);

	    if(size <= 0) goto alloc_dma_nomemory;

	    hvm_addr = alloc_dma_page(pages);
	    if(hvm_addr == 0) goto alloc_dma_nomemory;

	    //l4printf_r("hvm_addr=0x%x, *hvm_addr=%d\n", hvm_addr, *((int *)hvm_addr));

	    physical_addr = hvm_addr - MEMPOOL_ADRS;

	    targettask = TID2PROCNR(src);
	    // === Directory table =====
	    dirtblp = (unsigned **) mproc[targettask].pagelink;
	    if(dirtblp == 0){
	        L4_KDB_Enter("ALLOC_DMA request process has no page directory tables");
	    }
		
	    for(i = 0; i < pages; i++) {
		logical_page = (logical_addr + i * 4096) & 0xfffff000;

		// === Page table ======
		pagetblp = (unsigned **) dirtblp[logical_page >> 22];

		if(pagetblp == 0){
		    //l4printf_r("allocate page table for 0x%x (begin)\n", logical_page);
		    pagetblp = (unsigned **) alloc_page(targettask);
		    //l4printf_r("allocate page table (end)\n");
		    if(pagetblp == 0) goto alloc_dma_nomemory;
		    dirtblp[logical_page >> 22] = (unsigned *)pagetblp;
		    mproc[targettask].pages ++;
		    _memset((char *) pagetblp, 0, 4096);
		}

		//l4printf_r("setpagetable 0x%x (begin)\n", hvm_addr + i * 4096);
		targetpage = pagetblp[(logical_page >> 12) & 0x03ff];
		if(targetpage == 0){
		    pagetblp[(logical_page >> 12) & 0x03ff] = (unsigned *)(hvm_addr + i * 4096);
		    mproc[targettask].pages ++;
		    //l4printf_r("hoge 0x%x\n", (int)(hvm_addr + i * 4096));
		}else{
		    L4_KDB_Enter("ALLOC_DMA requested logical page is already alocated");
		}
		//l4printf_r("setpagetable 0x%x (end)\n", hvm_addr + i * 4096);
	    }
		
	    tgt_logical_page = logical_addr & 0xfffff000;

	    fpage = L4_Fpage(hvm_addr, pages * 4096);
	    L4_Set_Rights(&fpage, L4_FullyAccessible);
	    map = L4_MapItem(fpage, tgt_logical_page); 
	  
	    L4_MsgClear(&_MRs);
	    L4_MsgAppendWord(&_MRs, 0);
	    L4_MsgAppendWord(&_MRs, hvm_addr - MEMPOOL_ADRS);
	    L4_MsgAppendMapItem(&_MRs, map);
	    L4_MsgLoad(&_MRs);

	    tag = L4_ReplyWait(src,  &src);
	    continue;

	alloc_dma_nomemory:
	    L4_MsgClear(&_MRs);
	    L4_MsgAppendWord(&_MRs, (L4_Word_t)-1);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//--- MR1: phys_addr MR2: logical_addr in pc, MR3: log2size --------
	else if (label == PHYS_MEM_ALLOC)
	{
	    L4_Word_t logical_addr, physical_addr, hvm_addr;
	    L4_Word_t logical_page;
	    L4_Fpage_t fpage;
	    L4_MapItem_t map;
	    int targettask;
	    int pages;
	    int i, r, log2size, log2pages, phys_pgx;
	    unsigned in_sigma0, in_mxpager;
	    unsigned **dirtblp, **pagetblp, *targetpage;

	    physical_addr = L4_MsgWord(&_MRs, 0) & 0XFFFFF000;
	    logical_addr  = L4_MsgWord(&_MRs, 1) & 0XFFFFF000;
	    log2size     = L4_MsgWord(&_MRs, 2);
	    log2pages = log2size - 12;
	    pages = 1 << log2pages;
	    phys_pgx = physical_addr >> 12;

	    //	    l4printf_r("mxpager-PHYSMEM(0x%x 0x%x %d)\n", 
	    //		       physical_addr, logical_addr, log2size); 

	    in_sigma0 = 1;
	    in_mxpager = 1;
	    for (i = phys_pgx; i < phys_pgx+pages; i++){
	      in_sigma0 &= (memmap[i] == INSIGMA0);
	      in_mxpager &= (memmap[i] == FREEPAGE);
	    } 
	    //  l4printf_r("mxpager:in_sigma0=%d, in_mxpager=%d \n", in_sigma0, in_mxpager); 

	    if (in_mxpager) {
	    } 
	    else if (in_sigma0) {
	      r = get_pages_from_sigma0(physical_addr, 
					physical_addr+MEMPOOL_ADRS, log2pages);
	      if (r == 0)  goto phys_mem_alloc_fail;
	    }
	    else
	        goto phys_mem_alloc_fail;

	    //---------------------------------------
	    hvm_addr = physical_addr + MEMPOOL_ADRS; 

	    targettask = TID2PROCNR(src);
	    // === Directory table =====
	    dirtblp = (unsigned **) mproc[targettask].pagelink;
	    if(dirtblp == 0){
	        L4_KDB_Enter("ALLOC_DMA request process has no page directory tables");
	    }
		
	    for(i = 0; i < pages; i++) {
		logical_page = (logical_addr + i * 4096) & 0xfffff000;

		// === Page table ======
		pagetblp = (unsigned **) dirtblp[logical_page >> 22];

		if(pagetblp == 0){
		    pagetblp = (unsigned **) alloc_page(targettask);
		    if(pagetblp == 0) 
		        goto phys_mem_alloc_fail;
		    dirtblp[logical_page >> 22] = (unsigned *)pagetblp;
		    mproc[targettask].pages ++;
		    _memset((char *) pagetblp, 0, 4096);
		}

		targetpage = pagetblp[(logical_page >> 12) & 0x03ff];
		if(targetpage == 0){
		    pagetblp[(logical_page >> 12) & 0x03ff] = (unsigned *)(hvm_addr + i * 4096);
		    mproc[targettask].pages ++;
		}
		else{
		    L4_KDB_Enter("requested page is already allocated");
		}
	    }
		
	    fpage = L4_FpageLog2(hvm_addr, log2size);
	    L4_Set_Rights(&fpage, L4_FullyAccessible);
	    map = L4_MapItem(fpage, logical_addr); 
	  
	    L4_MsgClear(&_MRs);
	    L4_MsgAppendWord(&_MRs, pages*4096);      // MR1: size 
	    L4_MsgAppendWord(&_MRs, physical_addr);   // MR2: phys addr
	    L4_MsgAppendMapItem(&_MRs, map);          
	    L4_MsgLoad(&_MRs);

	    //	    l4printf_r("<PHYSMEM(0x%x 0x%x %d)\n", 
	    //		       physical_addr, logical_addr, log2size); 

	    tag = L4_ReplyWait(src,  &src);
	    continue;

	phys_mem_alloc_fail:
	    L4_MsgClear(&_MRs);
	    L4_MsgAppendWord(&_MRs, (L4_Word_t)0);  // MR1 
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//--- MR1: laddr ---------------------------------------------
	else if (label == GET_PADDR)
	{
	    L4_Word_t vaddr;
	    int client_task;
	    unsigned targetpage, hvm_addr;

	    vaddr = L4_MsgWord(&_MRs, 0);

	    client_task = TID2PROCNR(src);
	    targetpage = (unsigned)get_targetpage(client_task, vaddr, -1);
	    hvm_addr = targetpage + (vaddr & 0x0FFF);

	    //	  l4printf_b("  paddr taskid=0x%x, vaddr=0x%08x, paddr=0x%08x\n",
	    //		      client_task, vaddr, hvm_addr - MEMPOOL_ADRS);

	    L4_MsgClear(&_MRs);
	    L4_MsgAppendWord(&_MRs, hvm_addr - MEMPOOL_ADRS);
	    L4_MsgLoad(&_MRs);

	    tag = L4_ReplyWait(src,  &src);
	    continue;
	}

	//============== Page fault ==============================
	//---- MR1: pfaddr, MR2: ip  -----------------------------
	else {
	    if ((label>>4) != 0xFFE) { // If not page-fault:
	        DBGBRK("Mxpager <- illegalMsg src=%X label=%X pfadrs=%X ip=%X MR0=%X\n", 
		       src.raw, label, pfadrs, ip, tag);
	    }

	    pfadrs = L4_MsgWord(&_MRs, 0);  // read MR1 (not MR0 -- strange spec) 
	    ip = L4_MsgWord(&_MRs, 1);      // read MR2 (not MR1 -- strange spec)

	    client_task = TID2PROCNR(src);
	    pf_page = pfadrs & 0xFFFFF000;

	    targetpage = (unsigned)get_targetpage(client_task, pfadrs, ip);

	    if (pfadrs < (L4_Word_t)0x100000) {
	        DBGBRK("Page fault lower than 1M at 0x%x from %x ip:%x -> map:%x \n", 
		       pfadrs, src.raw, ip, targetpage); //???
	    }

	    fpage = L4_Fpage(targetpage, 4096);
	    L4_Set_Rights(&fpage, L4_FullyAccessible);
	    map = L4_MapItem(fpage,  pf_page);  // pf_page ?

	    // l4printf("PageFault at <%X %X> <=MAP= %X \n", client_task, pfadrs, 
	    //      L4_Address(fpage)); //L4_KDB_Enter("PagerReply"); 
        
	    /*** apply the mapping and wait for next fault ***/
	    L4_MsgClear(&_MRs);
	    L4_MsgAppendMapItem(&_MRs, map);
	    L4_MsgLoad(&_MRs);
	    tag = L4_ReplyWait(src,  &src);
	    if (L4_IpcFailed(tag)) 
	      DBGBRK(" mx-pager: map-ipc[%X] failed\n", pfadrs);
	}
    }
}


//------------------------------------------------------------
typedef  struct
{
    int       num;
    unsigned  low, high;
    int       v;
    int       type;
    int       t;
    unsigned  size;
}  memregion_t;

static memregion_t  memtbl[40];
static memregion_t  tmp;

unsigned  installed_max_mem()
{
    void * kip = L4_GetKernelInterface();
    int  i, num, n;
    unsigned  max = 0, x, y, type, t, v;

    num = n = L4_NumMemoryDescriptors(kip);
    for (i = 0; i < num; i++)
    { 
        L4_MemoryDesc_t memdesc = *L4_MemoryDesc(kip, i);
	x = memdesc.raw[0] & 0xFFFFFC00; y = memdesc.raw[1]+1024;
	type = memdesc.raw[0] & 0xF;
	t = (memdesc.raw[0] & 0xF0) >> 4;
	v = (memdesc.raw[0] & 0x200) >> 9;

	if ((type == 1) && (v == 0)) { // conventional && physical  
	    if (y > max) max = y;
	}
	memtbl[i] = (memregion_t){  i, x, y,  v, type, t, (y-x)/1024};
    }

    do {  //Sorting by low field
        for (i = 0; i < n-1; i++){
	    if (memtbl[i].low > memtbl[i+1].low) {
	        tmp = memtbl[i];
		memtbl[i] = memtbl[i+1];
		memtbl[i+1] = tmp;
	    }
	}
    } while  (--n > 0);

#if 1 //Print memory region list -----------------------------
#define  MT   memtbl[i]    
    for (i = 0; i<num; i++) {
        char *tp ="";
	switch (MT.type){
	case 0x0: tp = "Undef"; break;
	case 0x1: tp = "ConventionalMem"; break;
	case 0x2: tp = "ReservedMem"; break;
	case 0x3: tp = "DedicatedMem"; break;
	case 0x4: tp = "SharedMem"; break;
	case 0xe: 
	    switch(MT.t){ 
	    case 0: tp = "CODE: Misc";  break;
	    case 1: tp = "CODE: Initdata"; break;
	    case 2: tp = "CODE: L4Server"; break;
	    case 3: tp = "CODE: Module";   break;
	    default:tp = "CODE: -----";
	    }
	    break;
	case 0xf: tp = "ArchDepend";  break;
	default:  tp = "";
	}

	l4printf("[%2d] <%8X-%8X-1> %s type-t=<%X %X> size=%dK \t%s \n",
	         i, MT.low, MT.high, (MT.v)?"VM":"PM", MT.type, MT.t, MT.size, tp);
	//if (i && (i % 10) == 0) L4_KDB_Enter("memory");
    }
#endif //------------------------------------
    DBGBRK("INSTALLED MAX MEM= %X  \n", max);  
    //    L4_KDB_Enter("memory");

    return max;
}

//--------------------------------------------------------------
void mem_set_test(unsigned *start, unsigned size )
{
    int  i;
    l4printf_r(">mem_set_test(%x %x)\n", &start[0], size);
    for(i=0; i<size/4; i+=1024) {
        start[i] = (unsigned)i*4;
	l4printf_r("<%x> ", &start[i]);
    }
}

//-------------------------------------------------------------
int create_mxpager( )                 
{
    unsigned int   memmappages;   // map_adrs;
    signed int     i, j;
    //  L4_ThreadId_t  myid = L4_Myself(); 
    unsigned       max_padrs;

    totalpages = 0;
    sigmaZero = L4_Pager(); 
    // DBG("MemSvr(%X) starting, pager: %X...\n", L4_Myself().raw, sigmaZero.raw);

    {
        int    i;
	char *cp;
	for (i = 0, cp = (char*)0x400;  i<256; i++, cp++)
	    BIOS_data[i] = *cp;
    }

    max_padrs = installed_max_mem(); 

    if (max_padrs > MAXMEM)  
        max_padrs = MAXMEM;
  
    totalpages = max_padrs / 4096;      // Total pages
    memmappages = (totalpages + 4095) / 4096;

    for (i = 0; i < NR_PROCS; i++) {
        mproc[i].pagelink = 0;
	mproc[i].pages = 0;
    }  

    l4printf("max_padrs=%x  totalpages= %d  memmappages= %d \n", 
	     max_padrs, totalpages,  memmappages);

    // snatch all available pages and build free map.------
    for (j = 0; j < totalpages; j++) {
        memmap[j] = INSIGMA0; // touch memmap pages 
    }

    for (j = INDEX_1M; j < INDEX_16M; j++)  {
        int rc;
	rc = get_pages_from_sigma0(j*4096, MEMPOOL_ADRS + j*4096, 0);

	if (rc == 0) {
	    memmap[j] = RESERVED0;  
	}
	else {
	    memmap[j] = FREEPAGE;   
	}
	if (j % 512 == 0)  l4printf(".");  
    }

    //DBG("\nMemory server: Free Mem= %d pages \n", mproc[FREELIST].pages);
    l4printf("\n"); // pr_memmap(); 

#if 0 //test only ----------------------------
    check_sigma0_pages( );

    mem_set_test((unsigned)MEMPOOL_ADRS+(unsigned)12*1024*1024, 2*1024*1024);
#endif //------------------------------

    
    {  // start pager 
        unsigned int  utcb;
	L4_ThreadId_t  myself = L4_Myself();

	get_new_tid_and_utcb(&mx_pager, &utcb);
	L4_ThreadControl(mx_pager, myself, myself, L4_Pager(), (void*)utcb);
	L4_Start_SpIp(mx_pager,  (L4_Word_t) &pager_stack[PAGER_STACK_SIZE-1],
		      (L4_Word_t)mxpager_thread);
    }
    //  DBG("MXPAGER is created: %x \n", mx_pager.raw);
    return 1;
}

//----------------------------------------------------------------
void  check_sigma0_pages( )
{
    int  i, r; 
    unsigned  start = 0, end = 0;
    int  last = 0;

    l4printf_b("Available phys-pages: ");
    for (i = 0; i < totalpages; i++) {
      r = get_pages_from_sigma0(i*4096, i*4096 + MEMPOOL_ADRS, 0);

	if ((last == 0) & (r == 0)){
	  memmap[i] = RESERVED0;
	  continue;
	}
	else if ((last == 0) & (r != 0)){
	  memmap[i] = FREEPAGE;
	  start = i;
	  last = 1;
	  continue;
	}
	else if ((last == 1) & (r != 0)){
	  memmap[i] = FREEPAGE;
	  continue;
	}
	else if ((last == 1) & (r == 0)){
	  memmap[i] = RESERVED0;
	  end = i;
	  // l4printf_b("\t<%dM%dK, %dM%dK-1, %dM%dK> ", 
	  //	     start*4/1024, start*4%1024, end*4/1024, end*4%1024,
	  //	     (end-start)*4/1024, (end-start)*4%1024 ); 
	  l4printf_b("\t<%X, %X-1, %dM%dK> ", 
		     start*4096,  end*4096, 
		     (end-start)*4/1024, (end-start)*4%1024 ); 
	  last = 0;
	  continue;
	}
    }
    if (last != 0) {
          end = totalpages;
	  // l4printf_b("\t<%dM%dK, %dM%dK-1, %dM%dK> \n", 
	  //	     start*4/1024, start*4%1024, end*4/1024, end*4%1024,
	  //	     (end-start)*4/1024, (end-start)*4%1024 ); 
	  l4printf_b("\t<%X, %X-1, %dM%dK> ", 
		     start*4096,  end*4096, 
		     (end-start)*4/1024, (end-start)*4%1024 ); 
    }
}


//---------------------------------------
#define  OUTSIDE  0
#define  INSIDE   1

static void pr_memmap()
{
    int  i, startx = 0, endx = 0;
    int  state = OUTSIDE;

    for (i=0; i< totalpages; i++)  {
        if (memmap[i] == 0) {  // page-i is idle 
	    if (state == OUTSIDE){
	        state = INSIDE;
		startx = i;
	    }
	}
	else {
	    if (state == INSIDE) {
	        state = OUTSIDE;
		endx = i - 1;
		l4printf(" <%X(%dM%dK)-%X(%dM%dK)> \n", 
			 startx*4096, (startx*4)/1024, (startx*4)%1024,  
			 endx*4096, (endx*4)/1024, (endx*4)%1024);
	    }
	}
    }
}

//--- 031218 -------------------------------------------------
static void print_BIOS_data(int offset, int bytes)
{
    int    i;
    l4printf("BIOS[%x]:%x", offset);
    for (i = bytes -1; i >= 0; i--)
        l4printf("%x", BIOS_data[offset+i]);
    l4printf("                   \n");
}


//--------------------------------------------------------------
//  +----------------+---------------+----------------+
//  |        ********|***************|****            |
//  +----------------+---------------+----------------+

int copy2task(char *from, int tasknr, unsigned  to, int size)
{
    unsigned  to_offset;
    unsigned  to_page;
    unsigned  sz;

    // DBG("> copy2task(from=%X task=%X to=%X size=%d)\n", from, tasknr, to, size);
    to_offset = to & 0xfff;
    if (to_offset + size <= 4096 )
    {
        to_page = (unsigned)get_targetpage(tasknr, to, -1);
	if (to_page ==0)  
	    return  0;

	memcpy( (char*)(to_page + to_offset), from, size);
	return 1;
    } 

    {
        sz = 4096 - to_offset;
	to_page = (unsigned)get_targetpage(tasknr, to, -1);
	if (to_page ==0)  
	    return  0;

	memcpy( (char*)(to_page + to_offset), from, sz);
	to += sz;    // to % 4096 == 0
	from += sz;  
	size -= sz;
    }

    while (size)
    {
        if (size > 4096)
	    sz = 4096;
	else
	    sz = size; 

	to_page = (unsigned)get_targetpage(tasknr, to, -1);
	if (to_page ==0)  
	    return  0;

	// DBG("#4 ");  //??????
	memcpy( (char*)(to_page ), from, sz);
	to += 4096;
	from += 4096;
	size -= sz;
    }

    return  1;
}

//------------------------------------------
int set2task(char val, int tasknr, unsigned  to, int size)
{
    unsigned  to_offset;
    unsigned  to_page;
    unsigned  sz;

    // DBG("> set2task(val=%X task=%X to=%X size=%d) \n", val, tasknr, to, size);
 
    to_offset = to & 0xfff;
    if (to_offset + size <= 4096 )
    {
        to_page = (unsigned)get_targetpage(tasknr, to, -1);
	if (to_page ==0)  
	    return  0;
	_memset( (char*)(to_page + to_offset), val, size);
	return 1;
    } 

    {
        sz = 4096 - to_offset;
	to_page = (unsigned)get_targetpage(tasknr, to, -1);
	if (to_page ==0)  
	    return  0;
	_memset( (char*)(to_page + to_offset), val, sz);
	to += sz;    // to % 4096 == 0
	size -= sz;
    }

    while (size)
    {
        if (size > 4096)
	    sz = 4096;
	else
	    sz = size; 

	to_page = (unsigned)get_targetpage(tasknr, to, -1);
	if (to_page ==0)  
	    return  0;
	_memset( (char*)(to_page ), val, sz);
	to += 4096;
	size -= sz;
    }

    return  1;
}



//-----------------------------------------------------
void dbg_pagemap(int procnr)
{

    unsigned **p1, **p2;
    int  i, j;
    unsigned  endpgadrs = 0;
    unsigned  startpgadrs = 0;
    int  flag = 0;

    l4printf_r("pgmap[%d]: ", procnr);
    p1 = (unsigned**)mproc[procnr].pagelink;

    if (p1) {
        for (i=0; i<1024; i++) {
	    p2 = (unsigned **)p1[i];
	    if (p2) {
	        for (j=0; j<1024; j++) 
		  if (p2[j]) {
		    if (flag == 0) {
		      flag = 1;
		      startpgadrs = (i << 22) | (j << 12);
		    }
		  }
		  else{
		      if (flag == 1) {
			flag = 0;
			endpgadrs = (i << 22) | (j << 12);
			l4printf_r("[%x-%x)%d  ", startpgadrs, endpgadrs, 
				   (endpgadrs - startpgadrs)/4096);
		      }
		  }
	    }
	    else {
	        if (flag == 1) {
		    flag = 0;
		    endpgadrs = (i << 22);
		    l4printf_r("[%x-%x)%d  ", startpgadrs, endpgadrs, 
			       (endpgadrs - startpgadrs)/4096);
		}
	    }
	}
    }
    l4printf_r("\n");
}


//-------------------------------------------------

char * _strcat(char *ret, const char *s2)
{
  register char *s1 = ret;

  while (*s1++ != '\0')  /* EMPTY */ ;
  s1--;
  while ((*s1++ = *s2++))  /* EMPTY */ ;
  return ret;
}


void dbg_dump_mem(char *title, unsigned char *start, unsigned size)
{
  int  i, j, k;
  unsigned char c;
  char  buf[128];
  char  elem[32];

  l4printf_b("* dump mem <%s>\n", title);
  for(i=0; i<size; i+=16) {
    buf[0] = 0;
    for (j=i; j<i+16; j+=4) {
      if (j%4 == 0) _strcat(buf, " ");

      for(k=3; k>=0; k--) {
        c = start[j+k];
        l4snprintf(elem, 32, "%.2x", c);
        _strcat(buf, elem);
      }
    }

    _strcat(buf, "  ");
    for (j=i; j<i+16; j++) {
      c = start[j];
      if ( c >= ' ' && c < '~')
        l4snprintf(elem, 32, "%c", c);
      else
        l4snprintf(elem, 32, ".");
      _strcat(buf, elem);
    }
    l4printf_b("%s\n", buf);
  }
}





#if 0 //----------------------------------------------
int _ThreadContorl()
{
  L4_ThreadId_t dest;
  L4_ThreadId_t space;
  L4_ThreadId_t sched;
  L4_ThreadId_t pager;
  void*  utcb_location;

  L4_Msg_t  _MRs;
  L4_MsgTag_t  tag;

  L4_MsgStore(tag, &_MRs);
  dest.raw = L4_MsgWord(&_MRs, 1);
  space.raw = L4_MsgWord(&_MRs, 2);
  sched.raw = L4_MsgWord(&_MRs, 3);
  pager.raw = L4_MsgWord(&_MRs, 4);
  utcb_location = L4_MsgWord(&_MRs, 5);
  return L4_ThreadControl(dest, space, sched, pager, utcb_location);
}

//---------------------------------------------
int _SpaceControl( )
{
  L4_ThreadId_t space;
  L4_Word_t control;
  L4_Fpage_t KernelInterfacePageArea;
  L4_Fpage_t UtcbArea;
  L4_ThreadId_t redirector;
  L4_Word_t *old_control;

  L4_Msg_t  _MRs;
  L4_MsgTag_t  tag;

  L4_MsgStore(tag, &_MRs);
  space.raw = L4_MsgWord(&_MRs, 1);
  KernelInterfacePageArea.raw = L4_MsgWord(&_MRs, 2);
  UtcbArea.raw = L4_MsgWord(&_MRs, 3);
  redirector.raw = L4_MsgWord(&_MRs, 4);
  old_control = L4_MsgWord(&_MRs, 5);
  return L4_SpaceControl(space, control,  KernelInterfacePageArea,
			 UtcbArea, redirector, old_control);
}

//---------------------------------------
L4_Word_t _AssociateInterrupt ( )
{
  L4_ThreadId_t InterruptThread;
  L4_ThreadId_t HandlerThread;

  L4_Msg_t  _MRs;
  L4_MsgTag_t  tag;

  L4_MsgStore(tag, &_MRs);
  InterruptThread.raw = L4_MsgWord(&_MRs, 1);
  HandlerThread.raw = L4_MsgWord(&_MRs, 2);

  return L4_AssociateInterrupt(InterruptThread, HandlerThread);
}


L4_Word_t _DeassociateInterrupt ( )
{
  L4_ThreadId_t InterruptThread;

  L4_Msg_t  _MRs;
  L4_MsgTag_t  tag;

  L4_MsgStore(tag, &_MRs);
  InterruptThread.raw = L4_MsgWord(&_MRs, 1);

  return L4_DeassociateInterrupt(InterruptThread);
}
 
#endif //-----------------------------------------------------


#if 0 //-----------------------------------------------------
unsigned  alloc_page( )
{
  int  oldindex;
  unsigned  adrs;

  oldindex = index;
  while (memmap[index]) 
    {
      index++; 
      if (index == totalpages )  
	index = INDEX_16M;
      if (index == oldindex)  return 0;
    }
  memmap[index] = 1;
  adrs = ((index << 12) + MEMPOOL_ADRS);  // index*4096 + 2 Giga
  index++;
  //  l4printf("adrs=%x ", adrs); L4_KDB_Enter("< alloc_page"); 
  return  adrs;
}


unsigned  alloc_dma_page(int  pages )
{
  int  oldindex;
  unsigned  adrs;
  oldindex = indexdma;
  while (memmap[indexdma]) 
    {
      indexdma++; 
      if (indexdma == INDEX_16M )  
	indexdma = INDEX_1M;
      if (indexdma == oldindex)  return 0;
    }
  memmap[indexdma] = 1;
  adrs = ((indexdma << 12) + MEMPOOL_ADRS);  // index*4096 + 2 Giga
  indexdma++;

  //  l4printf("adrs=%x ", adrs); L4_KDB_Enter("< alloc_page"); 
  return  adrs;
}

#endif //------------------------------------------------
