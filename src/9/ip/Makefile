### WHERE #################################
INC_DIR  = ../../../include
CRT0     = ../../../lib/crt0-ia32.o
LIB_DIR  = ../../../lib

### this should be the very first rule #########
_default: all

################################################
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

#%.o:    %.S
#	$(CC) $(CPPFLAGS) $(SFLAGS) -o $@ -c $<

#%.o:	%.c
#	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

#%.gz:   %
#	$(GZIP)  $<

#### Linker ######################################
LINKBASE = 0x00900000

LDFLAGS 	+= -nostdlib


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
clean::	
	$(RM) -f *~  *.o ../port/*.o    $(LIB)

#################################################
OBJ =    arp.o \
 devip.o \
 esp.o \
 gre.o \
 icmp.o \
 icmp6.o \
 il.o \
 inferno.o \
 ip.o \
 ipaux.o \
 ipifc.o \
 ipmux.o \
 iproute.o \
 ipv6.o \
 netlog.o \
 ptclbsum.o \
 rudp.o \
 tcp.o \
 udp.o  \
 ethermedium.o \
 loopbackmedium.o \
 netdevmedium.o \
 nullmedium.o \
 pktmedium.o \
 chandial.o \

# eipconvtest.o \
# igmp.o \
# tripmedium.o \


OBJ_PC =   	../pc/_relief-l4.o  	../pc/devether.o 	 \
	../pc/pcf-l4.o   	../pc/sdata.o   	../pc/devrtc.o \
	../pc/ether79c970.o  	../pc/pci.o   	../pc/sdscsi.o   	../pc/config-l4.o \
	../pc/devfloppy.o 	../pc/dma.o ../pc/main-l4.o
#	../pc/errstr.o

OBJ_PORT = ../port/dev.o     ../port/devroot.o  ../port/devpipe.o \
	../port/netif.o   ../port/proc-l4.o  ../port/sysproc-l4.o \
	../port/allocb.o  ../port/devenv.o   ../port/devsd.o    \
	../port/parse.o   ../port/qio.o      ../port/xalloc-l4.o  \
	../port/chan.o    ../port/devfs.o    ../port/devsrv.o  \
	../port/pgrp.o    ../port/sysfile-l4.o  ../port/syssrv-l4.o \
	../port/alloc.o	  ../port/devdup.o    ../port/_dumptbl.o \
	../port/errlist.o  ../port/devramfs.o

#	../port/devsd-test2.o ../port/ether-test.o  \


TARGET =  ip

all:     $(TARGET)

ip:  $(OBJ)  $(OBJ_PORT) $(OBJ_PC)  # dummy.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -ll4 -ll4com -ll4io  -lc  -lgcc -lsec

clean::
	rm -f $(TARGET) 

