//%%%%%%%%%%%%%   L4 * Plan9 ==> LP49 
  
#include <l4all.h>
#include <l4/l4io.h>

#include  "u.h"
// #include  <libc.h>
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../port/error.h"
#include "../port/sd.h"

extern int  DBGFLG ;  // defined in _relief.c
#define  DBGPRN  if(DBGFLG) l4printf_g
#define  DBGBRK  l4printgetc

#define snprint  l4snprintf

extern void p9_register_irq_handler(int irq, void (*func)(Ureg*, void*),
				    void* arg, char *name);
extern void p9_unregister_irq_handler(int irq);
extern L4_ThreadId_t p9_irq_init();

static SDunit unit_tbl;
static char  data[2048] = 
  "       0 00 OO  9_9)  @_@)       \n"  
  "      0     __                   \n"
  "    _[]----|##|__                \n"
  "   ()______|__|__|               \n"
  "   ##oo ()()() 0 0               \n"
  "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n" ;

extern SDifc sdataifc;

Dev sddevtab;
Dev rootdevtab; // devroot.c
// These are referenced from devtab[] in _setup.c 

char * eve;

static char  buf [2048];

int main()
{
  int  i, rc,  n;
  SDev  *sdev;
  SDunit *unit = &unit_tbl;

  DBGPRN("<><><><><> SDATA TEST START <><><><>\n");
  for (i = 0; i<24; i++)
    l4printf("\n");

  p9_irq_init();

  server_init();

  DBGBRK("====== Legacy ======\n");

  sdev = sdataifc.legacy(0x1F0, 14);

  DBGBRK("===== Legacy=%X  =>  Enable =====\n", sdev);

  rc = sdataifc.enable(sdev);

  DBGBRK("===== Enable%X  =>  BIOWrite  =====\n", rc);

  l4printf_g("%s  \n", data);  
  DBGBRK("^ Data to be written \n");
  unit->dev = sdev;
  unit->subno = 0;
  unit->secsize = 512;

  n = sdataifc.bio(unit, 0, 1, data, 2, 4);

  DBGBRK("===== BIOWrite=%X  =>  BIORead =====\n", n);

  unit->dev = sdev;
  unit->subno = 0;
  unit->secsize = 512;

  n = sdataifc.bio(unit, 0, 0, buf, 2, 4);
  l4printf_g("%s  \n", buf);

  DBGBRK("===== BIORead=%X =====\n", n);

  DBGBRK("<<<<< sdata <<<<<");
  return 0;
}
