#include <u.h>
#include <libc.h>
#include <thread.h>


#if 1 //%%%% cmd/usb/lib/aux-l4.c =========================
 
// typedef struct Ref  Ref;
// struct Ref {
//   long ref;
// };
  
 
void incref(Ref *r)
{
  __asm__ __volatile__
    ("    lock            \n"
          "    incl  0(%%eax) \n"
     :
     : "a"(r)
     );
}
 
long decref(Ref *r)
{
  long  rval;
 
  __asm__ __volatile__
    ("    lock            \n"
          "    decl  0(%%eax) \n"
          "    jz    1f        \n"
          "    movl  $1, %%eax \n"
          "    jmp   2f        \n"
          "1:                  \n"
          "    movl  $0, %%eax \n"
          "2:                  \n"
 
     :  "=a" (rval)
     :  "a" (r)
     );
  return  rval;
}


#else //====================================================

static  void _xinc(long * _x)
{
#if 1 
  *_x = *_x + 1;  //%%%%%%%%%%%%
#else // not work

  __asm__ __volatile__("lock \n" 
		       "incl %%eax  \n"
		       : "=a" (_x) 
		       : "a" (_x));
#endif
}

static long _xdec(long * _x)
{
#if 1 //------------------
  *_x = *_x - 1;
  return *_x;

#else //------------------
  long  r;
  __asm__ __volatile__("lock \n"
                       "decl  %%eax  \n"
		       "jz    iszero  \n"
		       "movl  $1, %%eax \n"
		       "jmp   rtn  \n"
		       "iszero:   \n"
		       "movl  $0, %%eax  \n"
		       "rtn:  \n"

                       : "=a" (_x)
                       : "a" (_x));
#endif //--------------------
}

void
incref(Ref *r)
{
	_xinc(&r->ref);
}

long
decref(Ref *r)
{
	return _xdec(&r->ref);
}

#endif //=====================================================
