#include <u.h>
#include <libc.h>

extern	char	end[];
static	char	*bloc = { end };
extern	int	brk_(void*);

enum
{
	Round	= 7
};

#if 1 //%----------------------------------

extern int  _end;
char   *_brkadrs = (char *)&_end;

int brk(void *addr)
{
    _brkadrs = (char*)(((uint)addr + Round) & ~Round);
    return  0;
}

void* sbrk(ulong incr)
{
    char *newsize, *oldsize;
    oldsize = _brkadrs;
    newsize = _brkadrs + incr;
    if ((incr > 0 && newsize < oldsize) || (incr < 0 && newsize > oldsize))
        return( (char *) -1);
    if (brk(newsize) == 0)
        return  oldsize;
    else
      return ((void *)-1);
}

#else //original --------------------------------
int
brk(void *p)
{
	uintptr bl;

	bl = ((uintptr)p + Round) & ~Round;
	if(brk_((void*)bl) < 0)
		return -1;
	bloc = (char*)bl;
	return 0;
}

void*
sbrk(ulong n)
{
	uintptr bl;

	bl = ((uintptr)bloc + Round) & ~Round;
	if(brk_((void*)(bl+n)) < 0)
		return (void*)-1;
	bloc = (char*)bl + n;
	return (void*)bl;
}
#endif //%-------------------------------------
