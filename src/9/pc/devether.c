/*
 * (C2) YS, KM   NII
 */
//%%%%%%%%%%%%%%%%%%

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "ureg.h"
#include "../port/error.h"
#include "../port/netif.h"

#include "etherif.h"

#include "../errhandler-l4.h"  //%

#define _DBGFLG	  0
#include <l4/_dbgpr.h>


static Ether *etherxx[MaxEther];  
//% etherxx[] is set by etherreset() using etherprobe().

//% use example: # bind -a #l net
Chan*
etherattach(char* spec)   //% ONERR nil
{
	ulong ctlrno;
	char *p;
	Chan *chan;

	DBGPRN("> %s(%s)\n", __FUNCTION__, spec);
	ctlrno = 0;
	if(spec && *spec){
		ctlrno = strtoul(spec, &p, 0);
		if((ctlrno == 0 && p == spec) || *p || (ctlrno >= MaxEther))
		        ERROR_RETURN(Ebadarg, nil);  //%
	}
	if(etherxx[ctlrno] == 0)
	        ERROR_RETURN(Enodev, nil);  //%

	chan = devattach('l', spec);
	chan->dev = ctlrno;
	if(etherxx[ctlrno]->attach)
		etherxx[ctlrno]->attach(etherxx[ctlrno]);
	return chan;
}

static Walkqid*
etherwalk(Chan* chan, Chan* nchan, char** name, int nname)
{
  return netifwalk(&etherxx[chan->dev]->_netif, chan, nchan, name, nname); 
  //%    netifwalk(etherxx[chan->dev], 
}

static int
etherstat(Chan* chan, uchar* dp, int n)
{
  return netifstat(&etherxx[chan->dev]->_netif, chan, dp, n);  //%
}

static Chan*
etheropen(Chan* chan, int omode)
{
  return netifopen(&etherxx[chan->dev]->_netif, chan, omode);  //%
}

static void
ethercreate(Chan* _w, char* _x, int _y, ulong _z)
{
}

static void
etherclose(Chan* chan)
{
  netifclose(&etherxx[chan->dev]->_netif, chan);  //%
}

static long
etherread(Chan* chan, void* buf, long n, vlong off)
{
	Ether *ether;
	ulong offset = off;

DBGPRN(" > %s, chan=%d(%s)\n", __FUNCTION__, chan->fid, chan->path->s);

	ether = etherxx[chan->dev];
	if((chan->qid.type & QTDIR) == 0 && ether->ifstat){
		/*
		 * With some controllers it is necessary to reach
		 * into the chip to extract statistics.
		 */
		if(NETTYPE(chan->qid.path) == Nifstatqid)
			return ether->ifstat(ether, buf, n, offset);
		else if(NETTYPE(chan->qid.path) == Nstatqid)
			ether->ifstat(ether, buf, 0, offset);
	}

DBGPRN(" < %s\n", __FUNCTION__);

	return netifread(&ether->_netif, chan, buf, n, offset);  //% ether
}

static Block*
etherbread(Chan* chan, long n, ulong offset)
{
DBGPRN(" > %s, chan=%d(%s)\n", __FUNCTION__, chan->fid, chan->path->s);
DBGPRN(" < %s\n", __FUNCTION__);
  return netifbread(&etherxx[chan->dev]->_netif, chan, n, offset);  //%
}

static int
etherwstat(Chan* chan, uchar* dp, int n)
{
  return netifwstat(&etherxx[chan->dev]->_netif, chan, dp, n);  //%
}

static void
etherrtrace(Netfile* f, Etherpkt* pkt, int len)
{
	int i, n;
	Block *bp;

	if(qwindow(f->in) <= 0)
		return;
	if(len > 58)
		n = 58;
	else
		n = len;
	bp = iallocb(64);
	if(bp == nil)
		return;
	memmove(bp->wp, pkt->d, n);
	i = TK2MS(MACHP(0)->ticks);
	bp->wp[58] = len>>8;
	bp->wp[59] = len;
	bp->wp[60] = i>>24;
	bp->wp[61] = i>>16;
	bp->wp[62] = i>>8;
	bp->wp[63] = i;
	bp->wp += 64;
	qpass(f->in, bp);
}

Block*
etheriq(Ether* ether, Block* bp, int fromwire)
{
	Etherpkt *pkt;
	ushort type;
	int len, multi, tome, fromme;
	Netfile **ep, *f, **fp, *fx;
	Block *xbp;
	check_clock('e', ">etheriq"); //%

	ether->_netif.inpackets++;  //% ether->inpackets++;  

	pkt = (Etherpkt*)bp->rp;
	len = BLEN(bp);
	type = (pkt->type[0]<<8)|pkt->type[1];
	fx = 0;
	ep = &ether->_netif.f[Ntypes];   //% ep = &ether->f[Ntypes];

	multi = pkt->d[0] & 1;
	/* check for valid multcast addresses */
	if(multi && memcmp(pkt->d, ether->_netif.bcast, sizeof(pkt->d)) != 0 && ether->_netif.prom == 0){    //%  bcast  prom
	  if(!activemulti(&ether->_netif, pkt->d, sizeof(pkt->d))){   //% ether
			if(fromwire){
				freeb(bp);
				bp = 0;
			}
			return bp;
		}
	}

	/* is it for me? */
	tome = memcmp(pkt->d, ether->ea, sizeof(pkt->d)) == 0;
	fromme = memcmp(pkt->s, ether->ea, sizeof(pkt->s)) == 0;

	/*
	 * Multiplex the packet to all the connections which want it.
	 * If the packet is not to be used subsequently (fromwire != 0),
	 * attempt to simply pass it into one of the connections, thereby
	 * saving a copy of the data (usual case hopefully).
	 */
	for(fp = ether->_netif.f; fp < ep; fp++){   //% _netif
		if((f = *fp))
		if(f->type == type || f->type < 0)
		if(tome || multi || f->prom){
			/* Don't want to hear bridged packets */
			if(f->bridge && !fromwire && !fromme)
				continue;
			if(!f->headersonly){
				if(fromwire && fx == 0)
					fx = f;
				else if((xbp = iallocb(len))){
					memmove(xbp->wp, pkt, len);
					xbp->wp += len;
					if(qpass(f->in, xbp) < 0)
					  ether->_netif.soverflows++;  //% _netif
				}
				else
				  ether->_netif.soverflows++;   //%
			}
			else
				etherrtrace(f, pkt, len);
		}
	}

	if(fx){
		if(qpass(fx->in, bp) < 0)
		  ether->_netif.soverflows++;   //%
		return 0;
	}
	if(fromwire){
		freeb(bp);
		return 0;
	}

	return bp;
}

static int
etheroq(Ether* ether, Block* bp)
{
	int len, loopback, s;
	Etherpkt *pkt;

	ether->_netif.outpackets++;   //% _netif

	/*
	 * Check if the packet has to be placed back onto the input queue,
	 * i.e. if it's a loopback or broadcast packet or the interface is
	 * in promiscuous mode.
	 * If it's a loopback packet indicate to etheriq that the data isn't
	 * needed and return, etheriq will pass-on or free the block.
	 * To enable bridging to work, only packets that were originated
	 * by this interface are fed back.
	 */
	pkt = (Etherpkt*)bp->rp;
	len = BLEN(bp);
	loopback = memcmp(pkt->d, ether->ea, sizeof(pkt->d)) == 0;
	if(loopback || memcmp(pkt->d, ether->_netif.bcast, sizeof(pkt->d)) == 0 || ether->_netif.prom){    //% _netif
		s = splhi();
		etheriq(ether, bp, 0);
		splx(s);
	}

	if(!loopback){
		qbwrite(ether->oq, bp);
		if(ether->transmit != nil)
			ether->transmit(ether);
	} else
		freeb(bp);

	return len;
}

static long
etherwrite(Chan* chan, void* buf, long n, vlong _x)   //% ONERR
{
	Ether *ether;
	Block *bp;
	int nn, onoff;
	Cmdbuf *cb;

DBGPRN(" > %s, chan=%d(%s)\n", __FUNCTION__, chan->fid, chan->path->s);

	ether = etherxx[chan->dev];
	if(NETTYPE(chan->qid.path) != Ndataqid) {
	  nn = netifwrite(&ether->_netif, chan, buf, n);  //%  (ether, 
		if(nn >= 0)
			return nn;
		cb = parsecmd(buf, n);
		if(strcmp(cb->f[0], "nonblocking") == 0){
			if(cb->nf <= 1)
				onoff = 1;
			else
				onoff = atoi(cb->f[1]);
			qnoblock(ether->oq, onoff);
			free(cb);
			return n;
		}
		free(cb);
		if(ether->ctl!=nil)
			return ether->ctl(ether,buf,n);
			
		ERROR_RETURN(Ebadctl, ONERR);  //%
	}

	if(n > ether->maxmtu)
	        ERROR_RETURN(Etoobig, ONERR);  //%
	if(n < ether->minmtu)
	        ERROR_RETURN(Etoosmall, ONERR);  //%

	bp = allocb(n);
	if(WASERROR()){   //%
	_ERR_1:
		freeb(bp);
		NEXTERROR_RETURN(ONERR);   //%
	}
	memmove(bp->rp, buf, n);
	memmove(bp->rp+Eaddrlen, ether->ea, Eaddrlen);
	POPERROR();  //%
	bp->wp += n;

DBGPRN(" < %s\n", __FUNCTION__);
	return etheroq(ether, bp);
}

static long
etherbwrite(Chan* chan, Block* bp, ulong _x)   //% ONERR
{
	Ether *ether;
	long n;

DBGPRN(" > %s(%s), chan=%d(%s)\n", __FUNCTION__, __FILE__, chan->fid, chan->path->s);

	n = BLEN(bp);
	if(NETTYPE(chan->qid.path) != Ndataqid){
	        if(WASERROR()) {  //%
		_ERR_1:
			freeb(bp);
			NEXTERROR_RETURN(ONERR);
		}
		n = etherwrite(chan, bp->rp, n, 0);
		IF_ERR_GOTO(n, ONERR, _ERR_1);  //% 
		POPERROR();   //%

		freeb(bp);
		return n;
	}
	ether = etherxx[chan->dev];

	if(n > ether->maxmtu){
		freeb(bp);
		ERROR_RETURN(Etoobig, ONERR);  //%
	}
	if(n < ether->minmtu){
		freeb(bp);
		ERROR_RETURN(Etoosmall, ONERR);  //%
	}

DBGPRN(" < %s\n", __FUNCTION__);
	return etheroq(ether, bp);
}

static struct {
	char*	type;
	int	(*reset)(Ether*);
} cards[MaxEther+1];

void
addethercard(char* t, int (*r)(Ether*))
{
	static int ncard;

	if(ncard == MaxEther)
		panic("too many ether cards");
	cards[ncard].type = t;
	cards[ncard].reset = r;
	ncard++;
}

int
parseether(uchar *to, char *from)
{
	char nip[4];
	char *p;
	int i;

	p = from;
	for(i = 0; i < Eaddrlen; i++){
		if(*p == 0)
			return -1;
		nip[0] = *p++;
		if(*p == 0)
			return -1;
		nip[1] = *p++;
		nip[2] = 0;
		to[i] = strtoul(nip, 0, 16);
		if(*p == ':')
			p++;
	}
	return 0;
}

static Ether*
etherprobe(int cardno, int ctlrno)
{
	int i;
	Ether *ether;
	char buf[128], name[32];

	ether = malloc(sizeof(Ether));
	memset(ether, 0, sizeof(Ether));
	ether->ctlrno = ctlrno;
	ether->tbdf = BUSUNKNOWN;
	ether->_netif.mbps = 10;  //% _netif
	ether->minmtu = ETHERMINTU;
	ether->maxmtu = ETHERMAXTU;

	if(cardno < 0){
	  if(isaconfig("ether", ctlrno, &ether->_isaconf) == 0){ //%  _isaconf
			free(ether);
			return nil;
		}
		for(cardno = 0; cards[cardno].type; cardno++){
		  if(cistrcmp(cards[cardno].type, ether->_isaconf.type)) //% _isaconf
				continue;
		  for(i = 0; i < ether->_isaconf.nopt; i++){  //_isaconf.
		    if(strncmp(ether->_isaconf.opt[i], "ea=", 3))  //% _isaconf.
					continue;
		    if(parseether(ether->ea, &ether->_isaconf.opt[i][3])) //%
					memset(ether->ea, 0, Eaddrlen);
			}
			break;
		}
	}

	if(cardno >= MaxEther || cards[cardno].type == nil){
		free(ether);
		return nil;
	}
	if(cards[cardno].reset(ether) < 0){
		free(ether);
		return nil;
	}

	/*
	 * IRQ2 doesn't really exist, it's used to gang the interrupt
	 * controllers together. A device set to IRQ2 will appear on
	 * the second interrupt controller as IRQ9.
	 */
	if(ether->_isaconf.irq == 2)        //% _isaconf.
	        ether->_isaconf.irq = 9;    //% _isaconf.
	snprint(name, sizeof(name), "ether%d", ctlrno);

	/*
	 * If ether->irq is <0, it is a hack to indicate no interrupt
	 * used by ethersink.
	 */
	if(ether->_isaconf.irq >= 0)  //% _isaconf.
	  intrenable(ether->_isaconf.irq, ether->interrupt, ether, ether->tbdf, name);  //_isaconf

	i = sprint(buf, "#l%d: %s: %dMbps port 0x%luX irq %d",
		   ctlrno, cards[cardno].type, ether->_netif.mbps, ether->_isaconf.port, ether->_isaconf.irq);  //% _netif. _isaconf.s
	if(ether->_isaconf.mem)   //% _isaconf
	  i += sprint(buf+i, " addr 0x%luX", ether->_isaconf.mem);  //% _isaconf.
	if(ether->_isaconf.size)  //% _isaconf.
	  i += sprint(buf+i, " size 0x%luX", ether->_isaconf.size); //% _isaconf.
	i += sprint(buf+i, ": %2.2ux%2.2ux%2.2ux%2.2ux%2.2ux%2.2ux",
		ether->ea[0], ether->ea[1], ether->ea[2],
		ether->ea[3], ether->ea[4], ether->ea[5]);
	sprint(buf+i, "\n");
	print(buf);

	if(ether->_netif.mbps >= 1000){   //% _netif.
	  netifinit(&ether->_netif, name, Ntypes, 512*1024);  //% &..._netif
		if(ether->oq == 0)
			ether->oq = qopen(512*1024, Qmsg, 0, 0);
	}else if(ether->_netif.mbps >= 100){  //% _netif.
	  netifinit(&ether->_netif, name, Ntypes, 256*1024); //% &..._netif
		if(ether->oq == 0)
			ether->oq = qopen(256*1024, Qmsg, 0, 0);
	}else{
	        netifinit(&ether->_netif, name, Ntypes, 128*1024); //% &..._netif
		if(ether->oq == 0)
			ether->oq = qopen(128*1024, Qmsg, 0, 0);
	}
	if(ether->oq == 0)
		panic("etherreset %s", name);
	ether->_netif.alen = Eaddrlen;  //% _netif
	memmove(ether->_netif.addr, ether->ea, Eaddrlen);  //% _netif
	memset(ether->_netif.bcast, 0xFF, Eaddrlen);       //% _netif

	return ether;
}

static void
etherreset(void)
{
	Ether *ether;
	int cardno, ctlrno;

	// dbg_pagemap("etherreset1"); //%%
	for(ctlrno = 0; ctlrno < MaxEther; ctlrno++){
		if((ether = etherprobe(-1, ctlrno)) == nil)
			continue;
		etherxx[ctlrno] = ether;
	}

	if(getconf("*noetherprobe"))
		return;

	cardno = ctlrno = 0;
	while(cards[cardno].type != nil && ctlrno < MaxEther){
		if(etherxx[ctlrno] != nil){
			ctlrno++;
			continue;
		}
		if((ether = etherprobe(cardno, ctlrno)) == nil){
			cardno++;
			continue;
		}
		etherxx[ctlrno] = ether;
		ctlrno++;
	}

	// dbg_pagemap("etherreset2"); //%%
	//%% Here, pages [8f0000-900000) mapped. 
}

static void
ethershutdown(void)
{
	Ether *ether;
	int i;

	for(i = 0; i < MaxEther; i++){
		ether = etherxx[i];
		if(ether == nil)
			continue;
		if(ether->shutdown == nil) {
			print("#l%d: no shutdown fuction\n", i);
			continue;
		}
		(*ether->shutdown)(ether);
	}
}


#define POLY 0xedb88320

/* really slow 32 bit crc for ethers */
ulong
ethercrc(uchar *p, int len)
{
	int i, j;
	ulong crc, b;

	crc = 0xffffffff;
	for(i = 0; i < len; i++){
		b = *p++;
		for(j = 0; j < 8; j++){
			crc = (crc>>1) ^ (((crc^b) & 1) ? POLY : 0);
			b >>= 1;
		}
	}
	return crc;
}

Dev etherdevtab = {
	'l',
	"ether",

	etherreset,
	devinit,
	ethershutdown,
	etherattach,
	etherwalk,
	etherstat,
	etheropen,
	ethercreate,
	etherclose,
	etherread,
	etherbread,
	etherwrite,
	etherbwrite,
	devremove,
	etherwstat,
};
