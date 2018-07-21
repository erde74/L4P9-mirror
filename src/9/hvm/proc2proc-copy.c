//%%%%%%% proc2proc-copy.c %%%%

#define  _DBGFLG  0
#include  <l4/_dbgpr.h>


/*---------------------------------------------

 p1 +------------+------++-----------------++-----
    |    a1      |  b1  || a1-a2 | a2+b1   ||
    +------------+------++-----------------++-----

 p2 +------+------+-----++-----------------++-----
    |  a2  |  b1  |a1-a2|| a1-a2 | a2+b1   ||
    +------+------+-----++-----------------++-----

    --------------------------------------------*/

extern char*  get_targetpage(int  tasknr, unsigned tgtadrs);
extern void *memcpy(void *s1, void *s2, int n);


int proc2proc_copy(int fromproc, unsigned fromadrs,
		   int toproc,   unsigned toadrs,
		   int size)
{
  int     sz = size;
  unsigned   p1, a1, b1, c1, d1;
  unsigned   p2, a2, b2, c2, d2;

  a1 = fromadrs & 0x0FFF;
  a2 = toadrs   & 0x0FFF;
  b1 = 4096 - a1;
  b2 = 4096 - a2;

  p1 = (unsigned)get_targetpage(fromproc, fromadrs);
  p2 = (unsigned)get_targetpage(toproc, toadrs);
  
  if (a1 >= a2) {
    if (sz <= b1)  b1 = sz;
    
    DBGBRK("p-p-copy-11 %x %x %d %d\n", 
	   (char*)(p2 + a2), (char*)(p1 + a1), b1, sz);
    memcpy((char*)(p2 + a2), (char*)(p1 + a1), b1);
    sz -= b1;
    if (sz == 0)  return size;

    while(1) {
      c1 = a1 - a2;
      d1 = b1 + a2; // 4k + a2 - a1

      if (sz <= c1)  c1 = sz;
      fromadrs += 4096;
      p1 = (unsigned)get_targetpage(fromproc, fromadrs);
      DBGBRK("p-p-copy-12 %x %x %d %d\n", 
	     (char*)(p2 + a2 + b1), (char*)(p1 + 0), c1, sz);
      memcpy((char*)(p2 + a2 + b1), (char*)(p1 + 0), c1);      

      sz -= c1;
      if (sz == 0)  return size;

      if (sz <= d1) d1 = sz;
      toadrs += 4096;
      p2 = (unsigned)get_targetpage(toproc, toadrs);
      DBGBRK("p-p-copy-13 %x %x %d %d\n", 
	     (char*)(p2 + 0), (char*)(p1 + c1), d1, sz);
      memcpy((char*)(p2 + 0), (char*)(p1 + c1), d1);

      sz -= d1;
      if (sz == 0) return size;
    }
  }
  else {  // a1 < a2
    if (sz <= b2)  b2 = sz;
    
    DBGBRK("p-p-copy-21 %x %x %d %d\n", 
	   (char*)(p2 + a2), (char*)(p1 + a1), b2, sz);
    memcpy((char*)(p2 + a2), (char*)(p1 + a1), b2);
    sz -= b2;
    if (sz == 0)  return size;

    while(1) {
      c2 = a2 - a1;
      d2 = a1 + b2; // c2 + d2 == 4k

      if (sz <= c2)  c2 = sz;
      toadrs += 4096;
      p2 = (unsigned)get_targetpage(toproc, toadrs);
      DBGBRK("p-p-copy-22 %x %x %d %d\n", 
	     (char*)(p2), (char*)(p1 + a1 + b2), c2, sz);
      memcpy((char*)(p2), (char*)(p1 + a1 + b2), c2);      
      sz -= c2;
      if (sz == 0)  return size;

      if (sz <= d2) d2 = sz;
      fromadrs += 4096;
      p1 = (unsigned)get_targetpage(fromproc, fromadrs);
      DBGBRK("p-p-copy-23 %x %x %d %d\n", 
	     (char*)(p2 + a2 - a1), (char*)(p1 + 0), d2, sz);
       memcpy((char*)(p2 + a2 - a1), (char*)(p1 + 0), d2);
      sz -= d2;
      if (sz == 0) return size;
    }
  }
}


//=============================================
/*           ap
 *    +------+------++-----------------++------+------
 *    |      |      ||                 ||      |
 *    +------+------++-----------------++------+------
 *      a1      b1                         c1
 */

static void* memset(void *ap, int c, int n)
{
  char *p;
  p = ap;
  while(n > 0) {
    *p++ = c;
    n--;
  }
  return ap;
}

void*  proc_memset(int procnr, void* ap, int value, int size)
{
  unsigned  from = (unsigned)ap;
  unsigned  p1;
  unsigned  a1 = from & 0x0FFF;
  unsigned  b1 = 4096 - a1;
  unsigned  c1;
  int  sz = size;
  
  if (sz <= b1) b1 = sz;

  p1 = (unsigned)get_targetpage(procnr, from);
  memset((char*)(p1+a1), value, b1);
  sz -= b1;
  if (sz <= 0) return ap;

  while(1) {
    if (sz <= 4096)  c1 = sz;
    else             c1 = 4096;
    from += 4096;
    p1 = (unsigned)get_targetpage(procnr, from);
    memset((char*)p1, value, c1);
    sz -= c1;
    if (sz <= 0)
      break;
  }
  return  ap;
}
