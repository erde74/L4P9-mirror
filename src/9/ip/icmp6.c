/*
 * (C) Bell Lab
 * (C2) YS, KM	 NII
 * Plna9-C -> GCC :  many many.
 */
#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"								//% ../pc/
#include "../pc/dat.h"								//% ../pc/
#include "../pc/fns.h"								//% ../pc/
#include "../port/error.h"
#include "ip.h"
#include "ipv6.h"

typedef struct ICMPpkt ICMPpkt;
typedef struct IPICMP IPICMP;
typedef struct Ndpkt Ndpkt;
typedef struct NdiscC NdiscC;

struct ICMPpkt {
	uchar	type;
	uchar	code;
	uchar	cksum[2];
	uchar	icmpid[2];
	uchar	seq[2];
};

struct IPICMP {
        Ip6hdr    _ip6hdr;     //%
        ICMPpkt   _icmppkt;    //%
};

struct NdiscC
{
        IPICMP  _ipicmp;       //%
	uchar target[IPaddrlen];
};

struct Ndpkt
{
        NdiscC  _ndiscc;     //%
	uchar otype;
	uchar olen;	// length in units of 8 octets(incl type, code),
				// 1 for IEEE 802 addresses
	uchar lnaddr[6];	// link-layer address
};

enum {	
	// ICMPv6 types
	EchoReply	= 0,
	UnreachableV6	= 1,
	PacketTooBigV6	= 2,
	TimeExceedV6	= 3,
	SrcQuench	= 4,
	ParamProblemV6	= 4,
	Redirect	= 5,
	EchoRequest	= 8,
	TimeExceed	= 11,
	InParmProblem	= 12,
	Timestamp	= 13,
	TimestampReply	= 14,
	InfoRequest	= 15,
	InfoReply	= 16,
	AddrMaskRequest = 17,
	AddrMaskReply   = 18,
	EchoRequestV6	= 128,
	EchoReplyV6	= 129,
	RouterSolicit	= 133,
	RouterAdvert	= 134,
	NbrSolicit	= 135,
	NbrAdvert	= 136,
	RedirectV6	= 137,

	Maxtype6	= 137,
};

char *icmpnames6[Maxtype6+1] =
{
[EchoReply]		"EchoReply",
[UnreachableV6]		"UnreachableV6",
[PacketTooBigV6]	"PacketTooBigV6",
[TimeExceedV6]		"TimeExceedV6",
[SrcQuench]		"SrcQuench",
[Redirect]		"Redirect",
[EchoRequest]		"EchoRequest",
[TimeExceed]		"TimeExceed",
[InParmProblem]		"InParmProblem",
[Timestamp]		"Timestamp",
[TimestampReply]	"TimestampReply",
[InfoRequest]		"InfoRequest",
[InfoReply]		"InfoReply",
[AddrMaskRequest]	"AddrMaskRequest",
[AddrMaskReply]		"AddrMaskReply",
[EchoRequestV6]		"EchoRequestV6",
[EchoReplyV6]		"EchoReplyV6",
[RouterSolicit]		"RouterSolicit",
[RouterAdvert]		"RouterAdvert",
[NbrSolicit]		"NbrSolicit",
[NbrAdvert]		"NbrAdvert",
[RedirectV6]		"RedirectV6",
};

enum
{
	InMsgs6,
	InErrors6,
	OutMsgs6,
	CsumErrs6,
	LenErrs6,
	HlenErrs6,
	HoplimErrs6,
	IcmpCodeErrs6,
	TargetErrs6,
	OptlenErrs6,
	AddrmxpErrs6,
	RouterAddrErrs6,

	Nstats6,
};

static char *statnames6[Nstats6] =
{
[InMsgs6]	"InMsgs",
[InErrors6]	"InErrors",
[OutMsgs6]	"OutMsgs",
[CsumErrs6]	"CsumErrs",
[LenErrs6]	"LenErrs",
[HlenErrs6]	"HlenErrs",
[HoplimErrs6]	"HoplimErrs",
[IcmpCodeErrs6]	"IcmpCodeErrs",
[TargetErrs6]	"TargetErrs",
[OptlenErrs6]	"OptlenErrs",
[AddrmxpErrs6]	"AddrmxpErrs",
[RouterAddrErrs6]	"RouterAddrErrs",
};

typedef struct Icmppriv6
{
	ulong	stats[Nstats6];

	/* message counts */
	ulong	in[Maxtype6+1];
	ulong	out[Maxtype6+1];
} Icmppriv6;

typedef struct Icmpcb6 
{
	QLock;
	uchar headers;
} Icmpcb6;

static char *unreachcode[] =
{
[icmp6_no_route]	"no route to destination",
[icmp6_ad_prohib]	"comm with destination administratively prohibited",
[icmp6_unassigned]	"icmp unreachable: unassigned error code (2)",
[icmp6_adr_unreach]	"address unreachable",
[icmp6_port_unreach]	"port unreachable",
[icmp6_unkn_code]	"icmp unreachable: unknown code",
};

enum {
	ICMP_USEAD6	= 40,
};

enum {
	Oflag	= 1<<5,
	Sflag	= 1<<6,
	Rflag	= 1<<7,
};

enum {
	slladd	= 1,
	tlladd	= 2,
	prfinfo	= 3,
	redhdr	= 4,
	mtuopt	= 5,
};

static void icmpkick6(void *x, Block *bp);

static void
icmpcreate6(Conv *c)
{
	c->rq = qopen(64*1024, Qmsg, 0, c);
	c->wq = qbypass(icmpkick6, c);
}

static void
set_cksum(Block *bp)
{
	IPICMP *p = (IPICMP *)(bp->rp);

	hnputl(p->_ip6hdr.vcf, 0);  // borrow IP header as pseudoheader  //%
	hnputs(p->_ip6hdr.ploadlen, blocklen(bp)-IPV6HDR_LEN);           //%
	p->_ip6hdr.proto = 0;                                            //%
	p->_ip6hdr.ttl = ICMPv6;	// ttl gets set later            //%
	hnputs(p->_icmppkt.cksum, 0);                                    //%
	hnputs(p->_icmppkt.cksum, ptclcsum(bp, 0, blocklen(bp)));        //%
	p->_ip6hdr.proto = ICMPv6;                                       //%
}

static Block *
newIPICMP(int packetlen)
{
	Block	*nbp;
	nbp = allocb(packetlen);
	nbp->wp += packetlen;
	memset(nbp->rp, 0, packetlen);
	return nbp;
}

void
icmpadvise6(Proto *icmp, Block *bp, char *msg)
{
	Conv	**c, *s;
	IPICMP	*p;
	ushort	recid;

	p = (IPICMP *) bp->rp;
	recid = nhgets(p->_icmppkt.icmpid);          //%

	for(c = icmp->conv; *c; c++) {
		s = *c;
		if(s->lport == recid)
		  if(ipcmp(s->raddr, p->_ip6hdr.dst) == 0){     //%
			qhangup(s->rq, msg);
			qhangup(s->wq, msg);
			break;
		}
	}
	freeblist(bp);
}

static void
icmpkick6(void *x, Block *bp)
{
	Conv *c = x;
	IPICMP *p;
	uchar laddr[IPaddrlen], raddr[IPaddrlen];
	Icmppriv6 *ipriv = c->p->priv;
	Icmpcb6 *icb = (Icmpcb6*)c->ptcl;

	if(bp == nil)
		return;

	if(icb->headers==6) {
		/* get user specified addresses */
		bp = pullupblock(bp, ICMP_USEAD6);
		if(bp == nil)
			return;
		bp->rp += 8;
		ipmove(laddr, bp->rp);
		bp->rp += IPaddrlen;
		ipmove(raddr, bp->rp);
		bp->rp += IPaddrlen;
		bp = padblock(bp, sizeof(Ip6hdr));
	}

	if(blocklen(bp) < sizeof(IPICMP)){
		freeblist(bp);
		return;
	}
	p = (IPICMP *)(bp->rp);
	if(icb->headers == 6) {
	        ipmove(p->_ip6hdr.dst, raddr);        //%
		ipmove(p->_ip6hdr.src, laddr);        //%
	} else {
 	        ipmove(p->_ip6hdr.dst, c->raddr);     //%
		ipmove(p->_ip6hdr.src, c->laddr);     //%
		hnputs(p->_icmppkt.icmpid, c->lport);  //%
	}

	set_cksum(bp);
	p->_ip6hdr.vcf[0] = 0x06 << 4;                //%
	if(p->_icmppkt.type <= Maxtype6)	              //%
	        ipriv->out[p->_icmppkt.type]++;              //%
	ipoput6(c->p->f, bp, 0, c->ttl, c->tos, nil);
}

char*
icmpctl6(Conv *c, char **argv, int argc)
{
	Icmpcb6 *icb;

	icb = (Icmpcb6*) c->ptcl;

	if(argc==1) {
		if(strcmp(argv[0], "headers")==0) {
			icb->headers = 6;
			return nil;
		}
	}
	return "unknown control request";
}

static void
goticmpkt6(Proto *icmp, Block *bp, int muxkey)
{
	Conv	**c, *s;
	IPICMP	*p = (IPICMP *)bp->rp;
	ushort	recid; 
	uchar 	*addr;

	if(muxkey == 0) {
	        recid = nhgets(p->_icmppkt.icmpid);    //%
		addr = p->_ip6hdr.src;                //%
	}
	else {
		recid = muxkey;
		addr = p->_ip6hdr.dst;          //%
	}

	for(c = icmp->conv; *c; c++){
		s = *c;
		if(s->lport == recid && ipcmp(s->raddr, addr) == 0){
			bp = concatblock(bp);
			if(bp != nil)
				qpass(s->rq, bp);
			return;
		}
	}

	freeblist(bp);
}

static Block *
mkechoreply6(Block *bp)
{
	IPICMP *p = (IPICMP *)(bp->rp);
	uchar	addr[IPaddrlen];

	ipmove(addr, p->_ip6hdr.src);          //%
	ipmove(p->_ip6hdr.src, p->_ip6hdr.dst);        //%
	ipmove(p->_ip6hdr.dst, addr);          //%
	p->_icmppkt.type = EchoReplyV6;         //%
	set_cksum(bp);
	return bp;
}

/*
 * sends out an ICMPv6 neighbor solicitation
 * 	suni == SRC_UNSPEC or SRC_UNI, 
 *	tuni == TARG_MULTI => multicast for address resolution,
 * 	and tuni == TARG_UNI => neighbor reachability.
 */

extern void
icmpns(Fs *f, uchar* src, int suni, uchar* targ, int tuni, uchar* mac)
{
	Block	*nbp;
	Ndpkt *np;
	Proto *icmp = f->t2p[ICMPv6];
	Icmppriv6 *ipriv = icmp->priv;


	nbp = newIPICMP(sizeof(Ndpkt));
	np = (Ndpkt*) nbp->rp;


	if(suni == SRC_UNSPEC) 
	         memmove(np->_ndiscc._ipicmp._ip6hdr.src, v6Unspecified, IPaddrlen);  //%
	else 
	         memmove(np->_ndiscc._ipicmp._ip6hdr.src, src, IPaddrlen);            //%

	if(tuni == TARG_UNI)
	         memmove(np->_ndiscc._ipicmp._ip6hdr.dst, targ, IPaddrlen);      //%
	else
	        ipv62smcast(np->_ndiscc._ipicmp._ip6hdr.dst, targ);              //%

	np->_ndiscc._ipicmp._icmppkt.type = NbrSolicit;            //%
	np->_ndiscc._ipicmp._icmppkt.code = 0;                     //%
	memmove(np->_ndiscc.target, targ, IPaddrlen);   //%
	if(suni != SRC_UNSPEC) {
		np->otype = SRC_LLADDRESS;
		np->olen = 1;	/* 1+1+6 = 8 = 1 8-octet */
		memmove(np->lnaddr, mac, sizeof(np->lnaddr));
	}
	else {
		int r = sizeof(Ndpkt)-sizeof(NdiscC);
		nbp->wp -= r;
	}

	set_cksum(nbp);
	np = (Ndpkt*) nbp->rp;
	np->_ndiscc._ipicmp._ip6hdr.ttl = HOP_LIMIT;      //%
	np->_ndiscc._ipicmp._ip6hdr.vcf[0] = 0x06 << 4;   //%
	ipriv->out[NbrSolicit]++;
	netlog(f, Logicmp, "sending neighbor solicitation %I\n", targ);
	ipoput6(f, nbp, 0, MAXTTL, DFLTTOS, nil);
}

/*
 * sends out an ICMPv6 neighbor advertisement. pktflags == RSO flags.
 */
extern void
icmpna(Fs *f, uchar* src, uchar* dst, uchar* targ, uchar* mac, uchar flags)
{
	Block	*nbp;
	Ndpkt *np;
	Proto *icmp = f->t2p[ICMPv6];
	Icmppriv6 *ipriv = icmp->priv;

	nbp = newIPICMP(sizeof(Ndpkt));
	np = (Ndpkt*) nbp->rp;

	memmove(np->_ndiscc._ipicmp._ip6hdr.src, src, IPaddrlen);     //%
	memmove(np->_ndiscc._ipicmp._ip6hdr.dst, dst, IPaddrlen);     //%

	np->_ndiscc._ipicmp._icmppkt.type = NbrAdvert;                  //%
	np->_ndiscc._ipicmp._icmppkt.code = 0;                          //%
	np->_ndiscc._ipicmp._icmppkt.icmpid[0] = flags;                //%
	memmove(np->_ndiscc.target, targ, IPaddrlen); //%

	np->otype = TARGET_LLADDRESS;
	np->olen = 1;	
	memmove(np->lnaddr, mac, sizeof(np->lnaddr));

	set_cksum(nbp);
	np = (Ndpkt*) nbp->rp;
	np->_ndiscc._ipicmp._ip6hdr.ttl = HOP_LIMIT;      //%
	np->_ndiscc._ipicmp._ip6hdr.vcf[0] = 0x06 << 4;   //%
	ipriv->out[NbrAdvert]++;
	netlog(f, Logicmp, "sending neighbor advertisement %I\n", src);
	ipoput6(f, nbp, 0, MAXTTL, DFLTTOS, nil);
}

extern void
icmphostunr(Fs *f, Ipifc *ifc, Block *bp, int code, int free)
{
	Block *nbp;
	IPICMP *np;
	Ip6hdr	*p;
	int osz = BLEN(bp);
	int sz = MIN(sizeof(IPICMP) + osz, v6MINTU);
	Proto	*icmp = f->t2p[ICMPv6];
	Icmppriv6 *ipriv = icmp->priv;

	p = (Ip6hdr *) bp->rp;

	if(isv6mcast(p->src))
		goto clean;

	nbp = newIPICMP(sz);
	np = (IPICMP *) nbp->rp;

	rlock(&ifc->_rwlock);				//% _rwlock
	if(ipv6anylocal(ifc, np->_ip6hdr.src)) {        //%
		netlog(f, Logicmp, "send icmphostunr -> s%I d%I\n", p->src, p->dst);
	}
	else {
		netlog(f, Logicmp, "icmphostunr fail -> s%I d%I\n", p->src, p->dst);
		freeblist(nbp);
		if(free) 
			goto clean;
		else
			return;
	}

	memmove(np->_ip6hdr.dst, p->src, IPaddrlen);    //%
	np->_icmppkt.type = UnreachableV6;               //%
	np->_icmppkt.code = code;                        //%
	memmove(nbp->rp + sizeof(IPICMP), bp->rp, sz - sizeof(IPICMP));
	set_cksum(nbp);
	np->_ip6hdr.ttl = HOP_LIMIT;                   //%
	np->_ip6hdr.vcf[0] = 0x06 << 4;                //%
	ipriv->out[UnreachableV6]++;

	if(free)
		ipiput6(f, ifc, nbp);
	else {
		ipoput6(f, nbp, 0, MAXTTL, DFLTTOS, nil);
		return;
	}

clean:
	runlock(&ifc->_rwlock);							//% _rwlock
	freeblist(bp);
}

extern void
icmpttlexceeded6(Fs *f, Ipifc *ifc, Block *bp)
{
	Block *nbp;
	IPICMP *np;
	Ip6hdr	*p;
	int osz = BLEN(bp);
	int sz = MIN(sizeof(IPICMP) + osz, v6MINTU);
	Proto	*icmp = f->t2p[ICMPv6];
	Icmppriv6 *ipriv = icmp->priv;

	p = (Ip6hdr *) bp->rp;

	if(isv6mcast(p->src)) 
		return;

	nbp = newIPICMP(sz);
	np = (IPICMP *) nbp->rp;

	if(ipv6anylocal(ifc, np->_ip6hdr.src)) {    //%
		netlog(f, Logicmp, "send icmpttlexceeded6 -> s%I d%I\n", p->src, p->dst);
	}
	else {
		netlog(f, Logicmp, "icmpttlexceeded6 fail -> s%I d%I\n", p->src, p->dst);
		return;
	}

	memmove(np->_ip6hdr.dst, p->src, IPaddrlen);  //%
	np->_icmppkt.type = TimeExceedV6;              //%
	np->_icmppkt.code = 0;                         //%
	memmove(nbp->rp + sizeof(IPICMP), bp->rp, sz - sizeof(IPICMP));
	set_cksum(nbp);
	np->_ip6hdr.ttl = HOP_LIMIT;             //%
	np->_ip6hdr.vcf[0] = 0x06 << 4;          //%
	ipriv->out[TimeExceedV6]++;
	ipoput6(f, nbp, 0, MAXTTL, DFLTTOS, nil);
}

extern void
icmppkttoobig6(Fs *f, Ipifc *ifc, Block *bp)
{
	Block *nbp;
	IPICMP *np;
	Ip6hdr	*p;
	int osz = BLEN(bp);
	int sz = MIN(sizeof(IPICMP) + osz, v6MINTU);
	Proto	*icmp = f->t2p[ICMPv6];
	Icmppriv6 *ipriv = icmp->priv;

	p = (Ip6hdr *) bp->rp;

	if(isv6mcast(p->src)) 
		return;

	nbp = newIPICMP(sz);
	np = (IPICMP *) nbp->rp;

	if(ipv6anylocal(ifc, np->_ip6hdr.src)) {      //%
		netlog(f, Logicmp, "send icmppkttoobig6 -> s%I d%I\n", p->src, p->dst);
	}
	else {
		netlog(f, Logicmp, "icmppkttoobig6 fail -> s%I d%I\n", p->src, p->dst);
		return;
	}

	memmove(np->_ip6hdr.dst, p->src, IPaddrlen);   //%
	np->_icmppkt.type = PacketTooBigV6;             //%
	np->_icmppkt.code = 0;                          //% 
	hnputl(np->_icmppkt.icmpid, ifc->maxtu - ifc->m->hsize);     //%
	memmove(nbp->rp + sizeof(IPICMP), bp->rp, sz - sizeof(IPICMP));
	set_cksum(nbp);
	np->_ip6hdr.ttl = HOP_LIMIT;                //%
	np->_ip6hdr.vcf[0] = 0x06 << 4;             //%
	ipriv->out[PacketTooBigV6]++;
	ipoput6(f, nbp, 0, MAXTTL, DFLTTOS, nil);
}

/*
 * RFC 2461, pages 39-40, pages 57-58.
 */
static int
valid(Proto *icmp, Ipifc *ifc, Block *bp, Icmppriv6 *ipriv) {
	int 	sz, osz, unsp, n, ttl, iplen;
	int 	pktsz = BLEN(bp);
	uchar	*packet = bp->rp;
	IPICMP	*p = (IPICMP *) packet;
	Ndpkt	*np;

//%	USED(ifc);
	n = blocklen(bp);
	if(n < sizeof(IPICMP)) {
		ipriv->stats[HlenErrs6]++;
		netlog(icmp->f, Logicmp, "icmp hlen %d\n", n);
		goto err;
	}

	iplen = nhgets(p->_ip6hdr.ploadlen);    //%
	if(iplen > n-IPV6HDR_LEN || (iplen % 1)) {
		ipriv->stats[LenErrs6]++;
		netlog(icmp->f, Logicmp, "icmp length %d\n", iplen);
		goto err;
	}

	// Rather than construct explicit pseudoheader, overwrite IPv6 header
	if(p->_ip6hdr.proto != ICMPv6) {      //%
		// This code assumes no extension headers!!!
		netlog(icmp->f, Logicmp, "icmp error: extension header\n");
		goto err;
	}
	memset(packet, 0, 4);
	ttl = p->_ip6hdr.ttl;    //%
	p->_ip6hdr.ttl = p->_ip6hdr.proto;    //%
	p->_ip6hdr.proto = 0;         //%
	if(ptclcsum(bp, 0, iplen + IPV6HDR_LEN)) {
		ipriv->stats[CsumErrs6]++;
		netlog(icmp->f, Logicmp, "icmp checksum error\n");
		goto err;
	}
	p->_ip6hdr.proto = p->_ip6hdr.ttl;    //%
	p->_ip6hdr.ttl = ttl;          //%

	/* additional tests for some pkt types */
	if( (p->_icmppkt.type == NbrSolicit) ||           //%
	    (p->_icmppkt.type == NbrAdvert) ||            //%
	    (p->_icmppkt.type == RouterAdvert) ||    //%
	    (p->_icmppkt.type == RouterSolicit) ||   //%
	    (p->_icmppkt.type == RedirectV6) ) {  //%

	        if(p->_ip6hdr.ttl != HOP_LIMIT) {    //%
			ipriv->stats[HoplimErrs6]++; 
			goto err; 
		}
		if(p->_icmppkt.code != 0) {      //%
			ipriv->stats[IcmpCodeErrs6]++; 
			goto err; 
		}

		switch (p->_icmppkt.type) {     //%
		case NbrSolicit:
		case NbrAdvert:
			np = (Ndpkt*) p;
			if(isv6mcast(np->_ndiscc.target)) {    //%
				ipriv->stats[TargetErrs6]++; 
				goto err; 
			}
			if(optexsts(np) && (np->olen == 0)) {   //%
				ipriv->stats[OptlenErrs6]++; 
				goto err; 
			}
		
			if(p->_icmppkt.type == NbrSolicit) {    //%
			        if(ipcmp(np->_ndiscc._ipicmp._ip6hdr.src, v6Unspecified) == 0) {    //%
				        if(!issmcast(np->_ndiscc._ipicmp._ip6hdr.dst) || optexsts(np))  {   //%
						ipriv->stats[AddrmxpErrs6]++; 
						goto err;
					}
				}
			}
		
			if(p->_icmppkt.type == NbrAdvert) {    //%
			        if((isv6mcast(np->_ndiscc._ipicmp._ip6hdr.dst))&&(nhgets(np->_ndiscc._ipicmp._icmppkt.icmpid) & Sflag)){  //%
					ipriv->stats[AddrmxpErrs6]++; 
					goto err; 
				}
			}
			break;
	
		case RouterAdvert:
			if(pktsz - sizeof(Ip6hdr) < 16) {
				ipriv->stats[HlenErrs6]++; 
				goto err; 
			}
			if(!islinklocal(p->_ip6hdr.src)) {     //%
				ipriv->stats[RouterAddrErrs6]++; 
				goto err; 
			}
			sz = sizeof(IPICMP) + 8;
			while ((sz+1) < pktsz) {
				osz = *(packet+sz+1);
				if(osz <= 0) {
					ipriv->stats[OptlenErrs6]++; 
					goto err; 
				}	
				sz += 8*osz;
			}
			break;
	
		case RouterSolicit:
			if(pktsz - sizeof(Ip6hdr) < 8) {
				ipriv->stats[HlenErrs6]++; 
				goto err; 
			}
			unsp = (ipcmp(p->_ip6hdr.src, v6Unspecified) == 0);   //%
			sz = sizeof(IPICMP) + 8;
			while ((sz+1) < pktsz) {
				osz = *(packet+sz+1);
				if((osz <= 0) ||
					(unsp && (*(packet+sz) == slladd)) ) {
					ipriv->stats[OptlenErrs6]++; 
					goto err; 
				}
				sz += 8*osz;
			}
			break;
	
		case RedirectV6:
			//to be filled in
			break;
	
		default:
			goto err;
		}
	}

	return 1;

err:
	ipriv->stats[InErrors6]++; 
	return 0;
}

static int
targettype(Fs *f, Ipifc *ifc, uchar *target)
{
	Iplifc *lifc;
	int t;

	rlock(&ifc->_rwlock);							//% _rwlock
	if(ipproxyifc(f, ifc, target)) {
		runlock(&ifc->_rwlock);						//% _rwlock
		return t_uniproxy;
	}

	for(lifc = ifc->lifc; lifc; lifc = lifc->next) {
		if(ipcmp(lifc->local, target) == 0) {
			t = (lifc->tentative) ? t_unitent : t_unirany; 
			runlock(&ifc->_rwlock);					//% _rwlock
			return t;
		}
	}

	runlock(&ifc->_rwlock);							//% _rwlock
	return 0;
}

static void
icmpiput6(Proto *icmp, Ipifc *ipifc, Block *bp)
{
	uchar	*packet = bp->rp;
	IPICMP	*p = (IPICMP *)packet;
	Icmppriv6 *ipriv = icmp->priv;
	Block	*r;
	Proto	*pr;
	char	*msg, m2[128];
	Ndpkt* np;
	uchar pktflags;
	uchar lsrc[IPaddrlen];
	int refresh = 1;
	Iplifc *lifc;

	if(!valid(icmp, ipifc, bp, ipriv)) 
		goto raise;

	if(p->_icmppkt.type <= Maxtype6)      //%
	        ipriv->in[p->_icmppkt.type]++;    //%
	else
		goto raise;

	switch(p->_icmppkt.type) {           //%
	case EchoRequestV6:
		r = mkechoreply6(bp);
		ipriv->out[EchoReply]++;
		ipoput6(icmp->f, r, 0, MAXTTL, DFLTTOS, nil);
		break;

	case UnreachableV6:
	        if(p->_icmppkt.code > 4)     //%
			msg = unreachcode[icmp6_unkn_code];
		else
		        msg = unreachcode[p->_icmppkt.code];   //%

		bp->rp += sizeof(IPICMP);
		if(blocklen(bp) < 8){
			ipriv->stats[LenErrs6]++;
			goto raise;
		}
		p = (IPICMP *)bp->rp;
		pr = Fsrcvpcolx(icmp->f, p->_ip6hdr.proto);     //%
		if(pr != nil && pr->advise != nil) {
			(*pr->advise)(pr, bp, msg);
			return;
		}

		bp->rp -= sizeof(IPICMP);
		goticmpkt6(icmp, bp, 0);
		break;

	case TimeExceedV6:
	  if(p->_icmppkt.code == 0){         //%
	                sprint(m2, "ttl exceeded at %I", p->_ip6hdr.src);    //%

			bp->rp += sizeof(IPICMP);
			if(blocklen(bp) < 8){
				ipriv->stats[LenErrs6]++;
				goto raise;
			}
			p = (IPICMP *)bp->rp;
			pr = Fsrcvpcolx(icmp->f, p->_ip6hdr.proto);      //%
			if(pr != nil && pr->advise != nil) {
				(*pr->advise)(pr, bp, m2);
				return;
			}
			bp->rp -= sizeof(IPICMP);
		}

		goticmpkt6(icmp, bp, 0);
		break;

	case RouterAdvert:
	case RouterSolicit:
		/* using lsrc as a temp, munge hdr for goticmp6 
		memmove(lsrc, p->_ip6hdr.src, IPaddrlen);
		memmove(p->_ip6hdr.src, p->_ip6hdr.dst, IPaddrlen);
		memmove(p->_ip6hdr.dst, lsrc, IPaddrlen); */

	        goticmpkt6(icmp, bp, p->_icmppkt.type);     //%
		break;

	case NbrSolicit:
		np = (Ndpkt*) p;
		pktflags = 0;
		switch (targettype(icmp->f, ipifc, np->_ndiscc.target)) {   //%
		case t_unirany:
			pktflags |= Oflag;
			/* fall through */

		case t_uniproxy: 
		        if(ipcmp(np->_ndiscc._ipicmp._ip6hdr.src, v6Unspecified) != 0) {     //%
			  arpenter(icmp->f, V6, np->_ndiscc._ipicmp._ip6hdr.src, np->lnaddr, 8*np->olen-2, 0);  //%
				pktflags |= Sflag;
			}
			if(ipv6local(ipifc, lsrc)) {
				icmpna(icmp->f, lsrc, 
				       (ipcmp(np->_ndiscc._ipicmp._ip6hdr.src, v6Unspecified)==0)?v6allnodesL:np->_ndiscc._ipicmp._ip6hdr.src, //%
				       np->_ndiscc.target, ipifc->mac, pktflags);   //%
			}
			else
				freeblist(bp);
			break;

		case t_unitent:
			/* not clear what needs to be done. send up
			 * an icmp mesg saying don't use this address? */

		default:
			freeblist(bp);
		}

		break;

	case NbrAdvert:
		np = (Ndpkt*) p;

		/* if the target address matches one of the local interface 
		 * address and the local interface address has tentative bit set, 
		 * then insert into ARP table. this is so the duplication address 
		 * detection part of ipconfig can discover duplication through 
		 * the arp table
		 */
		lifc = iplocalonifc(ipifc, np->_ndiscc.target);    //%
		if(lifc && lifc->tentative)
			refresh = 0;
		arpenter(icmp->f, V6, np->_ndiscc.target, np->lnaddr, 8*np->olen-2, refresh); //%
		freeblist(bp);
		break;

	case PacketTooBigV6:

	default:
		goticmpkt6(icmp, bp, 0);
		break;
	}
	return;

raise:
	freeblist(bp);

}

int
icmpstats6(Proto *icmp6, char *buf, int len)
{
	Icmppriv6 *priv;
	char *p, *e;
	int i;

	priv = icmp6->priv;
	p = buf;
	e = p+len;
	for(i = 0; i < Nstats6; i++)
		p = seprint(p, e, "%s: %lud\n", statnames6[i], priv->stats[i]);
	for(i = 0; i <= Maxtype6; i++){
		if(icmpnames6[i])
			p = seprint(p, e, "%s: %lud %lud\n", icmpnames6[i], priv->in[i], priv->out[i]);
/*		else
			p = seprint(p, e, "%d: %lud %lud\n", i, priv->in[i], priv->out[i]);
*/
	}
	return p - buf;
}


// need to import from icmp.c
extern int	icmpstate(Conv *c, char *state, int n);
extern char*	icmpannounce(Conv *c, char **argv, int argc);
extern char*	icmpconnect(Conv *c, char **argv, int argc);
extern void	icmpclose(Conv *c);

void
icmp6init(Fs *fs)
{
	Proto *icmp6 = smalloc(sizeof(Proto));

	icmp6->priv = smalloc(sizeof(Icmppriv6));
	icmp6->name = "icmpv6";
	icmp6->connect = icmpconnect;
	icmp6->announce = icmpannounce;
	icmp6->state = icmpstate;
	icmp6->create = icmpcreate6;
	icmp6->close = icmpclose;
	icmp6->rcv = icmpiput6;
	icmp6->stats = icmpstats6;
	icmp6->ctl = icmpctl6;
	icmp6->advise = icmpadvise6;
	icmp6->gc = nil;
	icmp6->ipproto = ICMPv6;
	icmp6->nc = 16;
	icmp6->ptclsize = sizeof(Icmpcb6);

	Fsproto(fs, icmp6);
}

