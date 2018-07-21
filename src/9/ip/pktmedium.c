#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"								//% ../pc/
#include "../pc/dat.h"								//% ../pc/
#include "../pc/fns.h"								//% ../pc/
#include "../port/error.h"

#include "ip.h"


static void	pktbind(Ipifc*, int, char**);
static void	pktunbind(Ipifc*);
static void	pktbwrite(Ipifc*, Block*, int, uchar*);
static void	pktin(Fs*, Ipifc*, Block*);

Medium pktmedium =
{
.name=		"pkt",
.hsize=		14,
.mintu=		40,
.maxtu=		4*1024,
.maclen=	6,
.bind=		pktbind,
.unbind=	pktunbind,
.bwrite=	pktbwrite,
.pktin=		pktin,
.unbindonclose=	1,
};

/*
 *  called to bind an IP ifc to an ethernet device
 *  called with ifc wlock'd
 */
static void
pktbind(Ipifc* _x, int _y, char** _z)
{
}

/*
 *  called with ifc wlock'd
 */
static void
pktunbind(Ipifc* _x)
{
}

/*
 *  called by ipoput with a single packet to write
 */
static void
pktbwrite(Ipifc *ifc, Block *bp, int _x, uchar* _y)
{
	/* enqueue onto the conversation's rq */
	bp = concatblock(bp);
	if(ifc->conv->snoopers.ref > 0)
		qpass(ifc->conv->sq, copyblock(bp, BLEN(bp)));
	qpass(ifc->conv->rq, bp);
}

/*
 *  called with ifc rlocked when someone write's to 'data'
 */
static void
pktin(Fs *f, Ipifc *ifc, Block *bp)
{
	if(ifc->lifc == nil)
		freeb(bp);
	else {
		if(ifc->conv->snoopers.ref > 0)
			qpass(ifc->conv->sq, copyblock(bp, BLEN(bp)));
		ipiput4(f, ifc, bp);
	}
}

void
pktmediumlink(void)
{
	addipmedium(&pktmedium);
}
