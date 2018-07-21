### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

SUBDIRS = httpd

####################################################################
# this should be the very first rule
_default: all

####################################################################
CFLAGS	+= -Wall -Wno-format -O -freg-struct-return
CFLAGS		+= -fsigned-char -funsigned-bitfields

INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

SFLAGS	+= -DASSEMBLY

####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules
include ../../Maketool


#### Linker ########################################################

LINKBASE = 0x00C00000


LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib -L$(LIBGCC) -L$(LIB_DIR)


####################################################################

TARGET  =  echosrv  ping  telnet

all:     $(TARGET) $(SUBDIRS)
	cp   $(TARGET)  $(CMDBIN_DIR)

.PHONY: $(SUBDIRS)

$(SUBDIRS):
	make -C $@


echosrv: echosrv.o # _announce.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4com -ll4 -ll4io  -lc -lgcc -ll4io
	cp  $@  $@.u
	$(STRIP)  $@

ping:  ping.o  #  dial.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lip  -lc  -lgcc -ll4 -ll4com -ll4io
	cp  $@  $@.u
	$(STRIP)  $@

telnet:  telnet.o  #  dial.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lip  -lbio -lc  -lgcc -ll4 -ll4com -ll4io
	cp  $@  $@.u
	$(STRIP)  $@

install:  $(TARGET)
	@for d in $(SUBDIRS2); do \
	  (cd $${d} && make install) \
	done
	cp  $(TARGET)  $(CMDBIN_DIR)

clean::
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make clean) \
	done
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz

#	@for d in $(SUBDIRS); do \
#	  (cd $${d} && make clean) \
#	done




