#define size_t  unsigned 
#define NULL   (void*)0   

extern int  _end;  
char   *_brkadrs = (char *)&_end;

void free(void *ptr);

//---------------------
int brk(char *addr)
{
    //  unsigned   reqbrk = (unsigned)addr; 
    //  unsigned   oldbrk = (unsigned)_brkadrs;
    _brkadrs = addr;
    return  0;
}

//---------------------
char *sbrk(int incr)
{
    char *newsize, *oldsize;

    oldsize = _brkadrs;
    newsize = _brkadrs + incr;
    //l4printf("SBRK  oldsize= %x delta= %x  newsize= %x \n", 
    //	  oldsize, incr, newsize);
    if (incr > 0 && newsize < oldsize || incr < 0 && newsize > oldsize)
        return( (char *) -1);
    if (brk(newsize) == 0)
	return(oldsize);
    else
	return( (char *) -1);
}

#if 1 
#define  assert(expr)
#else 
#define assert(expr)    ((expr)? (void)0 : 1)
#endif

#define	ptrint		int
#define BRKSIZE		4096

#define	PTRSIZE		((int) sizeof(void *))
#define Align(x,a)	(((x) + (a - 1)) & ~(a - 1))
#define NextSlot(p)	(* (void **) ((p) - PTRSIZE))
#define NextFree(p)	(* (void **) (p))

static void *_bottom = NULL;
static void *_top = NULL;
static void *_empty = NULL;

static int grow(size_t len)
{
    char *p;

    assert(NextSlot((char *)_top) == 0);
    if ((char *) _top + len < (char *) _top
	|| (p = (char *)Align((ptrint)_top + len, BRKSIZE)) < (char *)_top ) {
        return(0);
    }
    if (brk(p) != 0)
        return(0);
    NextSlot((char *)_top) = p;
    NextSlot(p) = 0;
    free(_top);
    _top = p;
    return 1;
}

//---------------------
void * malloc(size_t size)
{
    char *prev, *p, *next, *new;
    unsigned len, ntries;

    if (size == 0)
	return NULL;

    for (ntries = 0; ntries < 2; ntries++) {
	if ((len = Align(size, PTRSIZE) + PTRSIZE) < 2 * PTRSIZE) {
		return NULL;
	}
	if (_bottom == 0) {
		if ((p = sbrk(2 * PTRSIZE)) == (char *) -1)
			return NULL;
		p = (char *) Align((ptrint)p, PTRSIZE);
		p += PTRSIZE;
		_top = _bottom = p;
		NextSlot(p) = 0;
	}
#ifdef SLOWDEBUG
	for (p = _bottom; (next = NextSlot(p)) != 0; p = next)
		assert(next > p);
	assert(p == _top);
#endif
	for (prev = 0, p = _empty; p != 0; prev = p, p = NextFree(p)) {
		next = NextSlot(p);
		new = p + len;	/* easily overflows!! */
		if (new > next || new <= p)
			continue;		/* too small */
		if (new + PTRSIZE < next) {	/* too big, so split */
			/* + PTRSIZE avoids tiny slots on free list */
			NextSlot(new) = next;
			NextSlot(p) = new;
			NextFree(new) = NextFree(p);
			NextFree(p) = new;
		}
		if (prev)
			NextFree(prev) = NextFree(p);
		else
			_empty = NextFree(p);
		return p;
	}
	if (grow(len) == 0)
		break;
    }
    assert(ntries != 2);
    return NULL;
}
//--------------------
static void * memcpy(void *s1, const void *s2, size_t n)
{
    char *p1 = s1;
    const char *p2 = s2;
    
    if (n) {
        n++;
	while (--n > 0) {
	  *p1++ = *p2++;
	}
    }
    return s1;
}

//---------------------
void * realloc(void *oldp, size_t size)
{
    register char *prev, *p, *next, *new;
    char *old = oldp;
    register size_t len, n;

    if (old == 0)
        return malloc(size);
    if (size == 0) {
	free(old);
	return NULL;
    }
    len = Align(size, PTRSIZE) + PTRSIZE;
    next = NextSlot(old);
    n = (int)(next - old);	// old length 

    // extend old if there is any free space just behind it
    for (prev = 0, p = _empty; p != 0; prev = p, p = NextFree(p)) {
	if (p > next)
		break;
	if (p == next) {	// 'next' is a free slot: merge 
		NextSlot(old) = NextSlot(p);
		if (prev)
			NextFree(prev) = NextFree(p);
		else
			_empty = NextFree(p);
		next = NextSlot(old);
		break;
	}
    }
    new = old + len;

    // Can we use the old, possibly extended slot?
    if (new <= next && new >= old) {	// it does fit 
        if (new + PTRSIZE < next) {	// too big, so split 
		/* + PTRSIZE avoids tiny slots on free list */
		NextSlot(new) = next;
		NextSlot(old) = new;
		free(new);
	}
	return old;
    }
    if ((new = malloc(size)) == NULL)	// it didn't fit 
	return NULL;
    memcpy(new, old, n);			// n < size 
    free(old);
    return new;
}


//---------------------
void free(void *ptr)
{
    register char *prev, *next;
    char *p = ptr;

    if (p == 0)
	return;

    assert(NextSlot(p) > p);
    for (prev = 0, next = _empty; next != 0; prev = next, next = NextFree(next))
	if (p < next)
		break;
    NextFree(p) = next;
    if (prev)
	NextFree(prev) = p;
    else
	_empty = p;
    if (next) {
	assert(NextSlot(p) <= next);
	if (NextSlot(p) == next) {	// merge p and next 
		NextSlot(p) = NextSlot(next);
		NextFree(p) = NextFree(next);
	}
    }
    if (prev) {
	assert(NextSlot(prev) <= p);
	if (NextSlot(prev) == p) {	// merge prev and p 
		NextSlot(prev) = NextSlot(p);
		NextFree(prev) = NextFree(p);
	}
    }
}

#include <l4all.h>
#include <l4/l4io.h>


#define CHR(i)  ((i%128<'!')?('.'):((i%128>'}')?('~'):(i%128)))


void mtest(int  sz)
{
    char *p;
    char *q;
    int  i;
    l4printf("mtest(%d)  ", sz);
    p = malloc(sz);
    for (i=0; i<sz; i++) p[i]=CHR(i);

    for (i=0; i<sz; i++) 
        if(p[i] != CHR(i))
	  l4printf("NG<i=%d>  ", i);

    free(p);
    l4printf("\n", sz);
}



int main( )
{
  int  i, j, sum = 0;
  char *p, *q;


  for (i = 1; i<40000; i+=1024)
    {
      mtest(i);
    }
  return 0;
}
