#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>
#include  <u.h>
#include  <libc.h>

static uvlong tt[32];

void dbg_time(int which, char *msg)  // 1st time: msg==nil
{
  uvlong  last, now, delta;
  uint  usec, msec, sec;

  if (which<0  || which>32) 
      return;
 
  if (msg) {
    now = L4_SystemClock().raw;  // usec in uvlong
    last = tt[which];
    delta = now - last;
    usec = (uint)(delta % 1000ULL);
    msec = (uint)((delta / 1000ULL) % 1000ULL);
    sec =  (uint)((delta / 1000000ULL) % 1000ULL);
    l4printf_b("%s<%d sec. %d msec. %d usec.> \n", msg, sec, msec, usec);
  }

  tt[which] = L4_SystemClock().raw;
  return ;
}

//----------------------------------------------------
void dbg_backtrace()
{
  typedef struct frame{
    struct frame *ebp;
    void   *ret;
  } frame;

  register void * _EBP_ asm("%ebp");

  frame  *ebp = _EBP_;
  l4printf_b(" Backtrace: ");
  while (ebp) {
    l4printf_b("0x%x ", ebp->ret);
    ebp = ebp->ebp;
  }
  l4printf_b("\n");
}

//--------------------------------------
void dbg_dump_mem(char *title, unsigned char *start, unsigned size)
{
  int  i, j, k;
  unsigned char c;
  char  buf[128];
  char  elem[32];

  l4printf_b("* dump mem <%s>\n", title);
  for(i=0; i<size; i+=16) {
    buf[0] = 0;
    for (j=i; j<i+16; j+=4) {
      if (j%4 == 0) strcat(buf, " ");

      for(k=3; k>=0; k--) {
        c = start[j+k];
        l4snprintf(elem, 32, "%.2x", c);
        strcat(buf, elem);
      }
    }

    strcat(buf, "  ");
    for (j=i; j<i+16; j++) {
      c = start[j];
      if ( c >= ' ' && c < '~')
        l4snprintf(elem, 32, "%c", c);
      else
        l4snprintf(elem, 32, ".");
      strcat(buf, elem);
    }
    l4printf_b("%s\n", buf);
  }
}

//----------------------------------
void dbg_prncpy(char *to, char *from, int len)
{
  int  i;
  char  ch;
  for(i = 0; i<len; i++) {
    ch = from[i];
    if (ch >= ' ' && ch <= '~')
      to[i] = ch;
    else
      to[i] = '^';
  }
  to[len] = 0;
}


//-------------------------------
char * dbg_qid(Qid qid)  // non-reentrant
{
   static char qidstr[80];
   union {
     uvlong  path;
     uint    u[2];
   } val;
   val.path = qid.path;
   sprint(qidstr, "<%x-%d-%x>", val.u[0], qid.vers, qid.type);
   return qidstr;
}

void dbg_dir(Dir*  dirp)
{
  if (dirp==nil) 
      l4printf_b("Dir:nil\n");
  else
    l4printf_b("Dir{t:%d d:%d Qid%s m:%x l:%d n:%s}\n",
	       dirp->type, dirp->dev,
	       dbg_qid(dirp->qid), dirp->mode, dirp->length, dirp->name);
}

//--------------------------------
unsigned long long dbg_cpuclock( )
{
    unsigned long long  val;
     __asm__ volatile("rdtsc" : "=A" (val));
    return  val;
}
