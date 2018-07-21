### WHERE #################################
INC_DIR      = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
#BOOT_DIR   = ../../../rootfs/boot
CMDBIN_DIR = ../../../rootfs/bin 

####################################################################
# this should be the very first rule
_default: all

####################################################################
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

####################################################################
LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)

#### Compiler ################################################
INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -DQSH=1  -DINITPROC=1 -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS		+= -fsigned-char -funsigned-bitfields

#### some evil magic ########################################
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

### compile rules ############################################
include  ../../Maketool

#### Linker ###################################################
LINKBASE = 0x00e00000

LDFLAGS 	+= -nostdlib

####################################################################
TARGET  =  qsh

all:     $(TARGET)
	cp   $(TARGET)  $(CMDBIN_DIR)

$(TARGET): qsh.o  dirread.o    mkdir.o  cp.o  zip.o  unzip.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	    $(CRT0) $^ -ll4 -ll4com -ll4io -lc -lgcc -ll4 -ldbg -lflate -lbio -lc


###################
install:   $(TARGET)
	cp   $(TARGET)  $(CMDBIN_DIR)

clean::
	$(RM) -f *~ *.o   $(TARGET) 

