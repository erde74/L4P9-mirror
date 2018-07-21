#include <u.h>
#include <libc.h>

//int l4printf_r(const char *fmt, ...);

void _BACKTRACE( )
{
    register void * _EBP_ asm("%ebp");
    typedef struct frame{
      struct frame *ebp;
      void   *ret;
    } frame;
 
    frame  *ebp = _EBP_;
    print(" Backtrace: ");
    while (ebp) {
        print("0x%x ", ebp->ret);
	ebp = ebp->ebp;
    }
    print("\n");
}

