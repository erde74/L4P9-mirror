/***************************************************************
 *        9p-like  protocol support                            *
 ***************************************************************/

/* [20] FUTURE tpe message  ------------------------
 *     	Replybox is used for receiving asynchronous reply
 *
 *     	Client 	       	       	       	      Servert
 *  []--------------+                     []--------------+
 *   |msgque ooo    |                      |msgque ooo	  |
 *   |replybox oo   |                      |replybox oo	  |
 *   | 	       	    |                      |     	  |
 *   | l_asend( )   | -------------------> |		  |
 *   |              |<------------------   | 	       	  |
 *   | 	       	    |tally;request-reply   |   	       	  |
 *   | l_arecvreply |  	   maching     	   |		  |
 *   | 		    | 	   Non-ordered	   |		  |
 *   +--------------+ 	      	  	   +--------------+
 *
 *   FUTURE-type message::
 *
 *  L_mbuf* l_putarg(L_mbuf *mbuf, int mlabel, char *fmt, ...);
 *  Ex.  mbuf = l_putarg(mbuf, 747, "i2s2", e1, e2, buf1, len1, buf2, len2)
 *
 *  int  l_getarg(L_mbuf *mbuf, char *fmt, ...);
 *  Ex.  tag = l_getarg(mbuf, "i2s2", e1, e2, buf1, &len1, buf2, &len2)
 * 
 *
 *  L_msgtag  l_asend0(L_thcb *dest, int msec, L_mbuf * mbuf);
 *  [Rule]    
 *   * The 1st argument of the message is a TALLY. 
 *   * TALLY is used to tie a request and a reply.
 *
 *  int  l_arecvreply0(int rectime, L_mbuf **mbuf, int tnum, ...);
 *  [Rule] 
 *   * The mlabel filed of the reply message contains TALLY.
 *        MR0 { TALLY | areply | flag | t | u } 
 *   * l_arecvreply0( ) (waits and) gets the reply message with TALLY.
 *   * If n-th TALLY is availabele,  l_arecvreply0( ) returns n.
 *   * If 'tnum' is zero, it returns the first reply. 
 *
 *
 *  L_msgtag  l_acall0(L_thcb  *dest, int recmsec, L_mbuf *mbuf);
 *    .EQ.    l_asend0(dest, 0, L_mbuf * mbuf);
 *            l_arecvreply0(rectime, L_mbuf **mbuf, 0);
 *
 *  Example
 *       L_mbuf *mbuf;
 *       l_putarg(mbuf, label, "i2s2", tally1, e2, buf1, len1, buf2, len2);
 *       l_asend(target, INF, mbuf);
 *       l_putarg(mbuf, label, "i2s1", tally2, e3, buf3, len3);
 *       l_asend(target, INF, mbuf);
 *          .....
 * 
 *       k = l_arecvreply(INF, &mbuf, 2, tally1, tally2);
 *       switch(k){
 *       case 1: l_getarg(mbuf, "i2", &x, &y);  // tally1
 *               .....
 *               break;
 *       case 2: l_getarg(mbuf, "i1s1", &z, &ss, &sz); // tally2
 *               ....
 *       }
 *
 */


/* [21] 9P message convenient function ------------------------
 *     	Reply box for receiving asynchronous reply
 *
 */

int l_aversion(L_thcb* dest, int tally, char * version, int sz);
size[4] Rversion tally[2] msize[4] version[s]

#define  l_tversion(mbuf,  tally, version, sz)	\
  l_putarg(mbuf, TVERSION, "i2s1", tally, version, sz) 

#define  l_rversion(mbuf, tally, msize, version, len) \
  l_getarg(mbuf, "i2s1", tally, msize, version, len) 


int l_aauth(L_thcb* dest,  int tally, int afid, char uname, int usz, char *aname, int asz);
size[4] Rauth tally[2] aqid[13]

size[4] Rerror tally[2] ename[s]

int l_aflush(L_thcb* dest,  int tally, iny oldtally);
size[4] Rflush tally[2]

int l_aattach(L_thcb* dest,  int tally, int fid, int afid,  char *uname, int ulen, char *aname, int alen);
size[4] Rattach tally[2] qid[13]

int l_awalk(L_thcb* dest,  int tally, int fid, int newfid, nwname[2] nwname*(wname[s])
size[4] Rwalk tally[2] nwqid[2] nwqid*(wqid[13])

int l_aopen(L_thcb* dest,  int tally, int fid, int mode);
size[4] Ropen tally[2] qid[13] iounit[4]

int l_acreate(L_thcb* dest,  int tally, int fid, int perm, int mode, char *name, int len);
size[4] Rcreate tally[2] qid[13] iounit[4]

int l_aread(L_thcb* dest,  int tally, int fid, int offset, int count);
size[4] Rread tally[2] count[4] data[count]

int l_awrite(L_thcb* dest,  int tally, int fid, int offset, int count, char * data, int size);
size[4] Rwrite tally[2] count[4]

int l_aclunk(L_thcb* dest,  int tally, int fid);
size[4] Rclunk tally[2]

int l_aremove(L_thcb* dest,  int tally, int fid);
size[4] Rremove tally[2]

int l_astat(L_thcb* dest,  int tally, int fid,
size[4] Rstat tally[2] stat[n]

int l_awstat(L_thcb* dest,  int tally, int fid, char *stat, int sz);
size[4] Rwstat tally[2]

/* [21] Convenient function ------------------------
 */

// File-access-like convenient functions for client process.
// 'mlabel' is up to the server.
enum{LNULL, LOPEN, LREAD, LPREAD, LWRITE, LPWRITE};

int l_read(L_thcb* dest, int fid, char *buf, int size);
int l_pread(L_thcb* dest, int fid, char *buf, int size, uint offset);

int l_write(L_thcb* dest, int fid, char *buf, int size);
int l_pwrite(L_thcb* dest, int fid, char *buf, int size, uint offset);

int l_open(L_thcb* dest, char *pathname, int attr1,  int attr2);

