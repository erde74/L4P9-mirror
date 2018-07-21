//%

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../pc/io.h"
#include "../port/error.h"

extern Dev rootdevtab;
extern Dev consdevtab;
extern Dev archdevtab;
extern Dev pnpdevtab;
extern Dev envdevtab;
extern Dev pipedevtab;
extern Dev procdevtab;
extern Dev mntdevtab;
extern Dev srvdevtab;
extern Dev dupdevtab;
extern Dev rtcdevtab;
extern Dev ssldevtab;
extern Dev tlsdevtab;
extern Dev capdevtab;
extern Dev kprofdevtab;
extern Dev etherdevtab;
extern Dev ipdevtab;
extern Dev drawdevtab;
extern Dev mousedevtab;
extern Dev vgadevtab;
extern Dev kbmapdevtab;
extern Dev sddevtab;
extern Dev floppydevtab;
extern Dev lptdevtab;
extern Dev audiodevtab;
extern Dev pccarddevtab;
extern Dev i82365devtab;
extern Dev uartdevtab;
extern Dev usbdevtab;
extern Dev ramfsdevtab;

Dev* devtab[]={
	&rootdevtab,
	&consdevtab,
	&archdevtab,
	//%	&pnpdevtab,
  	&envdevtab,
	&pipedevtab,
	&procdevtab,
	&mntdevtab,
	&srvdevtab,
	&dupdevtab,
	//%	&rtcdevtab,
	//%	&ssldevtab,
	//%	&tlsdevtab,
	//%	&capdevtab,
	//%	&kprofdevtab,
	&etherdevtab,
	&ipdevtab,
	&drawdevtab,	// HK 20091031
	&mousedevtab,	// HK 20091031
	&vgadevtab,	// HK 20090930
	//%	&kbmapdevtab,
	&sddevtab,    // bind -a #SsdC0  /dev
	&floppydevtab,
	//%	&lptdevtab,
	//%	&audiodevtab,
	//%	&pccarddevtab,
	//%	&i82365devtab,
	//%	&uartdevtab,
	&usbdevtab,
	&ramfsdevtab,
	nil,
};

extern void _i8237alloc(void);
//%     void (*i8237alloc)(void) = _i8237alloc;
int i8237dma = 2;

extern void realmodelink(void);
extern void devpccardlink(void);
extern void devi82365link(void);
extern void apmlink(void);
extern void ether2000link(void);
extern void ether2114xlink(void);
extern void ether589link(void);
extern void ether79c970link(void);
extern void ether8003link(void);
extern void ether8139link(void);
extern void ether82543gclink(void);
extern void ether82557link(void);
extern void ether83815link(void);
extern void etherdp83820link(void);
extern void etherec2tlink(void);
extern void etherelnk3link(void);
extern void etherga620link(void);
extern void etherigbelink(void);
extern void ethervgbelink(void);
extern void ethervt6102link(void);
extern void ethersinklink(void);
extern void ethersmclink(void);
extern void etherwavelanlink(void);
extern void ethermediumlink(void);
extern void pcmciamodemlink(void);
extern void netdevmediumlink(void);
extern void loopbackmediumlink(void);
extern void usbuhcilink(void);

void links(void){

        //%	bootlinks();
        //%	realmodelink();
        //%	devpccardlink();
        //%	devi82365link();
        //%	apmlink();
        ether2000link();     //% YU
        ether2114xlink();    //% YU
        //%	ether589link();

	ether79c970link();   //% YS

        //%	ether8003link();

        ether8139link();    //% KH 
        //% 	ether82543gclink();
        ether82557link();
        //%	ether83815link();
        //%	etherdp83820link();
        //%	etherec2tlink();
        //%	etherelnk3link();
        //%	etherga620link();
        //%	etherigbelink();
        //%	ethervgbelink();
        ethervt6102link();   //% YU

        //%	ethersinklink();
        //%	ethersmclink();
        //%	etherwavelanlink();

	ethermediumlink();

        //%	pcmciamodemlink();
        //%	netdevmediumlink();
        loopbackmediumlink();

	usbuhcilink();
}

extern PCArch archmp;
PCArch* knownarch[] = {
  //%	&archmp,
	nil,
};

#include "../port/sd.h"
extern SDifc sdataifc;
extern SDifc sd53c8xxifc;
extern SDifc sdmylexifc;
SDifc* sdifc[] = {
	&sdataifc,
	//%	&sd53c8xxifc,
	//%	&sdmylexifc,
	nil,
};

extern PhysUart i8250physuart;
extern PhysUart pciphysuart;
PhysUart* physuart[] = {
        //%	&i8250physuart,
        //%	&pciphysuart,
	nil,
};

#define	Image	IMAGE
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "screen.h"
extern VGAdev vga3dfxdev;
extern VGAdev vgaark2000pvdev;
extern VGAdev vgaclgd542xdev;
extern VGAdev vgaclgd546xdev;
extern VGAdev vgact65545dev;
extern VGAdev vgacyber938xdev;
extern VGAdev vgaet4000dev;
extern VGAdev vgahiqvideodev;
extern VGAdev vgai81xdev;
extern VGAdev vgamach64xxdev;
extern VGAdev vgamga2164wdev;
extern VGAdev vgamga4xxdev;
extern VGAdev vganeomagicdev;
extern VGAdev vganvidiadev;
extern VGAdev vgas3dev;
extern VGAdev vgat2r4dev;
extern VGAdev vgavesadev;
extern VGAdev vgavmwaredev;

extern VGAdev vgaqemustddev;	// HK 20090930
extern VGAdev vgageneric;	// HK 20090930

VGAdev* vgadev[] = {
	&vgaqemustddev,	// HK 20090930
	&vgageneric,		// HK 20090930

#if 0 //% ---------------------------
	&vga3dfxdev,
	&vgaark2000pvdev,
	&vgaclgd542xdev,
	&vgaclgd546xdev,
	&vgact65545dev,
	&vgacyber938xdev,
	&vgaet4000dev,
	&vgahiqvideodev,
	&vgai81xdev,
	&vgamach64xxdev,
	&vgamga2164wdev,
	&vgamga4xxdev,
	&vganeomagicdev,
	&vganvidiadev,
	&vgas3dev,
	&vgat2r4dev,
	&vgavesadev,
	&vgavmwaredev,
#endif //----------------------------
	nil,
};

extern VGAcur vga3dfxcur;
extern VGAcur vgaark2000pvcur;
extern VGAcur vgabt485cur;
extern VGAcur vgaclgd542xcur;
extern VGAcur vgaclgd546xcur;
extern VGAcur vgact65545cur;
extern VGAcur vgacyber938xcur;
extern VGAcur vgaet4000cur;
extern VGAcur vgahiqvideocur;
extern VGAcur vgai81xcur;
extern VGAcur vgamach64xxcur;
extern VGAcur vgamga2164wcur;
extern VGAcur vgamga4xxcur;
extern VGAcur vganeomagiccur;
extern VGAcur vganvidiacur;
extern VGAcur vgargb524cur;
extern VGAcur vgas3cur;
extern VGAcur vgat2r4cur;
extern VGAcur vgatvp3020cur;
extern VGAcur vgatvp3026cur;
extern VGAcur vgavmwarecur;

VGAcur* vgacur[] = {
#if 0 //%-----------------------
	&vga3dfxcur,
	&vgaark2000pvcur,
	&vgabt485cur,
	&vgaclgd542xcur,
	&vgaclgd546xcur,
	&vgact65545cur,
	&vgacyber938xcur,
	&vgaet4000cur,
	&vgahiqvideocur,
	&vgai81xcur,
	&vgamach64xxcur,
	&vgamga2164wcur,
	&vgamga4xxcur,
	&vganeomagiccur,
	&vganvidiacur,
	&vgargb524cur,
	&vgas3cur,
	&vgat2r4cur,
	&vgatvp3020cur,
	&vgatvp3026cur,
	&vgavmwarecur,
#endif //-------------------------
	nil,
};


#if 0 //% ----------------------------------- //% Move to ip/devip.c
#include "../ip/ip.h"
extern void ilinit(Fs*);
extern void tcpinit(Fs*);
extern void udpinit(Fs*);
extern void ipifcinit(Fs*);
extern void icmpinit(Fs*);
extern void icmp6init(Fs*);
void (*ipprotoinit[])(Fs*) = {
	ilinit,
	tcpinit,
	udpinit,
	ipifcinit,
	icmpinit,
	icmp6init,
	nil,
};
#endif //--------------------------------------

	int cpuserver = 0;
char* conffile = "pcf";

//% ulong kerndate = KERNDATE;


//%%% config parameter %%%%%%%%%%%%%%%%%%%%%%


char __bootline[64];
 
char __bootargs[2048] =
  "bootfile=sdC0!9fat!9pcauth\n"
  "monitor=multisync65\n"
  "vgasize=1024x768x16\n"
  "bootargs=local -B 4096\n"
 
  "ether0=type=79c790\n"
  "mouseport=ps2\n"
  "console=1 b19200\n"
  "baud=19200\n"
  "sdC0part=dos 63 1000/plan9 1001 2000\n"
  //  "sdD0part=dos 63 2000/plan9 2001 3000\n"
;

