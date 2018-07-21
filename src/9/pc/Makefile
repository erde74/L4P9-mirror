### WHERE #################################
INC_DIR  = ../../../include
CRT0     = ../../../lib/crt0-ia32.o
LIB_DIR  = ../../../lib
BOOT_DIR = ../../../rootfs/boot

### this should be the very first rule #########
_default: all

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS += -L$(LIBGCC) -L$(LIB_DIR)

#### Compiler #####################################
INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields

#### some evil magic ############################
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

### compile rules ################################
include  ../../Maketool

#### Linker ######################################
LINKBASE = 0x00a00000

LDFLAGS 	+= -nostdlib
# LDFLAGS 	+= -L$(LIBGCC)

### striptease ###################################
# %.stripped:	%
#	@chmod a+r $^
#	@chmod a-x $^
#	@cp -fp $^ $@
#	$(STRIP) $@

#### dependencies ################################
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

##################################################

OBJ =   main-l4.o  	_relief-l4.o  	devether.o 	errstr.o \
	sdata.o   	devrtc.o   	config-l4.o \
	ether79c970.o  	pci.o   	sdscsi.o  \
	devfloppy.o 	dma.o   	 \
	devusb.o  	devarch.o       kbd.o           devcons.o  \
	asm-lp49.o  	devi82365.o	usbuhci.o  \
	l4-p9-irq.o  	ether8139.o    	ether2000.o     ether8390.o \
	ether2114x.o    ether82557.o 	ethervt6102.o   ethermii.o  \
	../../libl4com/l4-tidutcb.o   \
	../vga/vga_test.o

#	devpccard.o   


OBJ_PORT = ../port/dev.o  	../port/devroot.o  	../port/devpipe.o \
	../port/netif.o   	../port/proc-l4.o  	../port/sysproc-l4.o \
	../port/allocb.o  	../port/devenv.o   	../port/devsd.o    \
	../port/parse.o   	../port/qio.o      	../port/xalloc-l4.o  \
	../port/chan.o    	../port/devsrv.o  \
	../port/pgrp.o    	../port/sysfile-l4.o  	../port/systry-l4.o \
	../port/syssrv-l4.o \
	../port/alloc.o	  	../port/devdup.o    	../port/_dumptbl.o  \
	../port/devrootfs.o  \
	../port/devmnt.o  	../port/cache.o  	../port/elfexec-l4.o \
	../port/portclock.o  	../port/taslock-l4.o  	../port/qlock.o  \
	../port/partition-l4.o  \
	../port/devproc.o \

#	../port/devmouse.o

# 	../port/devfs.o
#	../port/devsd-test2.o 	../port/ether-test.o ../port/ip-test.o \
#	../port/devmouse.o


OBJ_IP = ../ip/arp.o 		../ip/devip.o 		../ip/esp.o \
	../ip/gre.o 		../ip/icmp.o 		../ip/icmp6.o \
	../ip/il.o 		../ip/inferno.o 	../ip/ip.o \
	../ip/ipaux.o 		../ip/ipifc.o 		../ip/ipmux.o \
	../ip/iproute.o 	../ip/ipv6.o 		../ip/netlog.o \
	../ip/ptclbsum.o 	../ip/rudp.o 		../ip/tcp.o \
	../ip/udp.o  		../ip/ethermedium.o 	../ip/loopbackmedium.o \
	../ip/netdevmedium.o 	../ip/nullmedium.o 	../ip/pktmedium.o \
	../ip/chandial.o

# ../ip/eipconvtest.o
# ../ip/igmp.o
# ../ip/tripmedium.o


OBJ_VGA = ../vga/devvga.o 	../vga/vga.o  		../vga/vgax.o  \
	../vga/vgaqemustd.o   	../vga/screen.o 	../vga/cga.o   \
	../vga/devdraw.o  	../vga/devmouse.o  	../vga/vga-dummy.o    \
	../vga/vgageneric.o ../vga/mappages.o	../vga/mouse.o	\
	../vga/uarti8250.o	../vga/devuart.o

TARGET =  pc

all:     $(TARGET).gz
	cp  $(TARGET).gz  $(BOOT_DIR)

#pc:  $(OBJ)  $(OBJ_PORT) $(OBJ_IP) 
#	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
#		$(CRT0) $^  -lc -ll4 -ll4com -ll4io  -lgcc -lsec -lc -ldbg
#	cp  $@  $@.u
#	$(STRIP) $@

pc:  $(OBJ)  $(OBJ_PORT)  $(OBJ_IP)  $(OBJ_VGA)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -ll4 -ll4com -ll4io  -lgcc -lsec \
		-lcontrol -ldraw -lmemdraw -lmemlayer -lmemdraw -ldraw -lcontrol -lc
	cp  $@  $@.u
	$(STRIP) $@

pc2:  $(OBJ)  $(OBJ_PORT) $(OBJ_IP) 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -ll4 -ll4com -ll4io  -lgcc -lsec -lc
	cp  $@  $@.u
	$(STRIP) $@


pcvga:  $(OBJ)  $(OBJ_PORT)  $(OBJ_IP)  $(OBJ_VGA)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -ll4 -ll4com -ll4io  -lgcc -lsec \
		-lcontrol -ldraw -lmemdraw -lmemlayer -lmemdraw -ldraw -lcontrol -lc
	cp  $@  $@.u
	$(STRIP) $@


#############################################################

sdata:  sdata.o  sdata-test.o  tst-malloc.o  sdscsi.o  \
	     _relief.o   _setup.o ../port/errlist.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ll4 -ll4com -ll4io  -lgcc  -lc

devsd:  sdata.o  ../port/devsd-test.o  tst-malloc.o  sdscsi.o \
	  _relief.o  _setup.o   ../port/errlist.o \
	  ../port/devsd.o ../port/chan.o ../port/allocb.o \
	  ../port/dev.o  ../port/devroot.o  ../port/pgrp.o \
	  ../port/parse.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ll4 -ll4com -ll4io -lc -lgcc

ether:  ether79c970.o  ether-test.o  tst-malloc.o devether.o  \
	  _relief.o  _setup.o  ../port/errlist.o\
	  ../port/dev.o  ../port/netif.o  ../port/qio.o  \
	  ../port/allocb.o  ../port/chan.o ../port/devroot.o \
	  ../port/pgrp.o  
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ll4 -ll4com -ll4io  -lc -lgcc 

ether8169:  ether8169.o  ether-test.o  tst-malloc.o devether.o  \
	  _relief.o  _setup.o  ../port/errlist.o\
            ../port/dev.o  ../port/netif.o  ../port/qio.o  \
	  ../port/allocb.o  ../port/chan.o ../port/devroot.o \
	../port/pgrp.o  
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ll4 -ll4com -ll4io  -lc -lgcc 

ether8139:  ether8139.o  ether-test.o  tst-malloc.o devether.o  \
	  _relief.o  _setup.o  ../port/errlist.o\
	  ../port/dev.o  ../port/netif.o  ../port/qio.o  \
	  ../port/allocb.o  ../port/chan.o ../port/devroot.o \
	  ../port/pgrp.o  
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ll4 -ll4com -ll4io  -lc -lgcc 


ether2000:  ether2000.o  ether8390.o ether-test.o tst-malloc.o devether.o \
          _relief.o _setup.o ../port/errlist.o\
          ../port/dev.o  ../port/netif.o  ../port/qio.o \
          ../port/allocb.o  ../port/chan.o  ../port/devroot.o \
          ../port/pgrp.o
        $(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE) -o $@ \
        $(CRT0) $^  -ll4 -ll4com -ll4io -lc -lgcc
 
ether8390:  ether2000.o  ether8390.o ether-test.o tst-malloc.o devether.o \
          _relief.o _setup.o ../port/errlist.o\
          ../port/dev.o  ../port/netif.o  ../port/qio.o \
          ../port/allocb.o  ../port/chan.o  ../port/devroot.o \
          ../port/pgrp.o
        $(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE) -o $@ \
        $(CRT0) $^  -ll4 -ll4com -ll4io -lc -lgcc
 


vga:  $(OBJ_VGA)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ldraw -lmemdraw -lc -lgcc -ll4 -ll4com -ll4io 


devcons:  ../port/devcons.o
#	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
#	$(CRT0) $^   -lc -lgcc -ll4 -ll4com -ll4io 

usbehci.o: usbehci.c
	$(CC) $(CPPFLAGS) $(SFLAGS)  usbehci.c



install: $(TARGET).gz
	cp  $(TARGET).gz  $(BOOT_DIR)


clean::
	$(RM) -f *~ ../port/*~  ../ip/*~  \
		*.o ../port/*.o ../ip/*.o  *.i  \
		$(TARGET)  $(TARGET).u  $(TARGET).gz pcvga  

