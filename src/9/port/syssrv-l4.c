//%%% SYSSRV-L4: system call manager %%%%
//% (C) KM

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../pc/io.h"
#include "../port/error.h"

#include "../port/systab.h"
//    #include "../../libc/9syscall/sys.h"

#include "../errhandler-l4.h"

#define   _DBGFLG  0    
#include  <l4/_dbgpr.h>
#define   PRN   l4printf_r


#define  _I  1
#define  _A  2
#define  _S  3
#define  _M  4

#define NUM_OF_CLERKS   24 


L4_ThreadId_t  SrvManager;   
Clerkjob_t     cjob;
   
static L4_ThreadId_t  client;
extern int nsyscall; 

extern  L4_ThreadId_t create_start_thread(unsigned pc, unsigned stkp);
extern  Proc * tid2pcb(L4_ThreadId_t tid);
extern  void dump_mem(char *title, unsigned char *start, unsigned size);
extern  void check_clock(int  which, char *msg);  
extern  void dbg_time(int which, char *msg);  //%
static  void clerk_thread( );


/*
 *  'up' is a current Proc pointer.  defined in pc/dat.h
 *  #define  up   (((Clerkjob_t*)L4_UserDefinedHandle())->pcb)
 */ 

/*
 *  typedef struct Clerkjob  Clerkjob_t;
 *  struct Clerkjob {
 *     unsigned       stk[...];
 *     unsigned       stktop[2];
 *     //- - - - - - - - -
 *     L4_Fpage_t    mappee; // mapped to
 *     L4_Fpage_t    mapper; // mapped from
 *     Clerkjob_t    *next;
 *     L4_ThreadId_t  tid;   // clerk thread
 *     L4_ThreadId_t  client;
 *     Proc          *pcb;
 *     unsigned       pattern; 
 *     ulong          args[8];
 *     char           strbuf[512];
 *  };
 *
 *      Clerkjob       clerkThread
 *     +---------+       []---------+
 *     | stk     |        |         |
 *     |         |        |         |
 *     |         |<-------+- UserDefinedHandle
 *     | tid  ---+------->|         |
 *     | client  |        |         |
 *     |         |        |         |
 *     | args    |        +---------+
 *     | strbuf  |       
 *     +---------+  1:1 bound
 */ 

//-----------------------------------------------
static Clerkjob_t  *cpoolhead = nil;
static Lock        cpoollock = { .key = 0};

static Clerkjob_t * allocclerk()
{
    Clerkjob_t  *cp;
    lock(&cpoollock);
    if (cpoolhead) {
        cp = cpoolhead;
        cpoolhead = cpoolhead->next;
    }
    else {
        cp = nil;
	L4_KDB_Enter("allocclerk:out-of-memeory");  //FIXME
    }
    unlock(&cpoollock);
    return  cp;
}

static void freeclerk(Clerkjob_t  *cp)
{
    lock(&cpoollock);
    if (cpoolhead) {
        cp->next = cpoolhead;
    }
    else {
        cp->next = nil;
    }
    cpoolhead = cp;
    unlock(&cpoollock);
}

//---------------------------------------
#define  MAPAREABASE  0x60000000    
#define  MEGA         0x200000  
// #define  MEGA         0x100000  

static int  clerknr = 0;
static  Clerkjob_t  clerkstk[NUM_OF_CLERKS];

static void clerkpool_setup(int num)
{
    L4_ThreadId_t  tid;
    Clerkjob_t   *cp;

    for (; clerknr<num; clerknr++) {
        //    cp = malloc(sizeof(Clerkjob_t));  
	//    if (cp == 0) break;
        cp = &clerkstk[clerknr];

	tid = create_start_thread((unsigned)clerk_thread, (unsigned)&cp->stktop );
//print("clerk_thread=%X    ", tid); L4_KDB_Enter("");  //%

	cp->tid = tid;
	cp->next = 0;
        cp->mappee  = L4_Fpage(MAPAREABASE + clerknr * MEGA, MEGA);
	L4_Set_UserDefinedHandleOf(tid, (uint)cp); //070807

	freeclerk(cp);
    }
}


//-----------------------------------------
void clerk_thread()
{
    L4_MsgTag_t    tag;
    L4_Msg_t       _MRs;
    int            syscallnr;
    int            rcode;
    vlong          seekpos;
    Clerkjob_t     *myjob;

//print("\n<><> CLERK THREAD=%X <><>\n", L4_Myself().raw); L4_KDB_Enter();

    while(1) {
        L4_MsgClear(&_MRs);
	tag = L4_Receive(SrvManager);  //L4_Wait(&manager);

	if (L4_IpcFailed(tag)){
	    l4printf_r("clerk-rec-err:<%x %x %d> \n", 
		       L4_Myself(), tag, L4_ErrorCode());
	    continue;
	}

	//	dbg_time(5, "clerk-thread-start");  //%

	L4_MsgStore(tag, &_MRs);
	syscallnr = L4_Label(tag);     // MR[0]

	myjob = (Clerkjob_t*)L4_UserDefinedHandle(); // .EQ. MR[1] 
	//or:   L4_StoreMR(1, (L4_Word_t*)&myjob);     // MR[1]

	DBGPRN(" %s: syscallnr=%d myjob=%x ptrn=%x\n", 
	       __FUNCTION__, syscallnr, myjob, myjob->pattern);

#if 0 //----- debugging dump -----------
	{
	    int   argx = 0;
	    uint  pattern, attr;
	    pattern = myjob->pattern;
	    while ((attr = (pattern & 0x0F))) {
	        switch (attr) {
		case _I:  // Immediate data param.
		case _A:  // Address param.
		  l4printf_g("ARG[%d] %d  \n", argx, myjob->args[argx]);
		  break;
		case _S:  // String param.
		  l4printf_g("ARG-s[%d] %s \n", argx,  myjob->args[argx]);
		}
		pattern = pattern >> 4;
		argx++;
	    }
	}
	//  dump_mem(__FUNCTION__, myjob->args, 64); 
#endif  //-------------------------------

        if (!WASERROR()) {
	    cjob.pcb = tid2pcb(myjob->client);
	    up = tid2pcb(myjob->client);

	    if (syscallnr >= nsyscall)
	        rcode = -1;  // EBADCALL;
	    else if (syscallnr == SEEK)
	        rcode = sysseek_l4(myjob->args, &seekpos, myjob);
	    else
	        rcode = (*systab[syscallnr])(myjob->args, myjob);

	    IF_ERR_GOTO(rcode, ONERR, _ERR_1);
                                                                        
	    POPERROR();
	    //  no reply for EXIT and WAIT 
	    //  if (syscallnr == EXEC && error == OK) continue;
	    //  reply(client, 1, 1);

	    if (syscallnr == EXITS) 
	        goto  FinLabel;
        }
        else {
	_ERR_1:
	    print("syssrv[%s<-%x]:'%s' '%s'\n", 
		  sysctab[syscallnr], myjob->client.raw, 
		  up->errstr, up->syserrstr);
	    rcode =  -1;
        }

	L4_Set_VirtualSender(SrvManager); 

        if (syscallnr == SEEK) {  // must return vlong valu
	    if (rcode == -1){
	        seekpos = -1;
	    }
	    L4_LoadMR(0, MSGTAG(IPC_FINISH, 1, 0, 2));  //MR0: propagation=1 
	    L4_LoadMR(1, (ulong)seekpos );        //MR1
	    L4_LoadMR(2, (ulong)(seekpos>>32) );  //MR2
        }
        else 
        if (syscallnr == PIPE) { 
	    L4_LoadMR(0, MSGTAG(IPC_FINISH, 1, 0, 3));  //MR0: propagation=1 
	    L4_LoadMR(1, rcode );          //MR1
	    L4_LoadMR(2, myjob->args[0]);  //MR2 = fd[0]
	    L4_LoadMR(3, myjob->args[1]);  //MR3 = fd[1]
        }
        else {
	    L4_LoadMR(0, MSGTAG(IPC_FINISH, 1, 0, 1));  //MR0: propagation=1  
	    L4_LoadMR(1, rcode);      //MR1: return code
        }

	//	dbg_time(5, "clerk-thread-end"); //%

	if ((rcode == -1) || 
	    (rcode == ONERR) || 
	    ((syscallnr != EXITS) && (syscallnr != EXEC)) ) {
	    tag = L4_Reply(myjob->client);
	    if (L4_IpcFailed(tag))  
	        l4printf_r("syssrv:Reply<%s> to %x failed\n", 
			   sysctab[syscallnr], myjob->client.raw);
	}

    FinLabel:
	if (!L4_IsNilFpage(myjob->mapper)) {
	    L4_Fpage_t  tobeunmapped;
	    tobeunmapped = L4_Fpage(L4_Address(myjob->mappee), L4_Size(myjob->mapper));
	    L4_FpageAddRights(tobeunmapped, L4_FullyAccessible);

	    //    L4_UnmapFpage(tobeunmapped);
	    myjob->mapper = L4_Nilpage;
	}
	freeclerk(myjob);
    }
}


//-------------------------------------------------
void service_loop()   // never return function
{
    L4_MsgTag_t    tag;
    L4_Msg_t       _MRs;
    L4_StringItem_t  simplestring;

    //    L4_MsgBuffer_t   _BRs;  // to specify receive string
    //    static  struct {
    //        uint   raw[3];
    //        struct {
    //	        uint    BR0;  // L4_Fpage_t.raw | 0x1 
    //	        L4_StringItem_t  stritem;  // 'C' bit must be 0
    //	      } X;
    //    } _BRdata;

    Clerkjob_t    *cp;
    int            syscallnr;
    uint           pattern, pp; //[ 0| 0| 0|p4|p3|p2|p1|p0]  
    int            inx, argx, attr, len, strx;
    uint           acceptor = 0x1;

    SrvManager = L4_Myself();
    DBGBRK("\n<><> SERVICE LOOP: %X <><>\n", SrvManager.raw); 
    if (SrvManager.raw != SRV_MANAGER) 
        L4_KDB_Enter("SRV_MANAGER unmatch");

    L4_Set_UserDefinedHandle((L4_Word_t)&cjob); // or in main-l4.c

    clerkpool_setup(NUM_OF_CLERKS);

    ack2hvm(0);  // I am ready, now.

    while (1) {
        cp = allocclerk();
	if (cp == nil) L4_KDB_Enter("out-of-clerks");

	DBGPRN(" service_loop cp:%x \n", cp);
	memset(cp->args, 0, sizeof(cp->args));
	memset(cp->strbuf, 0, sizeof(cp->strbuf));
 	simplestring = L4_StringItem(512, cp->strbuf);

	cp->mapper = L4_Nilpage;
	acceptor = cp->mappee.raw | 0x01;

#if 1 //-------------------------------------
	L4_LoadBR(0, acceptor);
	L4_LoadBR(1, simplestring.raw[0]);
	L4_LoadBR(2, simplestring.raw[1]);
	// OPTIMIZE: use L4_LoadBRs(0, 3, &_BRdata);
#else //------------------------------------
        L4_MsgBufferClear(&_BRs);
	L4_MsgBufferAppendSimpleRcvString(&_BRs, simplestring);
	L4_AcceptStrings(L4_StringItemsAcceptor, &_BRs);
#endif //----------------------------------

	cp->strbuf[0] = 0;
	inx = 1;
	argx = strx = 0;

	tag = L4_Wait(&client);

	if (L4_IpcFailed(tag)){
	    l4printf_r("syssrv-rec-err:<%x %x %d> \n", client, tag, L4_ErrorCode());
	    freeclerk(cp);
	    continue;
	}

	//	dbg_time(5, 0);  //%

	L4_MsgStore(tag, &_MRs);
	syscallnr = L4_Label(tag);      // MR[0]
	cp->client = client;
	// L4_Set_UserDefinedHandle((ulong)tid2pcb(client)); 

	pattern = _MRs.raw[inx++];      // MR[1]
	cp->pattern = pattern;

#if  0 
	for(pp = pattern; pp; pp = pp>>4)
	    if ((pp & 0x0F) == _M)   
	    { 
	        L4_MapItem_t  map;
		L4_Fpage_t    fpage;

		if (tag.X.t==2) {
		    map.raw[0] = _MRs.raw[tag.X.u+1];
		    map.raw[1] = _MRs.raw[tag.X.u+2];
		    if(L4_IsMapItem(map)) {
		        fpage = L4_MapItemSndFpage(map);
			cp->mapper = fpage;
		    }
		} 
		else if (tag.X.t==4) {
		      map.raw[0] = _MRs.raw[tag.X.u+3];
		      map.raw[1] = _MRs.raw[tag.X.u+4];
		      if(L4_IsMapItem(map)) {
			  fpage = L4_MapItemSndFpage(map);
			  cp->mapper = fpage;
		      }
		}

		if (0 && cp->mapper.raw && (L4_Label(tag)!=50)  ){
	            l4printf_b("map{%d}<%x %x %x %x> \n", syscallnr, 
			       L4_Address(cp->mapper), L4_Size(cp->mapper), 
			       map.raw[0], map.raw[1]);
		    L4_KDB_Enter("srvloop");  //%%
		}
	    }
#endif

	DBGPRN("\n<><>SVR Label= %d pattern= %X <><> \n", syscallnr, pattern);

	//	check_clock('s', "scall");  //%%%%

	while ((attr = (pattern & 0x0F))) {
	    switch (attr) {
	    case _I:  // Immediate data param.
	    case _A:  // Address param.
	        cp->args[argx] = _MRs.raw[inx];

		DBGPRN("SVR ARG[%d] %d  \n", argx, cp->args[argx]);
		break;

	    case _S:  // String param.
	        len = _MRs.raw[inx];

		if (len == 0)         // 080726
		    cp->args[argx] = 0;
		else
		  cp->args[argx] = (unsigned)&cp->strbuf[strx];
		DBGPRN("SVR ARG[%d] %s : %d \n", argx,  cp->args[argx], len);

		strx += len;
		break;

	    case _M:  //fpages map: must be the last arg.
#if 1 
	      //l4printf_g("<%d %d> ", syscallnr, tag.X.t); 
	      { 
		  L4_MapItem_t  map;
		  L4_Fpage_t      fpage;

		  if (tag.X.t==2) {
		      map.raw[0] = _MRs.raw[tag.X.u+1];
		      map.raw[1] = _MRs.raw[tag.X.u+2];
		  } 
		  else if(tag.X.t==4) {
		      map.raw[0] = _MRs.raw[tag.X.u+3];
		      map.raw[1] = _MRs.raw[tag.X.u+4];
		  }

		  if( L4_IsMapItem(map)) {
		      fpage = L4_MapItemSndFpage(map);
		      cp->mapper = fpage;
		  }

		  if (0 && cp->mapper.raw && (L4_Label(tag)!=50)  ){
		      l4printf_b("map{%d}<%x %x %x %x> \n", syscallnr, 
				 L4_Address(cp->mapper), L4_Size(cp->mapper), 
				 map.raw[0], map.raw[1]);
		      L4_KDB_Enter("srvloop");  //%%
		  }
	      }
#endif
	      break;
	      
	    } //switch
	    pattern = pattern >> 4;
	    inx++;  argx++;
	}

	up->nerrlab = 0;
	//  dump_mem(__FUNCTION__, cp->args, 64); //%%%

	L4_LoadMR(0, syscallnr << 16 | 1);
	L4_LoadMR(1, (L4_Word_t)cp);
	L4_Send(cp->tid);    // Distribute the job to a clerk thread.
    }
}


int  _stkmargin(Clerkjob_t *myjob)
{
    int  margin;
    register void *stkptr   asm("%esp");
    margin = (int)stkptr - (int)(&myjob->stk); 
    print("STK{top=0x%x stp=0x%x bottom=0x%x margin=%d} \n", 
	  (int)(&myjob->stktop), (int)stkptr, (int)(&myjob->stk),  margin);
    return  margin;
}
