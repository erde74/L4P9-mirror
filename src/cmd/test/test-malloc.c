#define size_t int
#define NULL   (void*)0   

extern int  _end;  
char   *_brkadrs = (char *)&_end;

void free(void *ptr);

//---------------------
int brk(char *addr)
{
  unsigned   reqbrk = (unsigned)addr; 
  unsigned   oldbrk = (unsigned)_brkadrs;
  _brkadrs = addr;
  //  l4printf("BRK[%x: %x -> %x] \n", reqbrk, oldbrk, _brkadrs); 
  return(0);
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


#undef	 DEBUG		/* check assertions */
#undef	 SLOWDEBUG	/* some extra test loops (requires DEBUG) */

#ifndef DEBUG
#define NDEBUG
#endif


#if 1 
#define  assert(expr)
#else 
#define assert(expr)    ((expr)? (void)0 : \
                               __bad_assertion("Assertion \"" #expr \
                                    "\" failed, file " __xstr(__FILE__) \
                                    ", line " __xstr(__LINE__) "\n"))
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


#include <stdio.h>

main( )
{
  int  i, j, k;
  char *p, *q;

  for (i = 1; i<100; i++)
    {
      p = malloc(i * 8);
      if (p == (char*)0)  exit(0);

      for(q = p, j=0; j<i*8-2; q++, j++) 
	*q = '#'; 

      *q = 0;
      printf("[%x] %s\n", p, p);
    }
}
