//%%%%%%%%%%%%%   L4 * Plan9 ==> LP49 
  
#include <l4all.h>
#include <l4/l4io.h>

#include  "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../port/error.h"
#include "../port/sd.h"

int  DBGFLG  = 1;  // defined in _relief.c
#define  DBGPRN  if(DBGFLG) l4printf_g
#define  DBGBRK  l4printgetc
#define snprint  l4snprintf

Conf   conf = { .ialloc  4*1024*1024 };
L4_ThreadId_t  server;

void ilock(Lock *l)   // port/taslock.c
{    DBGPRN("! %s  \n", __FUNCTION__); }

void iunlock(Lock *l)
{    DBGPRN("! %s  \n", __FUNCTION__); }

int lock(Lock *l)
{  DBGPRN("! lock \n");  return 0;}

void unlock(Lock *l)
{  DBGPRN("! unlock \n");}

void rlock(RWlock *q)  // port/qlock.c
{   DBGPRN("! %s  \n", __FUNCTION__);}

void runlock(RWlock *q)
{   DBGPRN("! %s  \n", __FUNCTION__);}

void wlock(RWlock *q)
{  DBGPRN("! %s  \n", __FUNCTION__);}

void wunlock(RWlock *q)
{  DBGPRN("! %s  \n", __FUNCTION__);}

void panic(char *fmt, ...)
{  DBGBRK("! %s: %s \n", __FUNCTION__, fmt); }

void exit(int x) {  }

int  return0(void* _x)
{  return 0; }

void tsleep(Rendez *r, int (*func)(void*), void *arg, ulong micros)
{
  L4_ThreadId_t  from;
  L4_MsgTag_t   tag;

  DBGPRN("> TSLEEP  \n");
  if ((*func)(arg) == 0) {
    r->p = (Proc*)L4_Myself().raw;  //Type
    tag = L4_Wait_Timeout(L4_TimePeriod(micros), &from );
  }
  DBGBRK("< TSLEEP \n");
}

extern void xinit();

int main()
{
    char *pp[128];
    int  i;

    DBGBRK("> malloc-test \n");
    xinit();

    DBGBRK("< xinit \n");

    for (i=0; i<32; i++) {
      pp[i] = malloc(i*10+8);
      //      DBGBRK("< malloc %X \n", pp[i]);
      strcpy(pp[i], "(^_^)");
    }
    
    DBGBRK("-- Hit any key to free memories ----\n");
    for (i=0; i<32; i++){
      l4printf("[%X] %s \n", pp[i], pp[i]);
      free(pp[i]);
    }
}

