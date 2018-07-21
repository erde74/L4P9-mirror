#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "dat.h"
#include "fns.h"

#define DBGPRN  if(0)print
#define PRN     if(1)print

/*
 * We used to use 100 i/o buffers of size 2kb (Sectorsize).
 * Unfortunately, reading 2kb at a time often hopping around
 * the disk doesn't let us get near the disk bandwidth.
 *
 * Based on a trace of iobuf address accesses taken while
 * tarring up a Plan 9 distribution CD, we now use 16 128kb
 * buffers.  This works for ISO9660 because data is required
 * to be laid out contiguously; effectively we're doing agressive
 * readahead.  Because the buffers are so big and the typical 
 * disk accesses so concentrated, it's okay that we have so few
 * of them.
 *
 * If this is used to access multiple discs at once, it's not clear
 * how gracefully the scheme degrades, but I'm not convinced
 * it's worth worrying about.		-rsc
 */

#define	BUFPERCLUST	32 //% original is 64	/* 64*Sectorsize = 128kb */
#define	NCLUST		16

static Ioclust*	iohead;
static Ioclust*	iotail;
static Ioclust*	getclust(Xdata*, long);
static void	putclust(Ioclust*);
static void xread(Ioclust*);

void
iobuf_init(void)
{
	int i, j, n;
	Ioclust *c;
	Iobuf *b;
	uchar *mem;

	n = NCLUST*sizeof(Ioclust)+NCLUST*BUFPERCLUST*(sizeof(Iobuf)+Sectorsize);
#if 1 //%
	static char memblock[NCLUST*sizeof(Ioclust)+NCLUST*BUFPERCLUST*(sizeof(Iobuf)+Sectorsize)];
	mem = memblock;
#else //original
	mem = sbrk(n);
#endif //%
	if(mem == (void*)-1)
		panic(0, "iobuf_init");
	memset(mem, 0, n);

	for(i=0; i<NCLUST; i++){
		c = (Ioclust*)mem;
		mem += sizeof(Ioclust);
		c->addr = -1;
		c->prev = iotail;
		if(iotail)
			iotail->next = c;
		iotail = c;
		if(iohead == nil)
			iohead = c;

		c->buf = (Iobuf*)mem;
		mem += BUFPERCLUST*sizeof(Iobuf);
		c->iobuf = mem;
		mem += BUFPERCLUST*Sectorsize;
		for(j=0; j<BUFPERCLUST; j++){
			b = &c->buf[j];
			b->clust = c;
			b->addr = -1;
			b->iobuf = c->iobuf+j*Sectorsize;
		}
	}
}

void
purgebuf(Xdata *dev)
{
	Ioclust *p;

	for(p=iohead; p!=nil; p=p->next)
		if(p->dev == dev){
			p->addr = -1;
			p->busy = 0;
		}
}

static Ioclust*
getclust(Xdata *dev, long addr)
{
	Ioclust *c, *f;

	f = nil;
	for(c=iohead; c; c=c->next){
		if(!c->busy)
			f = c;
		if(c->addr == addr && c->dev == dev){
			c->busy++;
			return c;
		}
	}

	if(f == nil)
		panic(0, "out of buffers");

	f->addr = addr;
	f->dev = dev;
	f->busy++;
	if(waserror()){
		f->addr = -1;	/* stop caching */
		putclust(f);
		nexterror();
	}
	xread(f);
	poperror();
	return f;
}

static void
putclust(Ioclust *c)
{
	if(c->busy <= 0)
		panic(0, "putbuf");
	c->busy--;

	/* Link onto head for LRU */
	if(c == iohead)
		return;
	c->prev->next = c->next;

	if(c->next)
		c->next->prev = c->prev;
	else
		iotail = c->prev;

	c->prev = nil;
	c->next = iohead;
	iohead->prev = c;
	iohead = c;
}

Iobuf*
getbuf(Xdata *dev, long addr)
{
	int off;
	Ioclust *c;

	off = addr%BUFPERCLUST;
	c = getclust(dev, addr - off);
	if(c->nbuf < off){
		c->busy--;
		error("I/O read error");
	}
	return &c->buf[off];
}

void
putbuf(Iobuf *b)
{
	putclust(b->clust);
}

#if 1 //%Debug ---------------
void  print_fpage(void *adrs, uint size)
{
  uint  fpagesize;
  uint  fpgmask;
  uint  fpagebase;

  for(fpagesize = 4096; fpagesize <= 0x200000; fpagesize *= 2) {
    if (fpagesize < size)  continue; 

    fpgmask = ~(fpagesize - 1);
    fpagebase = (uint)adrs & fpgmask;

    if (((uint)adrs+size) <= (fpagebase + fpagesize)) {
      DBGPRN("fpage[%x %x] ", fpagebase, fpagesize);
      return ;
    }
  }
  print("9660srv:fpage-err[%x %x] \n", fpagebase, fpagesize);
}


int readn2(int fd, void *buf, int len)
{
    int  remain = len;
    int  sz = 0;
    int  m, i;
#define  BSIZE  (2048*4)
    //DBGPRN("  >readn2(%d, 0x%x, ox%x)  ", fd, buf, len);    

    for(i = 0; remain > 0; i++){
      DBGPRN(" Rd<%d>(0x%x, 0x%x):", i, buf + BSIZE*i, 
	     buf + BSIZE*i + ((remain<BSIZE)?remain:BSIZE) );    
      print_fpage(buf + BSIZE*i, (remain<BSIZE)?remain:BSIZE );
        m = read(fd, buf + BSIZE*i, (remain<BSIZE)?remain:BSIZE );
      DBGPRN("%d ", i);
	if (m < 0) {
	    print("readn2-err:%d\n", m);
	    return  m;
	}
	/*
	if (m==0){ 
	    if (sz == 0) return m;
	    break;
	}
	*/
	sz += m;
	remain -= m;
	if (m==0) break;
    }
DBGPRN("  <readn2(%d, 0x%x, ox%x):%d\n", fd, buf, len, sz);    
    return sz;
}
#endif //%-----------------------

static void
xread(Ioclust *c)
{
	int n;
	vlong addr;
	Xdata *dev;

	dev = c->dev;
	addr = c->addr;
	seek(dev->dev, addr*Sectorsize, 0);

#if 1 //%Debug
	n = readn2(dev->dev, c->iobuf, BUFPERCLUST*Sectorsize);
#else //%
	n = readn(dev->dev, c->iobuf, BUFPERCLUST*Sectorsize);
#endif //%

	if(n < Sectorsize)
		error("I/O read error");
	c->nbuf = n/Sectorsize;
}
