### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

####################################################################
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)

#### Compiler ######################################################
INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS		+= -funsigned-char -funsigned-bitfields

####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

####################################################################
# compile rules
include  ../../Maketool

#### Linker ########################################################
LINKBASE = 0x00d00000

LIBGCC   = $(dir $(shell $(CC) --print-lib))

LDFLAGS 	+= -nostdlib
LDFLAGS 	+= -L$(LIBGCC)

####################################################################
TARGET  =  filetest

all:     $(TARGET)
	cp  $(TARGET) $(CMDBIN_DIR)

filetest: filetest.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ldbg -ll4 -ll4io -lc -lgcc -ll4io


install: $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

clean::
	rm -f $(TARGET)  *e~ *.c~  *.o 

