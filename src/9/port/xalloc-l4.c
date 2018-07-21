//%%%%%%%%%% xalloc-l4.c %%%%%%%%%%%%%%%

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"

#include  <l4all.h>    //%
#include  <l4/l4io.h>  //%
#include  <lp49/lp49.h>  //%

#define datoff		((ulong)((Xhdr*)0)->data)

#if 1  //%------------------------------------------
#undef PADDR
#undef KADDR
#define  PADDR(x)  (x)
#define  KADDR(x)  (x)
#endif  //%-----------------------------------------

#if 1 //%-----------------------------------------------------------
#define  XMAP_LADDR      20*1024*1024    //20 Meg
#define  XMAP_PADDR      12*1024*1024    //12 Meg
#define  XMAP_LOG2SIZE   22              // 4MB = (1<<22)
#define  XPOOLSIZE       (1<<XMAP_LOG2SIZE)

ulong x_l2p_addr(ulong laddr)
{
    return  laddr - XMAP_LADDR + XMAP_PADDR;
}

ulong x_p2l_addr(ulong paddr)
{
    return  paddr - XMAP_PADDR + XMAP_LADDR;
}

ulong paddr(void* laddr)
{
#if 1
    L4_Msg_t msgreg = {{0}};
    L4_MsgTag_t tag;
    L4_ThreadId_t mx_pager;
    mx_pager.raw = MX_PAGER;
    L4_Word_t arg[2];

    //  DBGPRN(">>%s va=0x%x\n", __FUNCTION__, (uint)laddr);
    L4_MsgClear(&msgreg);
    L4_MsgAppendWord(&msgreg, (L4_Word_t)laddr);
    L4_Set_MsgLabel(&msgreg, GET_PADDR);

    L4_MsgLoad(&msgreg);
    tag = L4_Call(mx_pager);

    L4_MsgStore(tag, &msgreg);
    L4_StoreMRs(1, 2, arg);

  //  DBGPRN("<<%s  ret=0x%x\n", __FUNCTION__, arg[0]);
    return arg[0];
#else
    return  x_l2p_addr((ulong)laddr);
#endif
}


void* kaddr(ulong paddr)
{
    return  (void*)x_p2l_addr(paddr);
}


ulong xphysmap(ulong phys_addr, ulong xmap_addr, ulong log2size, ulong attribute)
{
	L4_Fpage_t  recv_fp;
	L4_MsgTag_t tag;
	L4_Msg_t    msg;
	ulong       size, phys_addr2;

	//	l4printf_g(">xphysmap(0x%x 0x%x %d %d)\n", 
	//		   phys_addr, xmap_addr, log2size, attribute);
	//	request_fp = L4_FpageLog2(phys_addr, log2size);
	recv_fp = L4_FpageLog2(xmap_addr, log2size);  
	
	L4_MsgClear (&msg);
	L4_Accept (L4_MapGrantItems (recv_fp));
	L4_MsgAppendWord(&msg, phys_addr);   //MR1
	L4_MsgAppendWord(&msg, xmap_addr);   //MR2
	L4_MsgAppendWord(&msg, log2size);    //MR3 
	L4_MsgAppendWord(&msg, attribute);   //MR4 
	L4_Set_MsgLabel(&msg, PHYS_MEM_ALLOC); //MR0: label
	L4_MsgLoad (&msg);
 
	tag = L4_Call(L4_Pager());
	if (L4_IpcFailed(tag)) {
	     L4_KDB_Enter("XPHYS MAP ERROR");
	     return  0;
	}

	L4_MsgStore(tag, &msg);
	size = L4_MsgWord(&msg, 0); // mapped phys-addr 
	phys_addr2 = L4_MsgWord(&msg, 1); // mapped phys-addr 
	//	l4printf_g("<xphysmap(0x%x 0x%x 0x%x %d)\n", 
	//		   phys_addr2, xmap_addr, size, attribute);
	return 1;
}


//--------------------------------------------------------------
void mem_read_test(unsigned *start, unsigned size )
{
    int  i;
    l4printf_g("mem_pool_read  ");
    for(i=0; i<size/4; i+=1024)
        l4printf_g("<%x> ", start[i] );
    l4printf_g("\n");
}

#endif //%----------------------------------------------------------------------

enum
{
  //%	Chunk		= 64*1024,
	Nhole		= 128,
	Magichole	= 0x484F4C45,			/* HOLE */
};

typedef struct Hole Hole;
typedef struct Xalloc Xalloc;
typedef struct Xhdr Xhdr;


struct Hole
{
	ulong	addr;
	ulong	size;
	ulong	top;
	Hole*	link;
};

struct Xhdr
{
	ulong	size;
	ulong	magix;
	char	data[1];
};

struct Xalloc
{
	Lock    _lock;
	Hole	hole[Nhole];
	Hole*	flist;
	Hole*	table;
};

static Xalloc	xlists;

void
xinit(void)
{
	Hole *h, *eh;

	eh = &xlists.hole[Nhole-1];
	for(h = xlists.hole; h < eh; h++)
		h->link = h+1;

	xlists.flist = xlists.hole;

#if 1   //----------------------------------------------------
	if (xphysmap(XMAP_PADDR, XMAP_LADDR, XMAP_LOG2SIZE, 0) == 0)
	     L4_KDB_Enter("XPHYSMAP");

	xhole(XMAP_LADDR, (1<<XMAP_LOG2SIZE));
	//	mem_read_test(XMAP_LADDR, 2*1024*1024);

#else   //--------------------------------------------------
	xhole(XMAP_LADDR, 1<<XMAP_LOG2SIZE);	
#endif  //---------------------------------------------------
}

void*
xspanalloc(ulong size, int align, ulong span)
{
	ulong a, v, t;
	a = (ulong)xalloc(size+align+span);
	if(a == 0)
		panic("xspanalloc: %lud %d %lux\n", size, align, span);

	if(span > 2) {
		v = (a + span) & ~(span-1);
		t = v - a;
		if(t > 0)
			xhole(PADDR(a), t);
		t = a + span - v;
		if(t > 0)
			xhole(PADDR(v+size+align), t);
	}
	else
		v = a;

	if(align > 1)
		v = (v + align) & ~(align-1);

	return (void*)v;
}

void*
xallocz(ulong size, int zero)
{
	Xhdr *p;
	Hole *h, **l;

	size += BY2V + sizeof(Xhdr);
	size &= ~(BY2V-1);

	ilock(&xlists._lock); //%
	l = &xlists.table;
	for(h = *l; h; h = h->link) {
		if(h->size >= size) {
			p = (Xhdr*)KADDR(h->addr);
			h->addr += size;
			h->size -= size;
			if(h->size == 0) {
				*l = h->link;
				h->link = xlists.flist;
				xlists.flist = h;
			}
			iunlock(&xlists._lock);  //%
			if(zero)
				memset(p->data, 0, size);
			p->magix = Magichole;
			p->size = size;
			return p->data;
		}
		l = &h->link;
	}
	iunlock(&xlists._lock);  //%
	return nil;
}

void*
xalloc(ulong size)
{
	return xallocz(size, 1);
}

void
xfree(void *p)
{
	Xhdr *x;

	x = (Xhdr*)((ulong)p - datoff);
	if(x->magix != Magichole) {
		xsummary();
		panic("xfree(0x%lux) 0x%lux!=0x%lux", p, (ulong)Magichole, x->magix);
	}
	xhole((ulong)PADDR(x), x->size);
}

int
xmerge(void *vp, void *vq)
{
	Xhdr *p, *q;

	p = vp;
	if((uchar*)vp+p->size == (uchar*)vq) {
		q = vq;
		p->size += q->size;
		return 1;
	}
	return 0;
}

void
xhole(ulong addr, ulong size)
{
	ulong top;
	Hole *h, *c, **l;

	if(size == 0)
		return;

	top = addr + size;
	ilock(&xlists._lock);  //%
	l = &xlists.table;
	for(h = *l; h; h = h->link) {
		if(h->top == addr) {
			h->size += size;
			h->top = h->addr+h->size;
			c = h->link;
			if(c && h->top == c->addr) {
				h->top += c->size;
				h->size += c->size;
				h->link = c->link;
				c->link = xlists.flist;
				xlists.flist = c;
			}
			iunlock(&xlists._lock); //%
			return;
		}
		if(h->addr > addr)
			break;
		l = &h->link;
	}
	if(h && top == h->addr) {
		h->addr -= size;
		h->size += size;
		iunlock(&xlists._lock);  //%
		return;
	}

	if(xlists.flist == nil) {
	  iunlock(&xlists._lock);  //%
		print("xfree: no free holes, leaked %lud bytes\n", size);
		return;
	}

	h = xlists.flist;
	xlists.flist = h->link;
	h->addr = addr;
	h->top = top;
	h->size = size;
	h->link = *l;
	*l = h;
	iunlock(&xlists._lock); //%
}

void
xsummary(void)
{
	int i;
	Hole *h;

	i = 0;
	for(h = xlists.flist; h; h = h->link)
		i++;

	print("%d holes free\n", i);
	i = 0;
	for(h = xlists.table; h; h = h->link) {
		l4printf("%.8lux %.8lux %lud\n", h->addr, h->top, h->size);
		i += h->size;
	}
	print("%d bytes free\n", i);
}

