//%%%%%%%%%%%%%  devsd-test2.c %%%%%%%%%%%%
  
#include <l4all.h>
#include <l4/l4io.h>

#include  "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../port/error.h"
#include "../port/sd.h"

static int  _DBGFLG = 1;  
#include  <l4/_dbgpr.h>


#define snprint  l4snprintf

extern void p9_register_irq_handler(int irq, void (*func)(Ureg*, void*),
				    void* arg, char *name);
extern void p9_unregister_irq_handler(int irq);
extern L4_ThreadId_t p9_irq_init();

static char  data[2048] = 
  "       0 00 OO  9_9)  @_@)       \n"  
  "      0     __                   \n"
  "    _[]----|##|__                \n"
  "   ()______|__|__|               \n"
  "   ##oo ()()() 0 0               \n"
  "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n" ;

extern Dev  sddevtab ;

static char  buf [2048];
//char *names[2] = {"ctl", 0};
void eve_procsetup();

void devsd_test()
{
  int   i;
  Chan  *chan, *chan2;
  Chan  *c0;

  DBGBRK("<><><><><> DEVSD TEST START <><><><>\n");
  for (i = 0; i<24; i++)  l4printf("\n");

  //  p9_irq_init();
  //  server_init();
  eve_procsetup();

  DBGBRK("====== RESET ======\n");
  sddevtab.reset();

  DBGBRK("====== ATTACH ======\n");
  c0 = sddevtab.attach("sdC0");

  DBGBRK("====== OPEN ======\n");
  chan = (Chan*) malloc(sizeof(Chan));
  chan->qid.path = 'C' << 20 | 0 << 12 | 0 << 4 | 4 /*Qctl*/ ; 
  //------ sdC0: dev='C', unit=0, partition=...   -----
  chan->qid.vers = 0;
  chan->qid.type = QTFILE;
  chan->_ref.ref = 1;
  chan->type = 'S'; //?

  DBGBRK("====== READ CTL ======\n");
  sddevtab.read(chan, buf, 256, 0);
  l4printf_b("%s \n", buf);

  DBGBRK("====== WRITE ======\n");
  chan2 = (Chan*) malloc(sizeof(Chan));
  chan2->qid.path = 'C' << 20 | 0 << 12 | 1 << 4 | 6 /*Qctl*/ ; 
  //------ sdC0: dev='C', unit=0, partition=1   -----
  chan2->qid.vers = 0;
  chan2->qid.type = QTFILE;
  chan2->_ref.ref = 1;
  chan2->type = 'S'; //?

  sddevtab.write(chan2, data, 256, 0);
  l4printf_g("---- Written data ----\n");  
  l4printf_g("%s \n", data);  

  DBGBRK("====== READ CTL ======\n");

  sddevtab.read(chan2, buf, 256, 0);
  l4printf_b("---- Read data ----\n");  
  l4printf_b("%s \n", buf);

  DBGBRK("<<<<< sdata <<<<<");
}

/*--- For reference ----------------------------
struct Dev {
  int     dc;
  char*   name;
  void    (*reset)(void);
  void    (*init)(void);
  void    (*shutdown)(void);
  Chan*   (*attach)(char*);
  Walkqid*(*walk)(Chan*, Chan*, char**, int);
  int     (*stat)(Chan*, uchar*, int);
  Chan*   (*open)(Chan*, int);
  void    (*create)(Chan*, char*, int, ulong);
  void    (*close)(Chan*);
  long    (*read)(Chan*, void*, long, vlong);
  Block*  (*bread)(Chan*, long, ulong);
  long    (*write)(Chan*, void*, long, vlong);
  long    (*bwrite)(Chan*, Block*, ulong);
  void    (*remove)(Chan*);
  int     (*wstat)(Chan*, uchar*, int);
  void    (*power)(int);  // power mgt: power(1) => on, power (0) => off /
  int     (*config)(int, char*, DevConf*);        // returns nil on error
};
-------------------------------------------------------------*/
