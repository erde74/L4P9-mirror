typedef  unsigned int  uint;

typedef struct  Atcb{
    int    *stkbase;
    int    *stktop;
    char    state;
    char    malloced;
    char    _x;
    char    _y;
}  Atcb;


typedef struct Clerkjob  Clerkjob;
  struct Clerkjob {
#if 1
    int   x;
    int   y, z;
#else
       L4_Fpage_t    mappee; // mapped to
       L4_Fpage_t    mapper; // mapped from
       Clerkjob_t    *next;
       L4_ThreadId_t  tid;   // clerk thread
       L4_ThreadId_t  client;
       Proc          *pcb;
       unsigned       pattern;
       ulong          args[8];
       char           strbuf[512];
#endif
   };
 



typedef struct Foo {
    Atcb       _a;
    Clerkjob   _b;
    int       stk[1000];
} Foo ;

Foo  foo = { ._a = {.stkbase = & foo.stk[1], 
                   .stktop  = & foo.stk[998] }
};


#define ATHREAD_MEM(name, typename, stksize)   \
  struct {                                     \
    Atcb       _a;                             \
    typename   _b;                             \
    int        stk[stksize];                   \
  }                                            \
  name  =                                      \
  { ._a = {.stkbase = & name.stk[1],           \
           .stktop = & name.stk[stksize-4] }}

    
ATHREAD_MEM(bar, Clerkjob, 1000); 


void*  thread_malloc(int dsize, int stksize, void **  stkbase)
{
  void  *p, *q;
  Atcb  *ap:

  int  dsize2, stksize2;
  dsize2 = ((dsize + 15)/16) * 16 ;
  stksize2 = ((stksize + 15)/16) * 16;
  totoalsize = sizeof(Atcb) + dsize2 + stksize2;
  // Shall we alignegn page bounrary ? -- Future task

  p = malloc(totalsize);
  ap = (Atcb*)p;
  ap->stkbase = (void*)((int)p + dsize2 + 8);
  ap->stktop = (void*)((int)p + dsize2 + stksize2 - 8);
 
  return  (void*)p;
}




