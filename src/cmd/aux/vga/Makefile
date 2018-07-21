### WHERE #################################
INC_DIR    = ../../../../include
CRT0       = ../../../../lib/crt0-ia32.o
LIB_DIR    = ../../../../lib
CMDBIN_DIR = ../../../../rootfs/bin/aux	  

#  SUBDIRS = dossrv  ext2srv  ip

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

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules
include ../../../Maketool

#### Linker ########################################################

LINKBASE = 0x00C00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib -L$(LIBGCC) -L$(LIB_DIR)

####################################################################
# striptease

# %.stripped:	%
#	@chmod a+r $^
#	@chmod a-x $^
#	@cp -fp $^ $@
#	$(STRIP) $@

# dependencies ####################################################
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

####################################################################

OBJS =	main.o error.o io.o vga.o palette.o data.o vesa.o vesadb.o db.o pci.o \
	qemustd.o

TARGET  =  vga

all:  $(TARGET) 
	cp  $(TARGET)  $(CMDBIN_DIR)

install:  $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

vga : Makefile $(OBJS) # ../lib/dbg9p.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE) -o $@ $(CRT0) $(OBJS) \
	-lauth -lndb -lbio -lc -ll4io -lgcc -ll4 -ll4com 
	$(STRIP) $@

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz

