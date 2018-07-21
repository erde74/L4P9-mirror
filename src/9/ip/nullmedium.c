#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"			//% ../pc/
#include "../pc/dat.h"			//% ../pc/
#include "../pc/fns.h"			//% ../pc/
#include "../port/error.h"

#include "ip.h"

static void
nullbind(Ipifc* _x, int _y, char** _z)
{
	error("cannot bind null device");
}

static void
nullunbind(Ipifc* _x)
{
}

static void
nullbwrite(Ipifc* _w, Block* _x, int _y, uchar* _z)
{
	error("nullbwrite");
}

Medium nullmedium =
{
.name=		"null",
.bind=		nullbind,
.unbind=	nullunbind,
.bwrite=	nullbwrite,
};

void
nullmediumlink(void)
{
	addipmedium(&nullmedium);
}
