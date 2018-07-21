#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"			//% ../pc/
#include	"../pc/dat.h"			//% ../pc/
#include	"../pc/fns.h"			//% ../pc/
#include	"../port/error.h"
#include	"ip.h"

/*
 *  some hacks for commonality twixt inferno and plan9
 */

char*
commonuser(void)
{
	return up->user;
}

Chan*
commonfdtochan(int fd, int mode, int a, int b)
{
	return fdtochan(fd, mode, a, b);
}

char*
commonerror(void)
{
	return up->errstr;
}

char*
bootp(Ipifc* _x)
{
	return "unimplmented";
}

int
bootpread(char* _x, ulong _y, int _z)
{
	return	0;
}

Medium tripmedium =
{
	"trip",
};
