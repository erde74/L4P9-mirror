#ifndef __L_ACTOBJ_H__
#define __L_ACTOBJ_H__
/*
 * [Receiver]
 *    - Thread
 *    - MsgBox:  asynchronous messaging.
 *    - ReplyBox: future message passing.
 * [Location]
 *    - Local (in the same process)
 *    - SameNode (in the same node)
 *    - RemoteNode (in the remote node)
 * [Messaging]
 *    (1) Sync. message
 *       - Inline data: Local, SameNode, RemoteNode
 *       - Outline data (buffer copy):  Local, SameNode
 *       - Page mapping:  Local, SameNode
 *       - Page copying: RemoteNode
 *    (2) Async. message
 *       - Inline data: Local, SameNode, RemoteNode
 *       - Outline data (buffer copy):  Local
 *       - Page mapping:  Not applicable
 *       - Page copying: RemoteNode
 *    (3) Future reply
 */

/* [1]  L_thcb type ---------------------------------------
 */

typedef         int     L_tidx;  //
typedef L4_MsgTag_t     L_msgtag;
typedef struct  L_mbuf  L_mbuf;
typedef struct  L_msgque  L_msgque;
typedef struct  L_replyto  L_replyto;
typedef struct  L_mbox     L_mbox;
typedef struct  L_replybox L_replybox;
typedef struct  L_thcb   L_thcb;

struct  L_msgque{
    L_mbuf  *head, *tail;
};

/* L4_ThreadId_t
 *    +-- 8---+---10--+-- 8--+--6--+
 *    | procx |  tidx | nodex| ver |    ver>=2
 *    +-------+-------+------+-----+
 */
#define MLABEL(msgtag)   ((msgtag).raw>>18)

#define  TID2PROCX(tid)   (((tid.raw)>>24) & 0xff)
#define  TID2TIDX(tid)    (((tid.raw)>>14) & 0x3ff)
#define  TID2NODEX(tid)   (((tid.raw)>>6) & 0xff)
#define  TID2VER(tid)     ((tid.raw) & 0x3f)
#define  TID2THCB(tid)    (L_thcbtbl[TID2TIDX(tid)])

#define  A2TIDX(a)        TID2TIDX(((L_thcb*)a)->tid)

#define  INF   (-1)  // Infinite time in send/recv

void  l_init();

struct L_mbox{
    L_mbuf   *mqhead, *mqtail;
    L_thcb   *tqhead, *tqtail;
    int  lock;
};

struct L_replybox{
    L_mbuf   *mqhead, *mqtail;
    L4_ThreadId_t  receiver;
    int  lock;
};


// Object: Thread | mbox | rbox;   thrad and mbox can coexist.
// Location: local | samenode | remotenode
struct   L_thcb{
    L_thcb        * next;
    L4_ThreadId_t   tid;
    L4_ThreadId_t   parent;
    unsigned int    isthread: 1;
    unsigned int    ismbox: 1;
  //    unsigned int    isrbox: 1;
    unsigned int    isproclocal: 1;
    unsigned int    islocalnode: 1;
    unsigned int    isremotenode: 1;
    unsigned int    isactobj:1;
    unsigned int          :2; 
    unsigned int    state:8;
    unsigned int         :16;
    char  * name;
    int     lock;
    L_mbox  mbox;      // Megbox for Async messages
    L_replybox replybox;
    union{
      struct{
	unsigned int    stkbase;
	unsigned int    stktop;
      } thd;
      struct{
      } proxy;
    };
};

extern L_thcb* L_thcbtbl[1024];

/*  L_thcbtbl
 *  +----------+
 *  |   [0]    | 			            L4_thread
 *  |   [1]    |      	  thcb 		       tid []----------+
 *  |   [2]  --|-------->+-----------+ 	  tid  	    |          |
 *  |    :     |      	 |  tid	     |------------->|	       |
 *  |    :     |      	 |  	     | 		    |	       |
 *  :    :     :      	 |-----------| 	  	    |	       |
 *  |    :     |      	 | inst-vars |<-------------|	       |
 *  |          |       	 +-----------+	UserDefined |	       |
 *  |          |      	ActiveObject	Handle	    +----------+
 *  +----------+
 *  [0]; main thread
 *  [1]: mediator thread
 *  [2..]: thread/mbox/replybox or proxy
 */

/* [2] L_mbuf type -------------------------------------------
 */

typedef union {
  L4_Word_t   raw;
  struct {
    L4_Word_t   u:6;
    L4_Word_t   t:6;
    L4_Word_t   flags:4;
    L4_Word_t   async:2 ; //0: Sync. 1: Async, 2: Areply
    L4_Word_t   mlabel:14;
  } X;
} L_MR0;

typedef union {
  L4_Word_t   raw;
  struct {
    L4_Word_t   ver:6; // Version
    L4_Word_t   nodex_msgptn:8;  // msgptn = nump<<7 + nums<<4 + numi
    L4_Word_t   tidx:10;
    L4_Word_t   procx:8 ; // Num. of int-args
  } X;
} L_MR1;

struct  L_mbuf {
    struct L_mbuf  * next;
    L4_ThreadId_t  sender;    // Source node local
    union {
        L4_Msg_t       MRs;
        struct{
	  L_MR0   mr0;
	  L_MR1   mr1;
	}X;
    };
};


/*      MRs   Ex. "i2s2"
 *      +-------------------+
 *      |     L_MR0         | 0
 *      +-------------------+
 *      |     L_MR1         | 1  Destination of asynchronous message
 *      +-------------------+
 *      |       e1          | 2
 *      +-------------------+
 *      |       e2          | 3
 *      +-------------------+
 *      |      size1        | 4
 *      +-------------------+
 *      |     str1          |
 *      |                   |
 *      +-------------------+
 *      |      size2        |
 *      +-------------------+
 *      |     str2          |
 *      |                   |
 *      +-------------------+
 */


/* [3] Thread/MsgBox Create and delete ----------------------------------------
 */

#define SAMEPROC(tid1, tid2)  (((tid1.raw ^ tid2.raw) & 0xFF000000) == 0)

L_thcb* l_thread_create(void (*func)(), int stksize, void *dataarea);
L_thcb* l_thread_create_args(void (*func)(), int stksize, void *dataarea,
			    int argc, ...);
L_thcb* l_mbox_create(char *name);
void    l_thread_setname(char *name);
void    l_yield(L_thcb *thcb);
L_msgtag  l_wait_ready(L_thcb*  thcb);
L_msgtag  l_post_ready(int  mlabel);

// L_tidx l_proxy_connect(char *name, int nodeid, L4_ThreadId_t mate);

int  l_thread_kill(L_thcb *thcb);
void l_thread_exits(char *exitstr);
void l_thread_killall();
void l_thread_exitsall(char *exitstr);
int  l_proxy_clear(L_thcb  *thcb);

void l_sleep_ms(unsigned  ms);

int  l_stkmargin0(unsigned int stklimit);
int  l_stkmargin() ;
int  l_myprocx();

void l_thcbtbl_dump();
void l_thcb_dump(L_thcb *thcb);


/* [4] Binding non-local objects ----------------------------------------
 */

L_thcb* l_thread_bind(char *name, L4_ThreadId_t mate, int attr);
int    l_thread_unbind(L_thcb* thcb);


/* [5] Active Object -----------------------------------------------
 *
 *   struct Actobj{
 *       L_thcb  _a;  //inherit L_thcb
 *       instance var...
 *   };
 *
 *   L4_Set_UserDefinedHandleOf(tid, aobj);
 *   L4_UserDefinedHandle();
 *
 */

// Get the object reference (Actobj *) from the current thread.
#define L_MYOBJ(type)   ((type)L4_UserDefinedHandle())

// Get the object reference (Actobj *) from the tid.
#define L_AOBJ(type, tid)  ((type)L4_UserDefinedHandleOf(tid))

/* Ex.
 *   typeded struct Alpha{
 *      L_thcb  _a;
 *      .....instance variables.....
 *      char    name[16];
 *      int     age;
 *      int     persoalId;
 *      ..........
 *   } Alpha;
 *
 *   void alpha(Alpha *self) 
 *   {    "Thread Body"    
 *        self->age = 20;  ... Instance var access example
 *        .......               
 *   }
 *
 *   Alpha * a1;
 *   a1 = (Alpha*)malloc(sizeof(Alpha));
 *   "Here, you can setup instance variables: a1->x = exp;"
 *
 *   l_create_thread(alpha, STKSIZE, a1);
 *        ....
 *   l_thread_kill(a1);
 *   free(a1);
 *
 *  +  "l_create_thread()" is used to create  pure-threads and active-objects.
 *  +  In case of active-object creataion, active object area must be passed
 *     explixitly by the third argument.
 *
 *   L4_ThreadId_t  tid = a1 -> _a.tid;
 *   a1 = L_AOBJ(Alpha*, tid);
 */


/* [6] L_mbuf/MRs set up  -----------------------------------
 *
 *  IF (mbuf == nil) THEN MRs in the Virtual Registers are setup.
 *  ELSE  mbuf is setup.
 */

L_mbuf* l_putarg(L_mbuf *mbuf, int mlabel, char *fmt, ...);
//Ex.  mbuf = l_putarg(mbuf, 747, "i2s2", e1, e2, buf1, len1, buf2, len2)

int  l_getarg(L_mbuf *mbuf, char *fmt, ...);
//Ex.  tag = l_getarg(mbuf, "i2s2", e1, e2, buf1, &len1, buf2, &len2)


/* [7] Timeout ---------------------------------------------
 */
L4_INLINE L4_Time_t msec2l4time(int  msec)
{
    L4_Time_t time;
    time.raw = 0;
    int  e = 10;
    if (msec <= -1)  return L4_Never;
    if (msec == 0)   return L4_ZeroTime;
    while (msec >= 1024) {
      e++; msec = msec/2;
    }
    time.period.e = e;
    time.period.m = msec;
    return  time;
}

L4_INLINE L4_Word_t l_timeouts (int sendmsec, int recvmsec)
{
    L4_Time_t  sendtimeout, recvtimeout;
    sendtimeout =  msec2l4time(sendmsec);
    recvtimeout =  msec2l4time(recvmsec);
    return (sendtimeout.raw << 16) + (recvtimeout.raw);
}


/* [8] Synchronous Messaging using L_mbuf ---------------------------------
 *   L_mbuf *mbuf; // is assumed
 */

L_msgtag  l_send0(L_thcb *thcb, int msec, L_mbuf *mbuf);

// Ex. mbuf = l_putarg(nil, mlabel, "i2s1", e1, e2, &buf, sizeof(buf));
//     msgtag = l_send0(thcb, INF, mbuf);

L_msgtag  l_recv0(L4_ThreadId_t *client, int msec, L_mbuf **mbuf);

// Notice: The first argument type is L4_ThreadId_t, not L_thcb*.
// Ex. msgtag = l_recv(&client, INF, nil);
//     n = l_getarg(nil, "i2s1", &x, &y, &buf2, &sz2);
// ? MR0 on ESI register might be explicitly handled..

L_msgtag  l_recvfrom0(L4_ThreadId_t client, int msec, L_mbuf **mbuf);

// Ex. msgtag = l_recvfrom(client, INF, nil);

L_msgtag  l_reply0(L4_ThreadId_t client, L_mbuf *mbuf);

// Ex. msgtag = l_reply0(client, mbuf);

L_msgtag  l_call0(L_thcb *thcb, int smsec, int rmsec, L_mbuf *mbuf);

// Ex. msgtag = l_call(thcb, INF, INF, mbuf);


/* [9] Asynchronous Messaging using L_mbuf ---------------------------------
 */

L_msgtag  l_asend0(L_thcb *dest, int msec, L_mbuf * mbuf);

//     Now, "msec" argument has no effect; 
//           merely for symmetry with "l_send()".
// Ex. mbuf = l_putarg(mbuf, mlabel, "i2s1", e1, e2, &buf, sizeof(buf));
//     msgtag = l_asend0(dest, 0, mbuf);

L_msgtag  l_arecv0(L_thcb  *mbox , int msec, L_mbuf **mbuf);

//  "msec" argument can be either INF (-1) or 0.
//         INF: wait for until message arrives.
//         0: No wait. If message has been received, it is returned.
// Ex. mbuf = l_arecv0(nil, INF, &mbuf); // receive from self mbox
//     mbuf = l_arecv0(mbox, INF, &mbuf); // receive from mbox
//     n = l_getarg(mbuf, "i2s1", &x, &y, &buf2, &sz2);

L_msgtag  l_areply0(L4_ThreadId_t client, L_mbuf * mbuf);

// Notice: The first argument type is L4_ThreadId_t, not L_thcb*.
// Ex. mbuf = l_putarg(mbuf, mlabel, "i1s1", e1, &buf, sizeof(buf));
//     n = l_asend0(client, mbuf);

int  l_arecvreply0(int rectime, L_mbuf **mbuf, int tnum, ...);

//  Wait for asynchronous reply with tally matching.
//       The L_MR0.mlabel field is assumed to have the tally value.
//       tnum: The number of tallies to be matched.
//             if (tnum==0) then the first reply is returned.
//       ...: tallies
//       Returen value(r): r-th tally is matched.
//
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


L_msgtag  l_acall0(L_thcb  *thcb, int recmsec, L_mbuf *mbuf);
// Ex. rc = l_acall0(dest, INF, INF, mbuf);


/* [10] Synchronous Messaging convenient function ------------------------
 */

// File-access-like convenient functions for client process.
// 'mlabel' is up to the server.
enum{LNULL, LOPEN, LREAD, LPREAD, LWRITE, LPWRITE};

int l_read(L_thcb *thcb, int fid, char *buf, int size);
int l_pread(L_thcb *thcb, int fid, char *buf, int size, unsigned int offset);

int l_write(L_thcb *thcb, int fid, char *buf, int size);
int l_pwrite(L_thcb *thcb, int fid, char *buf, int size, unsigned int offset);

int l_open(L_thcb *thcb, char *pathname, int attr1,  int attr2);


L_msgtag  l_send(L_thcb *thcb, int msec, int mlabel, char *fmt, ...);
// Ex. rc = l_send(tidx, INF, 747, "i2s1", i, j, &buf, sizeof(buf));

L_msgtag  l_recv(L_tidx *from, int msec, int mlabel, char *fmt, ...);
// Ex. rc = l_recv(&client, INF, 747, "i2s1", &x, &y, &buf2, &len2);

L_msgtag  l_recvfrom(L4_ThreadId_t client, int msec, int mlabel, char *fmt, ...);
// Ex. rc = l_recvfrom(client, INF, 747, "i2", &x, &y);

L_msgtag  l_call(L_thcb *thcb, int smsec, int rmsec, int mlabel, char *sfmt, char *rfmt, ...);
// Ex. rc = l_call(tidx, INF, INF, "i3", "i1", e1, e2, e3, &r);

L_msgtag  l_recv_mbuf(int msec, L_mbuf **from);
// Ex. L_mbuf *mbuf;
//     rc = l_arecv_str(INF, &mbuf);

L_msgtag  l_reply_mbuf(int msec, L_mbuf *mbuf);
// Ex. rc = l_areply_mbuf(0, mbuf);


/* [11] Asynchronous Messaging convenient function ---------------------
 */

L_msgtag  l_asend(L_thcb *thcb, int msec, int mlabel, char *fmt, ...);
// Ex. rc = l_asend(tidx, 0, 747, "i3", i, j, k);

L_mbuf * l_arecv(char *fmt,  ...);
// Ex. rc = l_arecv("i2", &x, &y);

L_msgtag  l_areply_mbuf(L_mbuf *mbuf, int msec);
// Ex. rc = l_areply_mbuf(mbuf, 0);

L_msgtag l_arecvfrom_mbuf(L_tidx from, L_mbuf **mbuf, int msec);
// Ex. L_mbuf *mbuf; 
//     rc = l_arecv_str(mboxid, &mbuf,  INF);

L_msgtag  l_acall_mbuf(L_mbuf *mbuf, int sndmsec, int recmsec);
// Ex. rc = l_acall_mbuf(mbuf, INF, INF);


/******************************************************
 *       helper functions                             *
 ******************************************************/

L_mbuf *l_allocmbuf();
void  l_freembuf(L_mbuf* mbuf);

void  l_lock(int *lk);
int   l_canlock(int *lk);
void  l_unlock(int *lk);

#endif /* __L_ACTOBJ_H__ */
