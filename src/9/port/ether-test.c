/*
 * (c) YS	NII
 */
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

#define  _DBGFLG 1
#include  <l4/_dbgpr.h>

#define snprint  l4snprintf

extern void p9_register_irq_handler(int irq, void (*func)(Ureg*, void*),
				    void* arg, char *name);
extern void p9_unregister_irq_handler(int irq);
extern L4_ThreadId_t p9_irq_init();

extern Conf conf;

static SDunit unit_tbl;
#if 0
static char  data[2048] = 
  "       0 00 OO  9_9)  @_@)       \n"  
  "      0     __                   \n"
  "    _[]----|##|__                \n"
  "   ()______|__|__|               \n"
  "   ##oo ()()() 0 0               \n"
  "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n" ;
#else
struct ether_format{
  char dst_mac[6];
  char src_mac[6];
  char type[2];

  struct arp_format{
	char hw_type[2];
	char proto[2];
	char hw_size;
	char proto_size;
	char opcode[2];
	char src_mac[6];
	char src_ip[4];
	char dst_mac[6];
	char dst_ip[4];
  }arp_data;

  char trailer[100];
  int size;
}data ={
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	  /* dst_mac		*/
  {0x00, 0x0C, 0x29, 0x7F, 0x7C, 0x06},	  /* src_mac		*/
  {0x08, 0x06},							  /* type (ARP)		*/
  {											/* arp_data			*/
	{0x00, 0x01},							  /* hw_type (Ethernet) */
	{0x08, 0x00},							  /* proto   (IP)		*/
	6,										  /* hw_size			*/
	4,										  /* proto_size			*/
	{0x00, 0x01},							  /* opcode  (request)	*/
	{0x00, 0x0C, 0x29, 0x7F, 0x7C, 0x06},	  /* src_mac			*/
	{136, 187, 44, 25},						  /* src_ip				*/
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	  /* dst_mac			*/
	{136, 187, 44, 24},						  /* dst_ip				*/
  },
  {0x00},								  /* trailer (0 fill) */
  60,									  /* size			*/
};
  
#endif


//char * eve;

SDifc sdataifc;   
Dev sddevtab;

extern Dev etherdevtab;

static char  buf [2048];

extern void ether79c970link(void);

/* dump content of buffer like a binary viewer */
void
hexdump(uchar *buf, ulong len){
  unsigned long dumped;
  int i;

  dumped = 0;
  while(1){
	for(i = 0; i < 16; i++){
	  if(dumped >= len){
		goto func_end;
	  }

	  DBGPRN("%02x ", buf[dumped++]);
	}
	DBGPRN("\n");
  }

func_end:
  DBGPRN("\n");
  return ;
}

int devether_test()
{
  int  i, rc;
  char *name1[] = {"ether0"};
  char *name2[] = {"clone"};
  char *name3[] = {"0"};
  char *name4[] = {"data"};
  char *name5[] = {"ctl"};
  Chan *cp1, *cp2, *cp3;
//  Block *bp;

  conf.ialloc = (8192/2) * BY2PG;
  for (i = 0; i<24; i++)
    l4printf("\n");

  DBGPRN("<><><><><> ETHER DRIVER TEST START <><><><>\n");

  /***************************************************************************/
  /* Ether card initialize section											 */
  /***************************************************************************/
  L4_KDB_Enter("Start Ether-Driver");
  DBGBRK("====== p9_irq_init ======\n");
//  p9_irq_init();

  DBGBRK("====== server_init ======\n");
  eve_procsetup();

  DBGBRK("====== ether79c970link ======\n");
#if 1
  DBGPRN("Skip ether79c970link()\n");
#else
  ether79c970link();
#endif

  DBGBRK("====== Reset ======\n");
//  chandevreset();
#if 1
  DBGPRN("Skip Reset section\n");
#else
  etherdevtab.reset();
#endif

  DBGBRK("====== Init ======\n");
//  chandevinit();
  etherdevtab.init();

  /***************************************************************************/
  /* Open #l/ether0/clone													 */
  /***************************************************************************/
  DBGBRK("====== Attach(for clone) ======\n");
  cp1 = namec("#l", Atodir, 0, 0);

  DBGBRK("====== Walk(clone  #l -> #l/ether0) ======\n");
  rc = walk(&cp1, name1, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp1));

  DBGBRK("====== Walk(clone  #l/ether0 -> #l/ether0/clone) ======\n");
  rc = walk(&cp1, name2, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp1));

  DBGBRK("====== Open(#l/ether0/clone, OREAD) ======\n");
  cp1 = etherdevtab.open(cp1, OREAD);


  /***************************************************************************/
  /* Open #l/ethre0/0/ctl													 */
  /***************************************************************************/
  DBGBRK("====== Attach(for ctl) ======\n");
  cp3 = namec("#l", Atodir, 0, 0);

  DBGBRK("====== Walk(ctl  #l -> #l/ether0) ======\n");
  rc = walk(&cp3, name1, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp3));

  DBGBRK("====== Walk(ctl  #l/ether0 -> #l/ether0/0) ======\n");
  rc = walk(&cp3, name3, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp3));

  DBGBRK("====== Walk(ctl  #l/ether0/ -> #l/ether0/0/ctl) ======\n");
  rc = walk(&cp3, name5, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp3));

  DBGBRK("====== Open(#l/ether0/0/ctl, OWRITE) ======\n");
  cp3 = etherdevtab.open(cp3, OWRITE);
  DBGPRN("rc = 0x%08x\n", cp3);

#define CONTYPE "connect 2054"

  DBGBRK("====== Write(#l/ether0/0/ctl, \"%s\") ======\n", CONTYPE);
  rc = etherdevtab.write(cp3, CONTYPE, sizeof(CONTYPE) - 1, 0);
  DBGPRN("rc = %d\n", rc);
  
  /***************************************************************************/
  /* Open #l/ether0/0/data													 */
  /***************************************************************************/
  DBGBRK("====== Attach(for data) ======\n");
  cp2 = namec("#l", Atodir, 0, 0);

  DBGBRK("====== Walk(data  #l -> #l/ether0) ======\n");
  rc = walk(&cp2, name1, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp2));

  DBGBRK("====== Walk(data  #l/ether0 -> #l/ether0/0) ======\n");
  rc = walk(&cp2, name3, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp2));

  DBGBRK("====== Walk(data  #l/ether0/0 -> #l/ether0/0/data) ======\n");
  rc = walk(&cp2, name4, 1, 1, nil);
  DBGPRN("chanpath = %s\n", chanpath(cp2));

  DBGBRK("====== Open(#l/ether0/0/data, ORDWR) ======\n");
  cp2 = etherdevtab.open(cp2, ORDWR);

  /***************************************************************************/
  /* Transmit and Receive section											 */
  /***************************************************************************/
  DBGBRK("===== TRANSMIT =====\n");
  rc = etherdevtab.write(cp2, &data, data.size, 0);
  DBGPRN("write rc=%d\n", rc);


  DBGBRK("===== RECEIVE  =====\n");
//  while(1){
    rc = etherdevtab.read(cp2, buf, sizeof(buf), 0);
    hexdump(buf, rc);
	DBGBRK("Any key ...\n");
//  }

  etherdevtab.close(cp3);
  cclose(cp3);

  etherdevtab.close(cp2);
  cclose(cp2);

  etherdevtab.close(cp1);
  cclose(cp1);

  DBGBRK("<<<<< ETHER <<<<<");
  return 0;
}
