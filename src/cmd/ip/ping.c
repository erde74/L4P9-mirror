//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%  km@nii

#include <u.h>
#include <libc.h>
#include <ip.h>

//%---------------------------------------------
#include  <l4all.h>
#include  <l4/l4io.h>

void  SET(void* x, ...) {  }
void  USED(void* x, ...) {  }

extern int l4printf_b(const char *fmt, ...);
extern char l4printgetc(const char *fmt, ...);

#define  PRN  l4printf_b
#define  BRK  l4printgetc
//%--------------------------------------------

typedef struct Icmp Icmp;
struct Icmp
{
	uchar	vihl;		/* Version and header length */
	uchar	tos;		/* Type of service */
	uchar	length[2];	/* packet length */
	uchar	id[2];		/* Identification */
	uchar	frag[2];	/* Fragment information */
	uchar	ttl;		/* Time to live */
	uchar	proto;		/* Protocol */
	uchar	ipcksum[2];	/* Header checksum */
	uchar	src[4];		/* Ip source */
	uchar	dst[4];		/* Ip destination */
	uchar	type;
	uchar	code;
	uchar	cksum[2];
	uchar	icmpid[2];
	uchar	seq[2];
	uchar	data[1];
};

enum
{			/* Packet Types */
	EchoReply	= 0,
	Unreachable	= 3,
	SrcQuench	= 4,
	EchoRequest	= 8,
	TimeExceed	= 11,
	Timestamp	= 13,
	TimestampReply	= 14,
	InfoRequest	= 15,
	InfoReply	= 16,

	ICMP_IPSIZE	= 20,
	ICMP_HDRSIZE	= 8,

	MAXMSG		= 8,    //% 32,
	SLEEPMS		= 1000,
};

typedef struct Req Req;
struct Req
{
	ushort	seq;	// sequence number
	vlong	time;	// time sent
	vlong	rtt;
	int	ttl;
	int	replied;
	Req	 *next;
};
Req	*first;		// request list
Req	*last;		// ...
Lock	listlock;

char *argv0;
int debug;
int quiet;
int lostonly;
int lostmsgs; 
int rcvdmsgs;
int done;
int rint;
vlong sum;
ushort firstseq;
int addresses;
int flood;

void usage(void);
void lost(Req*, Icmp*);
void reply(Req*, Icmp*);

#if 1 //%---------------------------------------------
static vlong usec()
{
    vlong microsec  =  L4_SystemClock().raw; 
    return microsec;
}

#define SECOND 1000000LL

ulong v2ulong(vlong v)
{
    union{vlong v; ulong u[2];} uv ;
    uv.v = v;
    if (uv.u[1]) return 0xffffffff;
    return  uv.u[0];
}

#else //plan9----------------------------------------
  #define SECOND 1000000000LL
#endif //%--------------------------------------------

#define MINUTE (60LL*SECOND)

static void
catch(void *a, char *msg)
{
        //%	USED(a);
	if(strstr(msg, "alarm"))
		noted(NCONT);
	else if(strstr(msg, "die"))
		exits("errors");
	else
		noted(NDFLT);
}

void
clean(ushort seq, vlong now, Icmp *ip)
{
	Req **l, *r;

	lock(&listlock);
	last = nil;
	for(l = &first; *l; ){
		r = *l;

		if(ip && r->seq == seq){
			r->rtt = now-r->time;
			//print("now=%d  r->time=%d\n", v2ulong(now), v2ulong(r->time)); //%
			r->ttl = ip->ttl;
			reply(r, ip);
		}

		if(now-r->time > MINUTE){
			*l = r->next;
			r->rtt = now-r->time;
			if(ip)
				r->ttl = ip->ttl;
			if(r->replied == 0)
				lost(r, ip);
			free(r);
		}else{
			last = r;
			l = &(r->next);
		}
	}
	unlock(&listlock);
}


static char sbuf[64*1024+512];
void
sender(int fd, int msglen, int interval, int n)
{
	Icmp *ip;
	int i, extra;
	Req *r;
	ushort seq;

	print(">sender fd=%d msglen=%d ival=%d n=%d\n", fd, msglen, interval, n); //%
	ip = (Icmp*)sbuf;

	srand(time(0));
	firstseq = seq = rand();

	for(i = 32; i < msglen; i++)
		sbuf[i] = i;
	ip->type = EchoRequest;
	ip->code = 0;

	for(i = 0; i < n; i++){
		if(i != 0){
			extra = rint? nrand(interval): 0;
			sleep(interval + extra);
		}
		r = malloc(sizeof *r);
		if(r != nil){
			hnputs(ip->seq, seq);
			r->seq = seq;
			r->next = nil;
			r->replied = 0;
			r->time = usec(); //% nsec /* avoid early free in reply! */
			lock(&listlock);
			if(first == nil)
				first = r;
			else
				last->next = r;
			last = r;
			unlock(&listlock);
			r->time = usec(); //% nsec 
			//PRN("ping-send(%d)\n", seq); //%
			if(write(fd, ip, msglen) < msglen){
				fprint(2, "%s: write failed: %r\n", argv0);
				L4_Sleep (L4_Never); //%
				return;
			}
			seq++;
		}
	}
	done = 1;
	L4_Sleep (L4_Never); //%
}


static uchar rbuf[64*1024+512];
void
rcvr(int fd, int msglen, int interval, int nmsg)
{
	Icmp *ip;
	ushort x;
	int i, n, munged;
	vlong now;
	Req *r;

	ip = (Icmp*)rbuf;
	sum = 0;

	//PRN(">rcvr(%d %d %d %d)\n", fd, msglen, interval, nmsg);

	while(lostmsgs+rcvdmsgs < nmsg){
	        //% alarm((nmsg-lostmsgs-rcvdmsgs)*interval+5000);
		n = read(fd, rbuf, sizeof(rbuf));
		//if (n>0) PRN("rcvr():%d \n", n); //%

		//% alarm(0);
		now = usec();
		if(n <= 0){	/* read interrupted - time to go */
			clean(0, now+MINUTE, nil);
			continue;
		}
		if(n < msglen){
			print("bad len %d/%d\n", n, msglen);
			continue;
		}
		munged = 0;
		for(i = 32; i < msglen; i++)
			if(rbuf[i] != (i&0xff))
				munged++;
		if(munged)
			print("currupted reply\n");
		x = nhgets(ip->seq);
		if(ip->type != EchoReply || ip->code != 0) {
			print("bad sequence/code/type %d/%d/%d\n",
				ip->type, ip->code, x);
			continue;
		}
		if(0){
		  union{vlong v; ulong u[2];} uv ;  uv.v = now;
		  print("usec=%d <%d-%d>\n", v2ulong(now),  uv.u[1], uv.u[0]);  //%
		}
		clean(x, now, ip);
	}
	
	lock(&listlock);
	for(r = first; r; r = r->next)
		if(r->replied == 0)
			lostmsgs++;
	unlock(&listlock);

	if(lostmsgs)
		print("%d out of %d messages lost\n", lostmsgs, lostmsgs+rcvdmsgs);
	L4_Sleep (L4_Never); //%
}

void
usage(void)
{
	fprint(2, "usage: %s [-alq] [-s msgsize] [-i millisecs] [-n #pings] destination\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int fd;
	int msglen, interval, nmsg;

	usec();		/* make sure time file is already open */

	fmtinstall('V', eipfmt);

	msglen = interval = 0;
	nmsg = MAXMSG;
	ARGBEGIN {
	case 'l':
		lostonly++;
		break;
	case 'd':
		debug++;
		break;
	case 's':
		msglen = atoi(ARGF());
		break;
	case 'i':
		interval = atoi(ARGF());
		break;
	case 'n':
		nmsg = atoi(ARGF());
		break;
	case 'a':
		addresses = 1;
		break;
	case 'q':
		quiet = 1;
		break;
	case 'r':
		rint = 1;
		break;
	case 'f':
		flood = 1;
		break;
	} ARGEND;
	if(msglen < 32)
		msglen = 64;
	if(msglen >= 65*1024)
		msglen = 65*1024-1;
	if(interval <= 0 && !flood)
		interval = SLEEPMS;

	if(argc < 1)
		usage();

	notify(catch);

	fd = dial(netmkaddr(argv[0], "icmp", "1"), 0, 0, 0);

	//BRK("<dial(...):%d \n", fd);   //%

	if(fd < 0){
		fprint(2, "%s: couldn't dial: %r\n", argv0);
		exits("dialing");
	}

	print("sending %d %d byte messages %d ms apart\n", nmsg, msglen, interval);

#if 1 //%----------------------------------------
       {
	 uint  stk[512];
	 L4_ThreadId_t  tid;
	 uint  *sp;
	 extern L4_ThreadId_t create_start_thread(uint pc, uint stkp);

	 sp = &stk[510];
	 *(--sp) = nmsg;
	 *(--sp) = interval;
	 *(--sp) = msglen;
	 *(--sp) = fd;
	 *(--sp) = 0;  // return address area

	 tid = create_start_thread((uint)rcvr, (uint)sp);
	 L4_Sleep(L4_TimePeriod (100 * 1000 /*microsec*/));

       }

       sender(fd, msglen, interval, nmsg);

#else //plan9-----------------------------------
	switch(rfork(RFPROC|RFMEM|RFFDG)){
	case -1:
		fprint(2, "%s: can't fork: %r\n", argv0);
	case 0:
		rcvr(fd, msglen, interval, nmsg);
		exits(0);
	default:
		sender(fd, msglen, interval, nmsg);
		wait();
		exits(lostmsgs ? "lost messages" : "");
	}
#endif //%-----------------------------------------
}

void
reply(Req *r, Icmp *ip)
{
        vlong  argrtt;  //%
        //% r->rtt /= 1000LL;   // usec
	sum += r->rtt;
	//print("sum=%d\n", sum); //%
	if(!r->replied)
		rcvdmsgs++;
	argrtt = sum/rcvdmsgs;
	if(!quiet && !lostonly){
		if(addresses)
			print("%ud: %V->%V rtt %lld usec, avg-rtt %lld usec, ttl = %d\n",
				r->seq-firstseq,
				ip->src, ip->dst,
				r->rtt, argrtt, r->ttl);
		else
			print("%ud: rtt %lld usec, avg-rtt %lld usecs, ttl = %d\n",
			      r->seq-firstseq,
			      r->rtt, argrtt, r->ttl);  //%
	}
	r->replied = 1;
}

void
lost(Req *r, Icmp *ip)
{
	if(!quiet){
		if(addresses)
			print("lost %ud: %V->%V\n", r->seq-firstseq,
				ip->src, ip->dst);
		else
			print("lost %ud\n", r->seq-firstseq);
	}
	lostmsgs++;
}

