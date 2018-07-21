/*****************************************************************
 *       Active Object library on L4 micro kernel                *
 *       (C) KM 2009                                             *
 *****************************************************************/

#include  <l4all.h>
#include  <lp49/lp49.h>
#define   _DBGFLG  0
#define   DBGPRN  if(_DBGFLG)print
#define   PRN   print

#include  <u.h>
#include  <libc.h>
#include  <lp49/lp49service.h>

#include  "l_actobj.h"

L_thcb* L_thcbtbl[1024];

/******************************************************
 *       helper functions                             *
 ******************************************************/
//----- l_lock -----------------
void
l_lock(int *lk)
{
    int i;
    if(!_tas((void*)lk))
        return;
    for(i=0; i<1000; i++){
      if(!_tas((void*)lk))
	  return;
	L4_Yield();    //%  sleep(0);
    }
    while(_tas((void*)lk))
      L4_Sleep(L4_TimePeriod(1000));  //% sleep(1000);
}

int
l_canlock(int *lk)
{
  if(_tas((void*)lk))
      return 0;
    return 1;
}

void
l_unlock(int *lk)
{
    *lk = 0;
}

//--- L_msgque -------------------------
void l_msgqinit(L_msgque *mq)
{
    mq->head = mq-> tail = nil;
}

void l_enqmsg(L_msgque *mq, L_mbuf* mbuf)
{
    mbuf->next = nil;
    if (mq->head == nil) mq->head = mq-> tail = mbuf;
    else {
        mq->tail->next = mbuf;
        mq->tail = mbuf;
    }
}

L_mbuf * l_deqmsg(L_msgque *mq)
{
    L_mbuf *mbuf;
    if (mq->head == nil)
        return  nil;
    mbuf = mq->head;
    mq->head = mq->head->next;
    return  mbuf;
}


/******************************************************
 *     Msgbox handling:   L_mbox                      *:
 ******************************************************/

void l_initmbox(L_mbox *mbox)
{
    mbox->mqhead = mbox->mqtail = nil;
    mbox->tqhead = mbox->tqtail = nil;
    mbox->lock = 0;
}

int l_enqpostmsg(L_mbox *mbox, L_mbuf *mbuf) 
{
    L_thcb  *thcb;
    L4_MsgTag_t mtag;

    mbuf->next = nil;
    l_lock(&mbox->lock);
    if (mbox->mqhead == nil)
        mbox->mqhead = mbox->mqtail = mbuf;
    else {
        mbox->mqtail->next = mbuf;
        mbox->mqtail = mbuf;
    }

    if (mbox->tqhead){
        thcb = mbox->tqhead;
	mbox->tqhead = thcb->next;
	l_unlock(&mbox->lock);
	//	DBGPRN("  %s-10(%x) \n", __FUNCTION__, thcb->tid);
	L4_LoadMR(0, 0);
	mtag = L4_Reply(thcb->tid);
	if (L4_IpcFailed(mtag)) 
	    print("Reply-err:%d \n", L4_ErrorCode());
    }
    else
        l_unlock(&mbox->lock);
    return 1;
}

L_mbuf * l_waitdeqmsg(L_mbox *mbox, int tm) // if(tm==0) nowait
{
    L_mbuf *mbuf;
    L_thcb *thcb;
    L4_ThreadId_t  from;
    L4_MsgTag_t mtag;

    thcb = TID2THCB(L4_Myself());
    //DBGPRN(">%s[thcb:%x me:%x mbox:%x]  ", __FUNCTION__, thcb, L4_Myself(), mbox);
    while(1){
        l_lock(&mbox->lock);
        if (mbox->mqhead) {
	    mbuf = mbox->mqhead;
	    mbox->mqhead = mbox->mqhead->next;
	    l_unlock(&mbox->lock);
	    //DBGPRN(" %s[mbox:%x mbuf:%x %d] ", __FUNCTION__, mbox, mbuf, i);
	    return  mbuf;
	}
	else {
	    if (tm == 0)  {
	        l_unlock(&mbox->lock);
		return  nil;
	    }
	    thcb->next = nil;
	    if (mbox->tqhead){
	      mbox->tqtail->next = thcb;
	      mbox->tqtail = thcb;
	    }
	    else 
	      mbox->tqhead = mbox->tqtail = thcb;
	
	    l_unlock(&mbox->lock);
	    //	DBGPRN("  %s-10[mbox:%x] \n", __FUNCTION__, mbox);
	    L4_LoadMR(0, 0); //
	    //  mtag = L4_Wait(&from);
	    mtag = L4_Wait_Timeout(msec2l4time(tm), &from);
	    if (L4_IpcFailed(mtag)){ 
	        if (L4_ErrorCode() & 0x2){
		    //print(" l_waitdeqmsg()timeout ");
		    mbox->tqhead = mbox->tqhead->next;
		    return  nil;
		}
		print("Reply-err:%d \n", L4_ErrorCode());
	    }
	    //DBGPRN("  %s:<%x %x>\n", __FUNCTION__, from, L4_GlobalIdOf(from));
	}
    }
}

/******************************************************
 *     Future message handling:  L_replybox           *:
 ******************************************************/

void l_initreplybox(L_replybox *rbox)
{
    l_lock(&rbox->lock);
    rbox->mqhead = rbox->mqtail= nil;
    rbox->receiver.raw = 0;
    rbox->lock = 0;
    l_unlock(&rbox->lock);
}


int l_putreply(L_replybox *rbox, L_mbuf *mbuf) // To be optimixed
{
    mbuf->next = nil;
    l_lock(&rbox->lock);
    if (rbox->mqhead == nil){
        rbox->mqhead = mbuf; 
        rbox->mqtail = mbuf;
    }
    else {
        rbox->mqtail->next = mbuf;
	rbox->mqtail = mbuf;
    }

    if(rbox->receiver.raw){
	l_unlock(&rbox->lock);
	L4_LoadMR(0, 0);
	L4_Reply(rbox->receiver);
	return  1;
    }
    else{
        l_unlock(&rbox->lock);
	return 1;
    }
}


int l_waitreply(L_replybox *rbox, int rectime, L_mbuf **mbuf, int tnum, int tally[])
{
    int  j;
    L_mbuf *mbuf1, *mbuf2, *mbuf3;
    L_MR0   mr0;
    L4_MsgTag_t msgtag;
    L4_ThreadId_t  from;
    L_thcb *self = TID2THCB(L4_Myself());

    l_lock(&rbox->lock);
    if (rectime == 0){
        if ((mbuf3 = self->replybox.mqhead)){
	    if (tnum == 0){ // any tally
	        self->replybox.mqhead = mbuf3->next;
		rbox->receiver.raw = 0;
		*mbuf = mbuf3;
		l_unlock(&rbox->lock);
		return  0;
	    }
	    else { // find matching tally 
	        mbuf1 = (L_mbuf*)&self->replybox.mqhead;
		mbuf2 = self->replybox.mqhead;
		while(mbuf2){
		    mr0.raw = mbuf2->MRs.raw[0];
		    for (j=0; j<tnum; j++){
		        if(mr0.X.mlabel == tally[j]){
			  mbuf1 = mbuf2->next;
			  rbox->receiver.raw = 0;
			  *mbuf = mbuf2;
			  l_unlock(&rbox->lock);
			  return j+1;
			}
		    }
		    mbuf2 = mbuf2->next;
		}
		l_unlock(&rbox->lock);
		return -1;
	    }
	}
	else{
	    l_unlock(&rbox->lock);
	    return -1;
	}
    }

    while(1){
        mbuf1 = (L_mbuf*)&self->replybox.mqhead;
	mbuf2 = self->replybox.mqhead;
	while(mbuf2){
	    mr0.raw = mbuf2->MRs.raw[0];
	    for (j=0; j<tnum; j++){
	        if(mr0.X.mlabel == tally[j]){
		    mbuf1 = mbuf2->next;
		    rbox->receiver.raw = 0;
		    *mbuf = mbuf2;
		    l_unlock(&rbox->lock);
		    return j+1;
		}
	    }
	    mbuf2 = mbuf2->next;
	}
	msgtag = L4_Wait(&from);
    }
}


/******************************************************
 *           Mediator thread                          *
 ******************************************************/

static void mediator_thread( )  // TBI: To be implemented.
{
    L_mbuf  *mbuf;
    L4_MsgTag_t  mtag;
    L4_ThreadId_t  from, to;
    int  toidx;
    L_thcb  *thcb;
    L_MR0  mr0;

    l_thread_setname("mediator-thread");
    for(;;){
        mbuf = l_allocmbuf();
        mtag = L4_Wait(&from);
	if (L4_IpcFailed(mtag)){
	  //	    ....
	}

	L4_StoreMR(0, &mr0.raw);

	if (mr0.X.async == 1) {
	    L4_MsgStore(mtag, &mbuf->MRs);
	    to.raw = mbuf->MRs.msg[1];  //MR[1]=to
	    toidx = TID2TIDX(to);
	    thcb = L_thcbtbl[toidx];
	    if (thcb->ismbox)
	        l_enqpostmsg(&thcb->mbox, mbuf);
	    else  /*err*/;
	}
	else if (mr0.X.async == 2) {
	    L4_MsgStore(mtag, &mbuf->MRs);
	    to.raw = mbuf->MRs.msg[1];  //MR[1]=to
	    toidx = TID2TIDX(to);
	    thcb = L_thcbtbl[toidx];
	    l_putreply(&thcb->replybox, mbuf);

	}
    }
}


/******************************************************
 *     L_thcb  management                           *
 ******************************************************/

int l_stkmargin0(unsigned int stkbase)
{
    register void *stkptr   asm("%esp");
    return  (int)stkptr - stkbase;
}

int  l_stkmargin()
{
    register void *stkptr   asm("%esp");
    L_thcb  * thcb = (L_thcb*)L4_UserDefinedHandle();
    return   (int)stkptr - thcb->thd.stkbase;
}

static void quietsleep()
{
    print(">>> quietsleep<%x> ...\n", L4_Myself());
    L4_Sleep(L4_Never);
}

//-------------------------------------
static struct {
    unsigned int   version ;
    unsigned int   utcb_size;
    unsigned int   utcb_base;
    void  *kip ;
    L4_ThreadId_t  proc_tid;
    L4_ThreadId_t  pager_tid;
    int   procx;

    unsigned int   thnum_base;  // procx<<10 + tidx
    int    thnum_max;
    int    thcnt;  // total number of threads
    char   used[1024];
    int    lock;
    L_mbuf *freembuf;
}  thdinfo ;

L_mbuf *l_allocmbuf()
{
    L_mbuf *mbuf;
    l_lock(&thdinfo.lock);
    if (thdinfo.freembuf){
        mbuf = thdinfo.freembuf;
	thdinfo.freembuf = mbuf->next;
	mbuf->next = nil;
    }
    else{
        mbuf = malloc(sizeof(L_mbuf));
        mbuf->next = nil;
    }
    l_unlock(&thdinfo.lock);
    return  mbuf;
}

void l_freembuf(L_mbuf *mbuf)
{
    l_lock(&thdinfo.lock);
    mbuf->next = thdinfo.freembuf;
    thdinfo.freembuf = mbuf;
    l_unlock(&thdinfo.lock);
}


void l_init() // FIXME: no-reentrant --> thread unsafe
{
    int  i;
    //    int  tidx;
    L_mbuf  *mp;
    L_thcb  *thcb;
    DBGPRN(">%s()\n", __FUNCTION__);
    if (thdinfo.kip != 0)   return;
    thdinfo.lock =  0;
    thdinfo.proc_tid =  L4_Myself();
    thdinfo.pager_tid = L4_Pager();
    thdinfo.procx = TID2PROCX(thdinfo.proc_tid);
    thdinfo.kip = L4_GetKernelInterface();
    thdinfo.utcb_size = L4_UtcbSize(thdinfo.kip);
    thdinfo.utcb_base = L4_MyLocalId().raw & ~(thdinfo.utcb_size - 1);
    thdinfo.thnum_base = L4_ThreadNo(thdinfo.proc_tid);
    thdinfo.version = L4_Version(thdinfo.proc_tid);
    thdinfo.thnum_max = 63; //%%%
    thdinfo.thcnt = 0;
    thdinfo.freembuf = nil;

    for (i=0; i<32; i++){
        mp = (L_mbuf*)malloc(sizeof (L_mbuf));
	if (mp == nil) break;
	mp->next = nil;
	l_freembuf(mp);  // Needs lock ...
    }

    thdinfo.used[0]= 1;  // main thread
    thdinfo.used[1]= 0;  // mediator thread
    thcb = l_thread_create(mediator_thread, 1000, 0);
    if (thcb == nil) print("mediator tidx is = %x,; must be 1\n", thcb);

    for (i=2; i<1024; i++)
        thdinfo.used[i] = 0;
    print("l_init::procx:%x tidx:%x kip:%x utcbbase:%x, utcbsize:%x\n",
	  thdinfo.thnum_base>>10, thdinfo.thnum_base&0x3FF, thdinfo.kip, 
	  thdinfo.utcb_base, thdinfo.utcb_size);
    //    L4_KDB_Enter("");
    DBGPRN("<%s()\n", __FUNCTION__);
}

// no-reentrant --> thread unsafe --> Lock must be applied.
static int assign_tid_and_utcb (L4_ThreadId_t *tid, L4_Word_t  *utcb)
{
    int  i;

    if (thdinfo.kip == 0) l_init();

    for (i = 0; i < thdinfo.thnum_max; i++) {
       if (thdinfo.used[i] == 0) {
          thdinfo.thcnt ++;
	  thdinfo.used[i] = 1;
	  *tid =  L4_GlobalId (thdinfo.thnum_base + i, thdinfo.version);
	  *utcb = thdinfo.utcb_base + thdinfo.utcb_size * i;
	  return 1;
       }
    }
    return  0;
}

// For tid of mbox and proxy
static int assign_tid (L4_ThreadId_t *tid)
{
    int  i;
    if (thdinfo.kip == 0)   l_init();

    for (i = 1023; i >512; i--) {
       if (thdinfo.used[i] == 0) {
          thdinfo.thcnt ++;
	  thdinfo.used[i] = 1;
	  *tid = L4_GlobalId (thdinfo.thnum_base + i, thdinfo.version);
	  return  1;
       }
    }
    return  0;
}

static void free_tid(L4_ThreadId_t tid)
{
    int  tidx;
    if (thdinfo.kip == 0)   l_init();

    tidx = TID2TIDX(tid);
    if (thdinfo.used[tidx]) {
        thdinfo.thcnt --;
	thdinfo.used[tidx] = 0;
    }
}

static unsigned int tid2utcb(L4_ThreadId_t tid)
{
    int  indx;
    indx = L4_ThreadNo(tid) - thdinfo.thnum_base;
    return  thdinfo.utcb_base + thdinfo.utcb_size * indx;
}

int l_myprocx()
{
  return TID2PROCX(thdinfo.proc_tid);
}

void l_thcbtbl_ls()
{
    int  i;
    L_thcb  *thcbp;
    char    *kind;
    print("---thcbtbl_ls---\n");
    print("[0] thread  'main' \n");

    for (i = 1; i<1024; i++)
        if (thdinfo.used[i] ){
	  thcbp = L_thcbtbl[i];
	  if (thcbp->isthread) kind = "thread";
	  else if (thcbp->ismbox) kind = "mbox";
	  else kind = "-";
				 
	  print("[%d] %s \t'%s' \t0x%x \t0x%x\n",
		i, kind, (thcbp->name)?thcbp->name:"-",
		thcbp->tid, thcbp);
	}
}

void l_thcb_ls(L_thcb *thcbp)
{
    int  i;
    char    *kind;
 
    if (thcbp->isthread) kind = "thread";
    else if (thcbp->ismbox) kind = "mbox";
    else kind = "-";
    
    print("[%d] %s \t'%s' \t0x%x \t0x%x\n",
	  TID2TIDX(thcbp->tid), kind, (thcbp->name)?thcbp->name:"-",
		thcbp->tid, thcbp);
}


//------------------------------------------------
static int l_HvmThreadControl(L4_ThreadId_t dest, L4_ThreadId_t space,
			      L4_ThreadId_t sched, L4_ThreadId_t pager, L4_Word_t utcb )
{
    L4_Msg_t    _MRs;
    L4_MsgTag_t  mtag;
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
    mtag = L4_Call(mx_pager);
    //  L4_MsgStore(mtag, &_MRs);
    return L4_Label(mtag);
}

//--------------------------------------------
L_thcb* l_thread_create(void (*func)(), int stksize, void *objarea)
{
    unsigned int          *sp;
    L4_ThreadId_t  tid;
    L_thcb      *thcb;
    L4_Word_t      utcb;
    int            rc, sz, tidx, isactobj;
    unsigned int   stkbase, stktop;
    DBGPRN(">l_thread_create(%x, %d, %x)\n", func, stksize, objarea);
    if (assign_tid_and_utcb(&tid, &utcb) != 1)
        return  nil;
    if(objarea) {
        isactobj = 0;
	thcb = objarea;
    }
    else{
        isactobj = 1;
	sz = sizeof(L_thcb);
	sz = (sz + 15) & 0xFFFFFFF0;
	thcb = (L_thcb*)malloc(sz);
	if (thcb == nil)   return  nil;
    }

    memset(thcb, 0, sizeof(L_thcb));
    thcb->isactobj = isactobj;
    thcb->parent = L4_Myself();  // Creator

    sz = (stksize + 15) & 0xFFFFFFF0;
    stkbase = (unsigned int)malloc(sz);
    stktop = (unsigned int)stkbase + (unsigned int)sz;
    thcb->thd.stkbase  = stkbase;
    sp = (unsigned int *)stktop;

    sz = 16; // thread name: max 16 bytes
    sp = (unsigned int*) ((unsigned int)sp - sz);
    *sp = 0;
    thcb->name = (char*)sp;

    {// copy an 'objarea' argument onto stack
      *(--sp) = 0; // No meaning.
      *(--sp) = (unsigned int)objarea;
    }

    *(--sp) = (unsigned int)quietsleep;  // 0; // Return address area

    tidx = TID2TIDX(tid);
    thcb->isthread = 1;
    thcb->ismbox = 1;
    thcb->isproclocal = 1;
    thcb->tid = tid;
    thcb->thd.stktop  = (unsigned int)sp;
    //    thcb->state = L4TH_ASIGND;
    L_thcbtbl[tidx] = thcb;

    rc = l_HvmThreadControl(tid, thdinfo.proc_tid, thdinfo.proc_tid,
			    thdinfo.pager_tid, utcb);
    if (rc ==0) {
        free(thcb);
	free((char*)stkbase);
	return  nil;
    }

    L4_Set_UserDefinedHandleOf(tid, (L4_Word_t)thcb);
    DBGPRN("thread created <%x> ", tid.raw);
    L4_Start_SpIp(tid, (L4_Word_t)sp, (L4_Word_t)func);
    l_yield(nil);
    return  thcb;
}

//--------------------------------------------
L_thcb* l_thread_create_args(void (*func)(),  int stksize, void *objarea,
			    int argc, ...)
{
    unsigned int          *sp;
    L4_ThreadId_t  tid;
    L_thcb      *thcb;
    L4_Word_t      utcb;
    int            rc, sz, tidx, isactobj;
    unsigned int   stkbase, stktop;

    DBGPRN(">l_thread_create_args(%x, %d, %x)\n", func, stksize, objarea);
    if (assign_tid_and_utcb(&tid, &utcb) != 1)
        return   nil;

    if(objarea) {
        isactobj = 0;
	thcb = objarea;
    }
    else{
        isactobj = 1;
	sz = sizeof(L_thcb);
	sz = (sz + 15) & 0xFFFFFFF0;
	thcb = (L_thcb*)malloc(sz);
	if (thcb == nil)   return  nil;
    }

    memset(thcb, 0, sizeof(L_thcb));
    thcb->isactobj = isactobj;
    thcb->parent = L4_Myself();  // Creator

    sz = (stksize + 15) & 0xFFFFFFF0;
    stkbase = (unsigned int)malloc(sz);
    stktop = (unsigned int)stkbase + (unsigned int)sz;
    sp = (unsigned int *)stktop;

    sz = 16; // max 16 bytes
    sp = (unsigned int*) ((unsigned int)sp - sz);
    *sp = 0;
    thcb->name = (char*)sp;

    {// copy arguments onto stack
        int *argp;
	int  i;
	argp = (int*)(&argc + 1);
	*(--sp) = 0; //
	for (i = argc-1; i >= 0; i--)
	  *(--sp) = argp[i];

	*(--sp) = (unsigned int)objarea; // The 1st argument is 'objarea'
    }
    *(--sp) = (unsigned int)quietsleep;  // 0; // Return address area

    tidx = TID2TIDX(tid);
    thcb->isthread = 1;
    thcb->ismbox = 1;
    thcb->isproclocal = 1;
    thcb->tid = tid;
    thcb->thd.stktop  = (unsigned int)sp;
    thcb->thd.stkbase  = stkbase;
    //    thcb->state = L4TH_ASIGND;
    L_thcbtbl[tidx] = thcb;

    rc = l_HvmThreadControl(tid, thdinfo.proc_tid, thdinfo.proc_tid,
			    thdinfo.pager_tid, utcb);
    if (rc ==0) {
        free(thcb);
	free((char*)stkbase);
	return  nil;
    }

    L4_Set_UserDefinedHandleOf(tid, (L4_Word_t)thcb);
    DBGPRN("thread created <%x> \n", tid.raw);
    L4_Start_SpIp(tid, (L4_Word_t)sp, (L4_Word_t)func);
    l_yield(nil);
    return  thcb;
}

//-------------------------------------------
void l_thread_setname(char *name)
{
    L_thcb  *thcb;
    thcb = L_MYOBJ(L_thcb*);
    strncpy(thcb->name, name, 15);
    thcb->name[15] = 0;
}

//--------------------------------------------
L_thcb* l_mbox_create(char *name)
{
    L4_ThreadId_t  tid;
    L_thcb       *thcb;
    int            tidx;
    int            size;
    char           *nmp;

    if (assign_tid(&tid) != 1)
        return  nil;

    size = (name)? strlen(name) + 1 : 0;
    size += sizeof(L_thcb);
    thcb = (L_thcb*)malloc(size);
    if (thcb == nil)   return  nil;
    memset(thcb, 0, size);

    tidx = TID2TIDX(tid);
    thcb->ismbox = 1;
    thcb->isproclocal = 1;
    thcb->tid = tid;
    if (name) {
        nmp = (char*)((int)thcb + sizeof(L_thcb));
        strcpy(nmp, name);
	thcb->name = nmp;
    }

    DBGPRN("MsgBox created <'%s' %x> ", name, tid.raw);
    L_thcbtbl[tidx] = thcb;
    return  thcb;
}

#if 1
L_msgtag  l_wait_ready(L_thcb  *thcb)
{
    L4_ThreadId_t  child;
    L_msgtag       mtag;

    if  (thcb) {
        child = thcb->tid;
	mtag = L4_Receive(child);
    }
    else {
        mtag = L4_Wait(&child);
    }
    return  mtag;
}

L_msgtag  l_post_ready(int  mlabel)
{
    L4_ThreadId_t  parent, me;
    L_thcb  *mythcb;
    L_msgtag  mtag;
    unsigned int  mr0;

    mr0 = mlabel << 18;
    me = L4_Myself();
    mythcb = TID2THCB(me);
    parent = mythcb->parent;
    L4_LoadMR(0, mr0);
    mtag = L4_Send(parent);
    return  mtag;
}
#endif

//-----------------------------------------------
int  l_thread_kill(L_thcb  *thcb) // TBI: To be implemented.
{
    L4_ThreadState_t  state;
    L_tidx         tidx;
    L4_ThreadId_t  to;

    tidx = TID2TIDX(thcb->tid);
    if (thdinfo.used[tidx])
    {
        if(thcb->isthread)
	{
            to = L_thcbtbl[tidx]->tid;
	    state = L4_AbortIpc_and_stop(to);
	    if (!L4_ThreadWasHalted(state))
	        DBGPRN("thread-killed:%d\n", state);

	    l_HvmThreadControl(to, L4_nilthread, L4_nilthread, L4_nilthread,
				  (L4_Word_t) -1);
	    thcb =  L_thcbtbl[tidx];

	    DBGPRN("thread deleted <'%s' %x> \n", (thcb->name)?(thcb->name):"", to.raw);
	    if (thcb->isactobj)
	        free(thcb);
	    free_tid(to);
	    return  1;
	}
	else if (thcb->ismbox){
	    free(L_thcbtbl[tidx]);
	    thdinfo.used[tidx] = 0;
	}
    }
    return  0;
}


void l_thread_exits(char *exitstr)
{
    int  utcb = -1;
    DBGPRN("l4_thread_exits: \n");
    L4_MsgTag_t  mtag;

    { // rc = requestThreadControl(tid, myself, myself, mypager, utcb);
	  L4_LoadMR(1, (L4_Word_t)thdinfo.proc_tid.raw);
	  L4_LoadMR(2, (L4_Word_t)L4_nilthread.raw);
	  L4_LoadMR(3, (L4_Word_t)thdinfo.proc_tid.raw);
	  L4_LoadMR(4, (L4_Word_t)thdinfo.proc_tid.raw);
	  L4_LoadMR(5, (L4_Word_t)utcb);
	  L4_LoadMR(0, THREAD_CONTROL<<16 | 5 );
	  mtag = L4_Call(thdinfo.pager_tid);
	  if (L4_IpcFailed(mtag)) return;
    }
    L4_Sleep(L4_Never);
}

void  l_thread_killall()
{
    int  i;
    L4_ThreadId_t  tid;

    for (i = 2; i<=thdinfo.thnum_max; i++)
        if ( thdinfo.used[i]) {
	    tid = L4_GlobalId (thdinfo.thnum_base + i, thdinfo.version);
	    l_thread_kill(L_thcbtbl[i]);
	}
}

void  l_thread_exitsall(char *exitstr)
{
    DBGPRN("thread_exitsall: %s\n", exitstr);
    L4_Sleep(L4_Never);
}

//-------------------------------------------------
void l_sleep_ms(unsigned  ms)
{
    L4_Word64_t  microsec;
    microsec = (L4_Word64_t)(ms * 1000);
    L4_Sleep(L4_TimePeriod(ms * 1000));
}

//-------------------------------------------------
void l_yield(L_thcb *thcb)
{
    L4_ThreadId_t  tid;
    if (thcb == nil) L4_Yield();

    tid = thcb->tid;
    L4_ThreadSwitch(tid);
}


/******************************************************
 *     L_mbuf/MRs handling                            *
 ******************************************************/
// Argument specifier:
//  "i" : 32 bits value. numi <= 9
//  "s" : byte-string    nums <= 7
//  "b" : buffer copy    numb <= 1
//Ex.  p = l_putarg(mbuf, 747, "i2s2b", 
//                  e1, e2, &data1, len1, &data2, len2, start, len)
L_mbuf* l_putarg(L_mbuf *mbuf, int mlabel, char *fmt, ...)
{
    va_list   ap;
    L_MR0     mr0;
    L_MR1     mr1;
    int   inx = 2;     //% MR1 is used for async message destination
    int   ival, i;
    int   start, len, wlen; // in bytes
    int  numi= 0, nums= 0, numb= 0;
    char ch;
    L4_StringItem_t stritem;

    va_start(ap, fmt);
    mr0.raw = 0;  mr0.X.mlabel = mlabel;
    mr1.raw = 0;

    for (i = 0; (ch=fmt[i++]); ) {
        if (ch == 'i'){
            ch = fmt[i++];
	    if (ch < '0' || '9'<ch) return (L_mbuf*)-1;
	    numi = ch - '0';
	}
	else  if (ch == 's'){
            ch = fmt[i++];
	    if (ch < '0' || '7'<ch) return (L_mbuf*)-1;
	    nums = ch - '0';
	}
	else if (ch == 'b'){
	    numb = 1;
	}
	else  return (L_mbuf*)-1;
    }

    if (mbuf) {
	for (i = 0; i < numi; i++) {
	    ival = va_arg(ap, int);
	    mbuf->MRs.raw[inx++] = ival;
	}

	for (i = 0; i < nums; i++) {
	    start = va_arg(ap, unsigned int);
	    len = va_arg(ap, int);
	    wlen = (len + 3)/4;
	    if (inx + wlen > 62) return (L_mbuf*)-1;
	    mbuf->MRs.raw[inx++] = len;
	    memcpy(& mbuf->MRs.raw[inx], (char*)start, len);
	    inx += wlen;
	}

	if (numb){
	    start = va_arg(ap, unsigned int);
	    len = va_arg(ap, int);
	    stritem = L4_StringItem(len, (void*)start);
	    mbuf->MRs.raw[inx] = stritem.raw[0];
	    mbuf->MRs.raw[inx+1] = stritem.raw[1];
	    mr0.X.t = 2; 
	}

	mr1.X.nodex_msgptn = (numb<<7) + (nums<<4) + numi; // will be used for checking
	mbuf->MRs.raw[1] = mr1.raw;
	mr0.X.u = inx - 1; 
	mbuf->MRs.raw[0] = mr0.raw;
    }
    else // MRs
    {
        L4_Word_t  *mr;
	mr =  (L4_Word_t*)__L4_X86_Utcb( );

	for (i = 0; i < numi; i++) {
	    ival = va_arg(ap, int);
	    *(mr + inx) = ival;
	    inx++;
	}

	for (i = 0; i < nums; i++) {
	    start = va_arg(ap, unsigned int);
	    len = va_arg(ap, int);
	    wlen = (len + 3)/4;
	    if (inx + wlen > 62) return (L_mbuf*)-1;
	    *(mr + inx) = len;
	    inx++;
	    memcpy((char*) (mr + inx), (char*)start, len);
	    inx += wlen;
	}

	if (numb){
	    start = va_arg(ap, unsigned int);
	    len = va_arg(ap, int);
	    stritem = L4_StringItem(len, (void*)start);
	    *(mr + inx) = stritem.raw[0];
	    *(mr + inx + 1) = stritem.raw[1];
	    mr0.X.t = 2; 
	}

	mr1.X.nodex_msgptn = (numb<<7) + (nums<<4) + numi; //
	*(mr+1) = mr1.raw;
	mr0.X.u = inx - 1; 
	*mr = mr0.raw;
    }
    va_end(ap);
    return  mbuf;
}


//Ex.  p = l_getarg(mbuf, "i2s2", e1, e2, &data1, &len1, &data2, &len2)
int  l_getarg(L_mbuf *mbuf, char *fmt, ...)
{
    va_list   ap;
    L_MR0     mr0;
    L_MR1     mr1;
    int   inx = 2;     //% MR1 is used for async message destination.
    int   *ival, i;
    unsigned int   *start;
    int   *len, len2, wlen; // in bytes
    int  numi = 0, nums = 0, numb = 0;
    char ch;
    L4_StringItem_t  stritem;

    va_start(ap, fmt);
    mr0.raw = 0; 
    mr1.raw = 0;  //* mr1.X.nodex_msgptn shall be used for checking.

    for (i = 0; (ch=fmt[i++]); ) {
        if (ch == 'i'){
            ch = fmt[i++];
	    if (ch < '0' || '9'<ch) return -1;
	    numi = ch - '0';
	}
	else  if (ch == 's'){
            ch = fmt[i++];
	    if (ch < '0' || '9'<ch) return -1;
	    nums = ch - '0';
	}
	else  if (ch == 'b'){
	  numb = 1;
        }
	else  return -1;
    }

    if (mbuf) {
	for (i = 0; i < numi; i++) {
	    ival = va_arg(ap, int*);
	    *ival = mbuf->MRs.raw[inx++];
	}

	for (i = 0; i < nums; i++) {
	    start = va_arg(ap, unsigned int*);
	    len = va_arg(ap, int*);
	    if (len == nil)  return -1;
	    wlen = (*len + 3)/4;
	    if (inx + wlen > 62) return -1;
	    len2 = mbuf->MRs.raw[inx++];
	    if (len2 > *len) len2 = *len;
	    *len = len2;
	    memcpy((char*)start,  & mbuf->MRs.raw[inx], len2);
	    inx += wlen;
	}
	if (numb){
	    start = va_arg(ap, unsigned int*);
	    len = va_arg(ap, int*);
	    if ((start == 0) || (len == 0))  return -1;
	    memcpy((char*)stritem.raw[0],  & mbuf->MRs.raw[inx], sizeof(stritem));
	    *start = (unsigned int)stritem.X.str.string_ptr;
	    *len = stritem.X.string_length;
	}

	return  mr0.X.mlabel;
    }
    else // MRs
    {
        L4_Word_t  *mr;
	mr =  (L4_Word_t*)__L4_X86_Utcb( );
	mr0.raw = *mr;
	for (i = 0; i < numi; i++) {
	    ival = va_arg(ap, int*);
	    *ival = *(mr + inx);
	    inx++;
	}

	for (i = 0; i < nums; i++) {
	    start = va_arg(ap, unsigned int*);
	    len = va_arg(ap, int*);
	    if (len == nil)  return -1;
	    wlen = (*len + 3)/4;
	    if (inx + wlen > 62) return -1;
	    len2 = *(mr + inx);
	    if (len2 > *len) len2 = *len;
	    *len = len2;
	    inx++;
	    memcpy((char*)start, (char*) (mr + inx), len2);
	    inx += wlen;
	}
	if (numb){
	    start = va_arg(ap, unsigned int*);
	    len = va_arg(ap, int*);
	    if ((start == 0) || (len == 0))  return -1;
	    memcpy((char*)stritem.raw[0],  (char*)(mr + inx), sizeof(stritem));
	    *start = (unsigned int)stritem.X.str.string_ptr;
	    *len = stritem.X.string_length;
	}


    }
    va_end(ap);
    return  mr0.X.mlabel;
}


/******************************************************
 *     Synchronous messaging                          *
 ******************************************************/

L_msgtag  l_send0(L_thcb *thcb, int msec, L_mbuf *mbuf)
{
    L4_ThreadId_t  dest;
    //    L_thcb   *thcb;
    L_msgtag   mtag;

    //    thcb = L_thcbtbl[tidx];
    // check....
    //    l_yield(nil);
    dest = thcb->tid;
    if (mbuf){
        int  argc = mbuf->MRs.raw[0] & 0x003F;
	L4_LoadMRs(0, argc, (L4_Word_t*)&mbuf->MRs);
    }

    mtag = L4_Send_Timeout(dest, msec2l4time(msec));
    if (L4_IpcFailed(mtag))
        return mtag;  // To be refined
    return  mtag;
}

L_msgtag  l_recv0(L4_ThreadId_t *client, int msec, L_mbuf **mp)
{
    int   u;
    L_msgtag  mtag;
    L4_ThreadId_t  from;

    // check....
    //    l_yield(nil);
    mtag = L4_Wait_Timeout(msec2l4time(msec), &from);
    if (L4_IpcFailed(mtag))
        return  mtag;
    u = mtag.X.u;
    if (client)
        *client = from;

    if (mp) {
        *mp = l_allocmbuf();
        L4_MsgStore(mtag, (L4_Msg_t*)(*mp));
    }

    return  mtag;
}

L_msgtag  l_recvfrom0(L4_ThreadId_t  client, int msec, L_mbuf **mbuf);
// Ex. rc = l_recvfrom(client, INF, &mbuf);

L_msgtag  l_call0(L_thcb *thcb, int smsec, int rmsec, L_mbuf *mbuf)
// Ex. rc = l_call(tid, INF, INF, &r, 2, j, k);
{
    L4_ThreadId_t  dest;
    L_msgtag   mtag;

    // check....
    //    l_yield(nil);
    dest = thcb->tid;
    if (mbuf){
        int  argc = mbuf->MRs.raw[0] & 0x003F;
	L4_LoadMRs(0, argc, (L4_Word_t*)&mbuf->MRs);
    }

    mtag = L4_Call_Timeouts(dest, msec2l4time(smsec), msec2l4time(rmsec));
    if (L4_IpcFailed(mtag))
        return mtag;  // To be refined
    
    if (mbuf){
        L4_MsgStore(mtag, (L4_Msg_t*)(mbuf));
    }
    return  mtag;
}


void l_set_recvbuf(int n, ... /* char *buf, int size */)
{
    int    i;
    int   *argp;
    void  *from;
    int    size;
    L4_StringItem_t  si;
    if (n <= 0 ) {
        L4_LoadBR(0, 0);
	return ;
    }
    if (n >= 16) {
        L4_LoadBR(0, 0);
	return ;
    }
    argp = &n;
    for (i = 1; i < n; ){
      from = (void*)*argp;
	size = *(argp + 1);
	si = L4_StringItem(size, from);
	L4_LoadBR(i++, si.raw[0]);
	L4_LoadBR(i++, si.raw[1]);
    }
}

L_msgtag  l_reply0(L4_ThreadId_t  dest, L_mbuf *mbuf)
{
    //    L_thcb   *thcb;
    L_msgtag   mtag;

    if (mbuf){
        int  argc = mbuf->MRs.raw[0] & 0x003F;
	L4_LoadMRs(0, argc, (L4_Word_t*)&mbuf->MRs);
    }

    mtag = L4_Reply(dest);
    if (L4_IpcFailed(mtag))
        return mtag;  // To be refined
    return  mtag;
}

L_msgtag  l_pull(L_tidx dest, int slabel, void *start, int size);

//------------------------------------------------------
int l_pread(L_thcb *thcb, int fid, char *buf, int size, unsigned int offset)
{
    L4_MsgTag_t  msgtag;
    struct{
      L_MR0  mr0;
      L_MR1  mr1;
      int    fid;
      int    size;
      unsigned int   offset;
    } mm;
    struct{
      int     br0;
      L4_StringItem_t  si;
    } bb;
    int  n = 0;

    mm.fid = fid;
    mm.size = size;
    mm.offset = offset;
    mm.mr0.raw = 0;
    mm.mr0.X.mlabel = LPREAD;
    mm.mr0.X.async = 0;
    mm.mr0.X.u = 5;
    mm.mr0.X.t = 3;
    bb.br0 = 1;
    bb.si = L4_StringItem(size, (void*)buf);
    L4_LoadBRs(0, 3, (L4_Word_t*)&bb);
    L4_LoadMRs(0, 5, (L4_Word_t*)&mm);

    msgtag = L4_Call(thcb->tid);
    //
    L4_StoreMR(1, (L4_Word_t*)&n);
    return  n;
}


int l_read(L_thcb *dest, int fid, char *buf, int size);

int l_write(L_thcb *dest, int fid, char *buf, int size);

int l_pwrite(L_thcb *dest, int fid, char *buf, int size, unsigned int offset)
{
    L4_MsgTag_t  msgtag;
    struct{
      L_MR0  mr0;
      L_MR1  mr1;
      int    fid;
      int    size;
      unsigned int   offset;
      L4_StringItem_t  si;
    } mm;
    int  n = 0;

    mm.fid = fid;
    mm.size = size;
    mm.offset = offset;
    mm.mr0.raw = 0;
    mm.mr0.X.mlabel = LPREAD;
    mm.mr0.X.async = 0;
    mm.mr0.X.u = 4;
    mm.mr0.X.t = 2;
    mm.si = L4_StringItem(size, (void*)buf);
    L4_LoadMRs(0, 5, (L4_Word_t*)&mm);

    msgtag = L4_Call(dest->tid);
    //
    L4_StoreMR(1, (L4_Word_t*)&n);
    return  n;
}

int l_open(L_thcb *thcb, char *pathname, int attr1,  int attr2);



/******************************************************
 *     Asynchronous Messaging using L_mbuf            *:
 ******************************************************/

L4_ThreadId_t MEDIATOR(L4_ThreadId_t tid)
{
    tid.raw = ((tid.raw & 0xFF003FFF) | (1 << 14));
    return tid;
}

L_msgtag  l_asend0(L_thcb *thcb, int msec, L_mbuf * mbuf)
{
    L4_ThreadId_t  mediator, dest;
    L_msgtag   mtag;
    L_thcb    *target_thcb;

    target_thcb = thcb;
    // check...
    //    l_yield(nil);
    if (! target_thcb->ismbox){
        mtag.raw = 0x8000;  // Err flag
        return  mtag;
    }
    dest = target_thcb->tid;
    //    DBGPRN(">%s(%d:%x)  ", __FUNCTION__, tidx, dest);

    if (target_thcb->isproclocal){
        if (mbuf == nil){
	    mbuf = l_allocmbuf();
	    mtag = L4_MsgTag();
	    L4_MsgStore(mtag, &mbuf->MRs);
	}

	mbuf->MRs.msg[1] = dest.raw; // MR[1]
	l_enqpostmsg(&target_thcb->mbox, mbuf);
	mtag.raw = mbuf->MRs.msg[0];
	l_yield(thcb);
	return  mtag;
    }
    else{
        mediator = MEDIATOR(dest);
	if (mbuf) {
	    int  argc = mbuf->MRs.raw[0] & 0x003F;
	    mbuf->MRs.msg[1] = dest.raw; // MR[1]s
	    L4_LoadMRs(0, argc, (L4_Word_t*)&mbuf->MRs);
	}
	mtag = L4_Send_Timeout(mediator, msec2l4time(msec));
	if (L4_IpcFailed(mtag))
	  return mtag;
    }
    return  mtag;
}


// 'msec==0' meens nodelay.
L_msgtag  l_arecv0(L_thcb *mboxid, int msec, L_mbuf **mp)
{
    L_thcb   *mboxthcb;
    L_mbuf   *mbuf;
    L_mbox   *mbox;
    L_msgtag  mtag;

    if (mboxid)  // Separate MBOX
        mboxthcb = mboxid;
    else         // Self MBOX
        mboxthcb = TID2THCB(L4_Myself());
    //print("NAME:%x:%s  \n", mboxthcb, mboxthcb->name);

    if (!mboxthcb->ismbox) { //TO BE REFINED
        mtag.raw = -1;
        return   mtag;  
    }

    mbox = & mboxthcb->mbox; 
    //DBGPRN("  %s{mbox:%x thcb:%x}\n", __FUNCTION__, mbox, thcb);
    mbuf = l_waitdeqmsg(mbox, msec);
    if (mbuf)			// HK 20091231
	    mtag.raw = mbuf->MRs.msg[0];	// HK 20091231
    else
            mtag.raw = 0;
    *mp = mbuf;
    return mtag;
}


L_msgtag  l_arecvfrom0(L4_ThreadId_t from, int msec, L_mbuf **mbuf);
// Ex. rc = l_arecv(mboxid, INF, &mbuf);

L_msgtag  l_acall_mbuf0(L_thcb *dest, int sndmsec, int recmsec, L_mbuf *mbuf);
// Ex. rc = l_acall_mbuf(dest, INF, INF, mbuf);


/***************************************************************
 * Asynchronous reply = future-type message                    *
 ***************************************************************/

// Ex. mbuf = l_putarg(mbuf, mlabel, "i1s1", e1, &buf, sizeof(buf));
//     n = l_areply0(client, mbuf);
L_msgtag  l_areply0(L4_ThreadId_t dest, L_mbuf * mbuf)
{
    L4_ThreadId_t  mediator;
    L_msgtag   mtag;
    L_replybox  *rbox;
    int       argc;

    //    DBGPRN(">%s(%d:%x)  ", __FUNCTION__, tidx, dest);
    //    l_yield(nil);
    mbuf->X.mr0.X.async = 2;  // Async reply
    if (TID2NODEX(dest) == 0){
        if (TID2PROCX(dest) == TID2PROCX(thdinfo.proc_tid)){
	    rbox = & L_thcbtbl[TID2TIDX(dest)]->replybox;
	    l_putreply(rbox, mbuf);
	    mtag.raw = 0;
	}
	else {
            mediator = MEDIATOR(dest);	
	    argc = mbuf->MRs.raw[0] & 0x003F;
	    mbuf->MRs.msg[1] = dest.raw; // MR[1]s
	    L4_LoadMRs(0, argc, (L4_Word_t*)&mbuf->MRs);

	    mtag = L4_Send(mediator);
	    if (L4_IpcFailed(mtag))
	       return mtag;
	}
    }
    return  mtag;
}


/*  Example
 *       L_mbuf *mbuf;
 *       l_putarg(mbuf, label, "i2s2", tally1, e2, buf1, len1, buf2, len2);
 *       l_asend(target, INF, mbuf);
 *       l_putarg(mbuf, label, "i2s1", tally2, e3, buf3, len3);
 *       l_asend(target, INF, mbuf);
 *          .....
 *       k = l_arecvreply(INF, &mbuf, 2, tally1, tally2);
 *       switch(k){
 *       case 1: l_getarg(mbuf, "i2", &x, &y);  // tally1
 *               .....
 *               break;
 *       case 2: l_getarg(mbuf, "i1s1", &z, &ss, &sz); // tally2
 *               ....
 *       }
 */
int  l_arecvreply0(int rectime, L_mbuf **mbuf, int tnum, ...)
{
    int   tally[32];
    int  *pp = &tnum;
    int   i;
    L_mbuf  *mbuf3;
    L_thcb *self = TID2THCB(L4_Myself());

    //    l_yield(nil);
    if (rectime == 0){
        if ((mbuf3 = self->replybox.mqhead)){
	    self->replybox.mqhead = mbuf3->next;
	    *mbuf = mbuf3;
	    return  0;
	}
	else
	    return -1;
    }

    for (i = 0; i<tnum; i++)
        tally[i] = pp[i+1];

    return l_waitreply(&self->replybox, rectime, mbuf, tnum, tally);
}



#if 0 //======================================================
/******************************************************
 *     l_actobj Synchronous message example           *
 ******************************************************/

/*********************************************************************
 *      main-thread       alpha1            beta1     	   gamma1
 *    []---------+      []---------+       []--------+     []--------+
 *     |         |       |         |        |        |      |        |
 *     |       --------->|         |        |        |      |	     |
 *     |         | "i2"  |        --------->|        |      |	     |
 *     |         |       |         | "i2s1" |       ------->|	     |
 *     |         |       |         |        |        |"i2s1"|	     |
 *     +---------+       +---------+        +--------+      +--------+
 *********************************************************************/

#define MILISEC 1000
#define STKSIZE 1000

char  *txt[4][3] =
  { {0, 0, 0},
    {0, "Iro Ha Nihoedo", "Chiri Nuru Wo"},
    {0, "abcdefgh", "ijklmnop"},
    {0, "To be or not to be", "that is a question"},
  };

typedef struct Alpha1{
    L_thcb  _a;
    int    u, v;
    char   name[16];
} Alpha1;

typedef struct Beta1{
    L_thcb  _a;
    int   u, v;
    char  name[16];
} Beta1;

Alpha1  *alpha1_obj;
Beta1   *beta1_obj;
L_thcb  *gamma1_obj;

void alpha1(Alpha1 *self, void *arg1, void *arg2)
{
    int  x, y;
    L4_ThreadId_t  src;
    //    Alpha1  *self;
    L_msgtag  msgtag;
    int       mlabel;

    l_thread_setname("alpha1");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    self = L_MYOBJ(Alpha1*);
    strcpy(self->name, "Alpha1");
    self->u = 333; self->v = 555;
    print("---- alphaS(%d %d)  [%x] ----\n",
	  (int)arg1, (int)arg2, L4_Myself());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);
    for(;;){
        msgtag = l_recv0(&src, INF, nil);
	mlabel = MLABEL(msgtag);
	l_getarg(0, "i2", &x, &y);
        print("alphaS-[%d]<%d %d>\n",mlabel, x, y);
	l_putarg(0, mlabel, "i2s1", x*10, y*10, txt[x][y], strlen(txt[x][y]));
	l_send0((L_thcb*)beta1_obj, INF, 0);
    }
}

void beta1(Beta1 *self, void *arg1, void *arg2)
{
    char  buf[64];
    int   u, v, len;
    L4_ThreadId_t  src;
    //    Beta1  *self = L_MYOBJ(Beta1*);
    L_msgtag  msgtag;
    int        mlabel;

    l_thread_setname("beta1");
    strcpy(self->name, "beta1");
    self->u = 777;
    self->v = 888;

    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    print("---- betaS-(%d %d)  [%x]<%d> ----\n",
	  (int)arg1, (int)arg2, L4_Myself(), l_stkmargin());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);
    for(;;){
        msgtag = l_recv0(&src, INF, nil);
	mlabel = MLABEL(msgtag);
	len = 64;
	l_getarg(0, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("  betaS-[%d]{%d %d '%s'}    \n", mlabel, u, v, buf);
	l_putarg(0, mlabel, "i2s1", u*10, v*10, buf, len);
	l_send0(gamma1_obj, INF, 0);
    }
}

void gamma1(void *self, int x, int y, int z )
{
    char  buf[64];
    int   u, v, len;
    L4_ThreadId_t  src;
    L_msgtag  msgtag;
    int       mlabel;

    print("---- gammaS-(%d %d %d) [%x] ----\n", x, y, z, L4_Myself().raw);
    l_thread_setname("gamma1");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    for(;;){
        msgtag = l_recv0(&src, INF, nil);
	mlabel = MLABEL(msgtag);
	len = 64;
	l_getarg(0, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("    gammaS-[%d]{%d %d '%s'}\n", mlabel, u, v, buf);
    }
    L4_Sleep(L4_TimePeriod(200*MILISEC));
}

//------------------------------------
typedef struct Alpha2  Alpha2;
typedef struct Beta2   Beta2;

struct Alpha2{
    L_thcb  _a;
    int    u, v;
    char   name[16];
};

struct Beta2{
    L_thcb  _a;
    int   u, v;
    char  name[16];
};

Alpha2  *alpha2_obj;
Beta2   *beta2_obj;
L_thcb  *gamma2_obj;


void alpha2(Alpha2 *self, void *arg1, void *arg2)
{
    int  x, y;
    L_mbuf *mbuf;
    //    Alpha2  *self;
    L_MR0  mr0;
    int    mlabel;

    l_thread_setname("alpha2");
    L4_LoadMR(0, 0);
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    self = L_MYOBJ(Alpha2*);
    strcpy(self->name, "Alpha2");
    self->u = 333; self->v = 555;
    print("\n---- alphaA(%d %d)  [%x] ----\n",
	  (int)arg1, (int)arg2, L4_Myself());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);

    for(;;){
        l_arecv0(nil, INF, &mbuf);
	if (mbuf==nil) print("mbuf-nil\n");
	mr0.raw = mbuf->MRs.msg[0];
	mlabel = mr0.X.mlabel;
	l_getarg(mbuf, "i2", &x, &y);
        print("alphaA-[%d]<%d %d>\n", mlabel, x, y);
	l_putarg(mbuf, mlabel, "i2s1", x*10, y*10, txt[x][y], strlen(txt[x][y]));
	l_asend0((L_thcb*)beta2_obj, INF, mbuf);
    }
}

void beta2(Beta2 *self, void *arg1, void *arg2)
{
    char  buf[64];
    int   u, v, len;
    //    Beta2  *self = L_MYOBJ(Beta2*);
    L_mbuf  *mbuf;
    L_MR0  mr0;
    int    mlabel;

    l_thread_setname("beta2");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    strcpy(self->name, "beta2");
    self->u = 777;
    self->v = 888;
    print("\n---- betaA(%d %d)  [%x]<%d> ----\n",
	  (int)arg1, (int)arg2, L4_Myself(), l_stkmargin());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);

    for(;;){
	len = 64;
        L4_LoadMR(0, 0);
        l_arecv0(nil, INF, &mbuf);
	mr0.raw = mbuf->MRs.raw[0];
	mlabel = mr0.X.mlabel;
	l_getarg(mbuf, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("  betaA-[%d]{%d %d '%s'}    \n", mlabel, u, v, buf);
	l_putarg(mbuf, mlabel, "i2s1", u*10, v*10, buf, len);
	l_asend0(gamma2_obj, INF, mbuf);
    }
}

void gamma2(void *self, int x, int y, int z )
{
    char  buf[64];
    int   u, v, len;
    L_mbuf  *mbuf;
    L_MR0  mr0;
    int    mlabel;

    l_thread_setname("gamma2");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    print("---- gammaA(%d %d %d) [%x] ----\n", x, y, z, L4_Myself().raw);
    L4_LoadMR(0, 0);

    for(;;){
	len = 64;
        l_arecv0(nil, INF, &mbuf);
	mr0.raw = mbuf->MRs.raw[0];
	mlabel = mr0.X.mlabel;
	l_getarg(mbuf, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("    gammaA-[%d]{%d %d '%s'}\n", mlabel, u, v, buf);
	l_freembuf(mbuf);
    }
    L4_Sleep(L4_TimePeriod(200*MILISEC));
}


int main(int argc, char **argv)
{
    int  i, j;
    L_mbuf  *mbuf;
    int  which = 1;

    l_init();
    //    print("< %d %s %s>\n", argc, argv[0], argv[1]); 
    if (argc>=2){
      if (strcmp(argv[1], "s")==0) which = 1;
      else if (strcmp(argv[1], "a")==0) which = 2;
    }
    L4_Sleep(L4_TimePeriod(5*MILISEC));

    if (which == 1){
        print("<><><> Synchronous messaging [%X] <><><> \n", L4_Myself());
        gamma1_obj = l_thread_create_args(gamma1, 
					  STKSIZE, nil, 3, 111, 222, 333);
	beta1_obj = (Beta1*)malloc(sizeof(Beta1));
	beta1_obj  = (Beta1*)l_thread_create_args(beta1, 
					  STKSIZE, beta1_obj, 2, 30, 40 );
	alpha1_obj = (Alpha1*)malloc(sizeof(Alpha1));
	alpha1_obj = (Alpha1*)l_thread_create_args(alpha1, 
					  2000, alpha1_obj, 2, 10, 20);
	L4_Sleep(L4_TimePeriod(10*MILISEC));
        for (i = 1; i<4; i++) {
	    for (j=1; j<3; j++) {
	        print("<%d %d>  ", i, j);
	        l_putarg(0, 10*i+j, "i2", i, j);
		l_send0((L_thcb*)alpha1_obj, INF, 0);
		//L4_Sleep(L4_TimePeriod(500*MILISEC));
	    }
	    //L4_Sleep(L4_TimePeriod(500*MILISEC));
	}
    }
    else{
        print("<><><> Asynchronous messaging [%X] <><><> \n", L4_Myself());
        gamma2_obj = l_thread_create_args(gamma2, 
					  STKSIZE, nil, 3, 111, 222, 333);
	beta2_obj = (Beta2*)malloc(sizeof(Beta2));
	beta2_obj  = (Beta2*)l_thread_create_args(beta2, 
					  STKSIZE, beta2_obj, 2, 30, 40 );
	alpha2_obj = (Alpha2*)malloc(sizeof(Alpha2));
	alpha2_obj = (Alpha2*)l_thread_create_args(alpha2, 
					  STKSIZE, alpha2_obj, 2, 10, 20);
	L4_Sleep(L4_TimePeriod(10*MILISEC));
	for (i = 1; i<4; i++) {
	    for (j=1; j<3; j++) {
	      print("<%d %d>  ", i, j);
	      mbuf = l_allocmbuf();
	      l_putarg(mbuf, 10*i+j, "i2", i, j);
	      l_asend0((L_thcb*)alpha2_obj, INF, mbuf);
	      //L4_Sleep(L4_TimePeriod(500*MILISEC));
	    }
	    //L4_Sleep(L4_TimePeriod(500*MILISEC));
	  }
      }
    //    L4_KDB_Enter("");
    L4_Sleep(L4_TimePeriod(50*MILISEC));
    l_thread_killall();
    print("<><><>  Bye-bye [%X] <><><> \n", L4_Myself());
    exits(0); 
    return  0;
}

#endif //===============================================
