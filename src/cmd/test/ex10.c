/*****************************************************************
 *       Active Object test                                      *
 *       (C) KM 2009                                             *
 *****************************************************************/

#include  <l4all.h>
#include  <lp49/lp49.h>
#include  <lp49/l_actobj.h>

#define   _DBGFLG  0
#define   DBGPRN  if(_DBGFLG)print
#define   PRN   print

#include  <u.h>
#include  <libc.h>
#include  <lp49/lp49service.h>

extern L_thcb* L_thcbtbl[1024];

#define MILISEC 1000
#define STKSIZE 8196
//#define STKSIZE 100000000

typedef struct{
    L_thcb  _a;
    char  zz[0X100000];
} Aobj;

void foo(void *self, int m)
{
    char  buf[20];
    snprint(buf, sizeof buf, "Foo-%d", m);
    l_thread_setname(buf);
    l_post_ready(m);
    sleep(10000);

    for(;;){
      sleep(1000+100*m);
      print("<%d>  ", m);
    }
}

int main(int argc, char **argv)
{
    int  i;
    int  num = 10;
    L_thcb *thcb;
    Aobj   *ap; 
    L_msgtag   mtag;

    if (argc >= 2)  num = atoi(argv[1]);
    else  exits(0);

    l_init();
    
    print("<><><> EX10 [%X]<%d> <><><> \n", L4_Myself(), num);

    for(i=0; i<num; i++){
       ap = (Aobj*)malloc(sizeof(Aobj));
       if (ap == nil) {
	   print("Out of memory <%d> \n", i);
	   exits(0);
       }
	   
       thcb  = l_thread_create_args(foo, STKSIZE, ap, 1, i);
       l_yield(thcb);
       mtag = l_wait_ready(thcb);
       print(" Ready[%d] ", MLABEL(mtag));
       l_thcb_ls(thcb);
       sleep(200);
    }
    print("<><><>  Bye-bye [%X] <><><> \n", L4_Myself());
    sleep(1000000);
    print("---------------------------------------------\n");
    exits(0); 
    return  0;
}


