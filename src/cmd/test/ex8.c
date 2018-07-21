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

#if 1 //======================================================
/******************************************************
 *     l_actobj Synchronous message example           *
 ******************************************************/

/*********************************************************************
 *      main-thread       alpha1            beta1     	   gamma1
 *    []---------+      []---------+       []--------+     []--------+
 *     |         |       |         |        |        |      |        |
 *     |       --------->|         |        |        |      |	     |
 *     |         | "i2"  |        --------->|        |      |	     |
 *     |         |       |         | "i2s1" |       ------->|	     |
 *     |         |       |         |        |        |"i2s1"|	     |
 *     +---------+       +---------+        +--------+      +--------+
 *********************************************************************/

#define MILISEC 1000
#define STKSIZE 1000

char  *txt[4][3] =
  { {0, 0, 0},
    {0, "Iro Ha Nihoedo", "Chiri Nuru Wo"},
    {0, "abcdefgh", "ijklmnop"},
    {0, "To be or not to be", "that is a question"},
  };

typedef struct Alpha1{
    L_thcb  _a;
    int    u, v;
    char   name[16];
} Alpha1;

typedef struct Beta1{
    L_thcb  _a;
    int   u, v;
    char  name[16];
} Beta1;

Alpha1  *alpha1_obj;
Beta1   *beta1_obj;
L_thcb  *gamma1_obj;

void alpha1(Alpha1 *self, void *arg1, void *arg2)
{
    int  x, y;
    L4_ThreadId_t  src;
    //    Alpha1  *self;
    L_msgtag  msgtag;
    int       mlabel;

    l_thread_setname("alpha1");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    self = L_MYOBJ(Alpha1*);
    strcpy(self->name, "Alpha1");
    self->u = 333; self->v = 555;
    print("---- alphaS(%d %d)  [%x] ----\n",
	  (int)arg1, (int)arg2, L4_Myself());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);
    for(;;){
        msgtag = l_recv0(&src, INF, nil);
	mlabel = MLABEL(msgtag);
	l_getarg(0, "i2", &x, &y);
        print("alphaS-[%d]<%d %d>\n",mlabel, x, y);
	l_putarg(0, mlabel, "i2s1", x*10, y*10, txt[x][y], strlen(txt[x][y]));
	l_send0((L_thcb*)beta1_obj, INF, 0);
    }
}

void beta1(Beta1 *self, void *arg1, void *arg2)
{
    char  buf[64];
    int   u, v, len;
    L4_ThreadId_t  src;
    //    Beta1  *self = L_MYOBJ(Beta1*);
    L_msgtag  msgtag;
    int        mlabel;

    l_thread_setname("beta1");
    strcpy(self->name, "beta1");
    self->u = 777;
    self->v = 888;

    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    print("---- betaS-(%d %d)  [%x]<%d> ----\n",
	  (int)arg1, (int)arg2, L4_Myself(), l_stkmargin());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);
    for(;;){
        msgtag = l_recv0(&src, INF, nil);
	mlabel = MLABEL(msgtag);
	len = 64;
	l_getarg(0, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("  betaS-[%d]{%d %d '%s'}    \n", mlabel, u, v, buf);
	l_putarg(0, mlabel, "i2s1", u*10, v*10, buf, len);
	l_send0(gamma1_obj, INF, 0);
    }
}

void gamma1(void *self, int x, int y, int z )
{
    char  buf[64];
    int   u, v, len;
    L4_ThreadId_t  src;
    L_msgtag  msgtag;
    int       mlabel;

    print("---- gammaS-(%d %d %d) [%x] ----\n", x, y, z, L4_Myself().raw);
    l_thread_setname("gamma1");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    for(;;){
        msgtag = l_recv0(&src, INF, nil);
	mlabel = MLABEL(msgtag);
	len = 64;
	l_getarg(0, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("    gammaS-[%d]{%d %d '%s'}\n", mlabel, u, v, buf);
    }
    L4_Sleep(L4_TimePeriod(200*MILISEC));
}

//------------------------------------
typedef struct Alpha2  Alpha2;
typedef struct Beta2   Beta2;

struct Alpha2{
    L_thcb  _a;
    int    u, v;
    char   name[16];
};

struct Beta2{
    L_thcb  _a;
    int   u, v;
    char  name[16];
};

Alpha2  *alpha2_obj;
Beta2   *beta2_obj;
L_thcb  *gamma2_obj;


void alpha2(Alpha2 *self, void *arg1, void *arg2)
{
    int  x, y;
    L_mbuf *mbuf;
    //    Alpha2  *self;
    L_MR0  mr0;
    int    mlabel;

    l_thread_setname("alpha2");
    L4_LoadMR(0, 0);
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    self = L_MYOBJ(Alpha2*);
    strcpy(self->name, "Alpha2");
    self->u = 333; self->v = 555;
    print("\n---- alphaA(%d %d)  [%x] ----\n",
	  (int)arg1, (int)arg2, L4_Myself());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);

    for(;;){
        l_arecv0(nil, INF, &mbuf);
	if (mbuf==nil) print("mbuf-nil\n");
	mr0.raw = mbuf->MRs.msg[0];
	mlabel = mr0.X.mlabel;
	l_getarg(mbuf, "i2", &x, &y);
        print("alphaA-[%d]<%d %d>\n", mlabel, x, y);
	l_putarg(mbuf, mlabel, "i2s1", x*10, y*10, txt[x][y], strlen(txt[x][y]));
	l_asend0((L_thcb*)beta2_obj, INF, mbuf);
    }
}

void beta2(Beta2 *self, void *arg1, void *arg2)
{
    char  buf[64];
    int   u, v, len;
    //    Beta2  *self = L_MYOBJ(Beta2*);
    L_mbuf  *mbuf;
    L_MR0  mr0;
    int    mlabel;

    l_thread_setname("beta2");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    strcpy(self->name, "beta2");
    self->u = 777;
    self->v = 888;
    print("\n---- betaA(%d %d)  [%x]<%d> ----\n",
	  (int)arg1, (int)arg2, L4_Myself(), l_stkmargin());
    print("myobj = {%s, %d, %d}\n", self->name, self->u, self->v);

    for(;;){
	len = 64;
        L4_LoadMR(0, 0);
        l_arecv0(nil, INF, &mbuf);
	mr0.raw = mbuf->MRs.raw[0];
	mlabel = mr0.X.mlabel;
	l_getarg(mbuf, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("  betaA-[%d]{%d %d '%s'}    \n", mlabel, u, v, buf);
	l_putarg(mbuf, mlabel, "i2s1", u*10, v*10, buf, len);
	l_asend0(gamma2_obj, INF, mbuf);
    }
}

void gamma2(void *self, int x, int y, int z )
{
    char  buf[64];
    int   u, v, len;
    L_mbuf  *mbuf;
    L_MR0  mr0;
    int    mlabel;

    l_thread_setname("gamma2");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    print("---- gammaA(%d %d %d) [%x] ----\n", x, y, z, L4_Myself().raw);
    L4_LoadMR(0, 0);

    for(;;){
	len = 64;
        l_arecv0(nil, INF, &mbuf);
	mr0.raw = mbuf->MRs.raw[0];
	mlabel = mr0.X.mlabel;
	l_getarg(mbuf, "i2s1", &u, &v, buf, &len);
	buf[len] = 0;
	print("    gammaA-[%d]{%d %d '%s'}\n", mlabel, u, v, buf);
	l_freembuf(mbuf);
    }
    L4_Sleep(L4_TimePeriod(200*MILISEC));
}


//------------------------------------
L_thcb *delta_obj;
void delta(void *self, int x, int y, int z )
{
    char  buf[64];
    int   i;
    L4_ThreadId_t  src;
    L_msgtag  msgtag;
    int       mlabel;

    print("---- deltaS-(%d %d %d) [%x] ----\n", x, y, z, L4_Myself().raw);
    l_thread_setname("delta1");
    L4_LoadMR(0, 0);  L4_LoadBR(0, 0);
    for(i=0; i<8; i++){
        msgtag = l_recv0(&src, 40000, nil);
	if (L4_IpcFailed(msgtag)){
	  print ("[%d]Delta<%d> \n", i, L4_ErrorCode());
	}
    }
    L4_Sleep(L4_TimePeriod(200*MILISEC));
}




int main(int argc, char **argv)
{
    int  i, j;
    L_mbuf  *mbuf;
    int  which = 1;

    l_init();

    delta_obj = l_thread_create_args(delta, 
				     STKSIZE, nil, 3, 100, 200, 300);

    sleep(200000);
#if 0
    //    print("< %d %s %s>\n", argc, argv[0], argv[1]); 
    if (argc>=2){
      if (strcmp(argv[1], "s")==0) which = 1;
      else if (strcmp(argv[1], "a")==0) which = 2;
    }

    L4_Sleep(L4_TimePeriod(5000*MILISEC));

    if (which == 1){
        print("<><><> Synchronous messaging [%X] <><><> \n", L4_Myself());
        gamma1_obj = l_thread_create_args(gamma1, 
					  STKSIZE, nil, 3, 111, 222, 333);
	L4_Sleep(L4_TimePeriod(500*MILISEC));
	beta1_obj = (Beta1*)malloc(sizeof(Beta1));
	beta1_obj  = (Beta1*)l_thread_create_args(beta1, 
					  STKSIZE, beta1_obj, 2, 30, 40 );
	L4_Sleep(L4_TimePeriod(500*MILISEC));
	alpha1_obj = (Alpha1*)malloc(sizeof(Alpha1));
	alpha1_obj = (Alpha1*)l_thread_create_args(alpha1, 
					  2000, alpha1_obj, 2, 10, 20);
	L4_Sleep(L4_TimePeriod(10*MILISEC));
        for (i = 1; i<4; i++) {
	    for (j=1; j<3; j++) {
	        print("<%d %d>  ", i, j);
	        l_putarg(0, 10*i+j, "i2", i, j);
		l_send0((L_thcb*)alpha1_obj, INF, 0);
		L4_Sleep(L4_TimePeriod(500*MILISEC));
	    }
	    //L4_Sleep(L4_TimePeriod(500*MILISEC));
	}
    }
    else{
        print("<><><> Asynchronous messaging [%X] <><><> \n", L4_Myself());
        gamma2_obj = l_thread_create_args(gamma2, 
					  STKSIZE, nil, 3, 111, 222, 333);
	L4_Sleep(L4_TimePeriod(500*MILISEC));
	beta2_obj = (Beta2*)malloc(sizeof(Beta2));
	beta2_obj  = (Beta2*)l_thread_create_args(beta2, 
					  STKSIZE, beta2_obj, 2, 30, 40 );
	L4_Sleep(L4_TimePeriod(500*MILISEC));
	alpha2_obj = (Alpha2*)malloc(sizeof(Alpha2));
	alpha2_obj = (Alpha2*)l_thread_create_args(alpha2, 
					  STKSIZE, alpha2_obj, 2, 10, 20);
	L4_Sleep(L4_TimePeriod(10*MILISEC));
	for (i = 1; i<4; i++) {
	    for (j=1; j<3; j++) {
	      print("<%d %d>  ", i, j);
	      mbuf = l_allocmbuf();
	      l_putarg(mbuf, 10*i+j, "i2", i, j);
	      l_asend0((L_thcb*)alpha2_obj, INF, mbuf);
	      L4_Sleep(L4_TimePeriod(500*MILISEC));
	    }
	    //L4_Sleep(L4_TimePeriod(500*MILISEC));
	  }
      }
    //    L4_KDB_Enter("");
#endif

    L4_Sleep(L4_TimePeriod(50*MILISEC));
    l_thread_killall();
    print("<><><>  Bye-bye [%X] <><><> \n", L4_Myself());
    exits(0); 
    return  0;
}

#endif //===============================================
