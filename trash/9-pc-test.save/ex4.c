/*
 * A user-level program.
 */
#include <u.h>
#include <libc.h>

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

int l4thread_create_start(L4_ThreadId_t *newtid, uint *stackbase, int stacksize,
                          void (*func)(void *arg0, void *arg1), void* arg0, void *arg1);
int  l4send_nint(L4_ThreadId_t dest, int label, int num, ...);
int  l4receive_nint(L4_ThreadId_t *from, int *label, int num, ...);
int  l4send_str_nint(L4_ThreadId_t dest, int label, char *buf, int len, int num, ...);
int  l4receive_str_nint(L4_ThreadId_t *from, int *label, char *buf, int len, int num, ...);


L4_ThreadId_t  Alice, Bob;

char  buf1[128];
char  buf2[1024];


void alice(int x, int y)
{
  int  i, rc; 

  L4_Yield();
  print("Alice(%d %d)\n",  x, y); 
  for(i=0; i<10; i++) {
    L4_Sleep(L4_TimePeriod(1000UL));
    print("Alice sends <%d %d %d %d>  to %x \n", x, y, y*10, y*100, Bob.raw); 

    //rc = l4send_nint(Bob, x++, 3, y, y*10, y*100);
    rc = l4send_str_nint(Bob, x++, buf1, strlen(buf1), 3, y, y*10, y*100);
    if (rc<0) print("send-err\n");
    y++;
  }
  l4thread_kill(L4_Myself());
  L4_Sleep(L4_Never);
}

void bob(int x, int y)
{
  L4_ThreadId_t  tid;
  int  label;
  int  data1, data2, data3; 
  int  i, rc;

  print("Bob(%d %d)\n",  x, y); 

  for(i=0; i<20; i++) {
    //rc = l4receive_str_nint(&tid, &label, 3, &data1, &data2, &data3);
    rc = l4receive_str_nint(&tid, &label, buf2, sizeof(buf2), 3, &data1, &data2, &data3);
    if (rc < 0) 
      print("receive err\n");
    else
      print("Bob received <%d %d %d %d %s>  from %x \n", 
	    label, data1, data2, data3, buf2, tid.raw); 
  }
  L4_Sleep(L4_Never);
}



int main(int argc, char *argv[])
{
  L4_ThreadId_t  myself = L4_Myself();

#if 0
  va_list arg;
  ARGBEGIN{
  }ARGEND ;
  fprint(2, "%s tid=%x: To be or not to be  \n", argv0, myself.raw);
  //  va_start(arg, fmt);
  //  vfprint(2, fmt, arg);
  //  va_end(arg);
  //  fprint(2, ": %r\n");
#else 
  l4printf_c (62, "=== Nobody understands quantum theory.(R. Feynman) [%X] ===\n", myself.raw);

  strcpy(buf1, "To be or not to be, that is a question.");

  l4thread_create_start(&Bob,   0,  1024, bob, 10, 20);
  l4thread_create_start(&Alice, 0,  1024, alice, 777, 10);


#endif 

  L4_Sleep(L4_TimePeriod(100000UL));
  l4thread_killall();
  exits(0);

  L4_KDB_Enter ("Hello debugger!");
};
