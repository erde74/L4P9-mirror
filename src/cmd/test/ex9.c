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
#define STKSIZE 1000

char  *txt[4][3] =
  { {0, 0, 0},
    {0, "Iro Ha Nihoedo", "Chiri Nuru Wo"},
    {0, "abcdefgh", "ijklmnop"},
    {0, "To be or not to be", "that is a question"},
  };

typedef struct Foo1{
    L_thcb  _a;
    int    u, v;
    char   name[16];
} Foo1;

typedef struct Bar1{
    L_thcb  _a;
    int   u, v;
    char  name[16];
} Bar1;

typedef  struct Abc{
    int  a;
    int  b;
    int  c;
} Abc;


Foo1  *foo1_obj;
Bar1   *bar1_obj;
L_thcb  *gamma1_obj;

int  dummy()
{  int  i;
  int  x = 0, y = 0;
  for(i=0; i<1000;i++) x++;
  for(i=0; i<1000;i++) y++;
  return x+y;
}

void foo1(Foo1 *self, void *arg1, void *arg2)
{
    int  x, y;
    Abc    z;
    int    zlen;
    L4_ThreadId_t  src;
    L_msgtag  msgtag;
    int       mlabel;


    l_thread_setname("foo1");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    strcpy(self->name, "Foo1");
    self->u = 333; self->v = 555;
    print("---- fooS(%d %d)  [%x] ----\n",
	  (int)arg1, (int)arg2, L4_Myself());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);
    for(;;){
        msgtag = l_recv0(nil, INF, nil);
	mlabel = MLABEL(msgtag);
	//print("  -----------------\n");
	zlen = sizeof(z);
	l_getarg(0, "i2s1", &x, &y, &z, &zlen );
        print("  fooS-[%d]<%d,  %d,  %d:%d:%d>\n",
	      mlabel, x, y, z.a, z.b, z.c);
	l_putarg(0, mlabel, "i2s1", x*10, y*10, &z, sizeof(z));
	l_send0((L_thcb*)bar1_obj, INF, 0);
    }
}

void bar1(Bar1 *self, void *arg1, void *arg2)
{
    char  buf[64];
    int   u, v, zlen;
    Abc   z;
    L4_ThreadId_t  src;
    L_msgtag   msgtag;
    int        mlabel;

    l_thread_setname("bar1");
    strcpy(self->name, "bar1");
    self->u = 777;
    self->v = 888;

    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    print("---- barS-(%d %d)  [%x]<%d> ----\n",
	  (int)arg1, (int)arg2, L4_Myself(), l_stkmargin());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);

    for(;;){
        msgtag = l_recv0(nil, INF, nil);
	mlabel = MLABEL(msgtag);
	//self->u = 777; 
	dummy();
	print("");
	zlen = sizeof(z);
	l_getarg(0, "i2s1", &u, &v, &z, &zlen);

	print("  barS-[%d]{%d,  %d,  %d:%d:%d}    \n", 
	      mlabel, u, v, z.a, z.b, z.c);
    }
}




int main(int argc, char **argv)
{
    int  i, j;
    Abc  z;
    int  zlen;
    L_mbuf  *mbuf;

    l_init();

    print("<><><> Synchronous messaging [%X] <><><> \n", L4_Myself());

    sleep(500);
    bar1_obj = (Bar1*)malloc(sizeof(Bar1));
    bar1_obj  = (Bar1*)l_thread_create_args(bar1, 
					  STKSIZE, bar1_obj, 2, 30, 40 );
    sleep(500);
    foo1_obj = (Foo1*)malloc(sizeof(Foo1));
    foo1_obj = (Foo1*)l_thread_create_args(foo1, 
					  2000, foo1_obj, 2, 10, 20);
    sleep(500);
    for (i = 1; i<4; i++) {
	    for (j=1; j<3; j++) {
	        print("<%d %d>  \n", i, j);
		z.a = i;
		z.b = j;
		z.c = i+j;
	        l_putarg(0, 10*i+j, "i2s1", i, j, &z, sizeof(z));
		l_send0((L_thcb*)foo1_obj, INF, 0);
		sleep(1000);
	    }
	    //L4_Sleep(L4_TimePeriod(500*MILISEC));
    }


    sleep(2000);
    l_thread_killall();
    print("<><><>  Bye-bye [%X] <><><> \n", L4_Myself());
    exits(0); 
    return  0;
}


