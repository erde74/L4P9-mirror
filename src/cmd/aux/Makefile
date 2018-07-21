### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin


####################################################################
# this should be the very first rule
_default: all

####################################################################

CFLAGS	+= -Wall -Wno-format -O -freg-struct-return
CFLAGS		+= -fsigned-char -funsigned-bitfields

INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

####################################################################
# compile rules
include  ../../Maketool


#### Linker ########################################################
LINKBASE = 0x00C00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib -L$(LIBGCC) -L$(LIB_DIR)

####################################################################
TARGET  =  listen1   # listen

all:     $(TARGET) 

# .PHONY: $(SUBDIRS)
# $(SUBDIRS):
#	make -C $@


listen1: listen1.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -ll4com -lauth -lbio -lc -lgcc -ll4io -ll4
	cp  $@  $@.u
	$(STRIP) $@
	cp  listen1  $(CMDBIN_DIR)


listen: listen.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -ll4com  -lauth -lbio -lc -lgcc -ll4io -ll4
	cp  $@  $@.u
	$(STRIP)  $@
	cp  listen  $(CMDBIN_DIR)

install: $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)


clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u 

#	@for d in $(SUBDIRS); do \
#	  (cd $${d} && make clean) \
#	done

