### WHERE #################################
INC_DIR      = ../../../../include
CRT0       = ../../../../lib/crt0-ia32.o
LIB_DIR    = ../../../../lib
CMDBIN_DIR = ../../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

####################################################################

INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields


####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules
include ../../../Maketool

#### Linker ########################################################

LINKBASE = 0x00c00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)
LDFLAGS  += -L../lib 


####################################################################
TARGET  =  usbsfs

OBJS = scsireq.o usbsfs.o  pmsrv-l4.o

all:     $(TARGET)


usbsfs: $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -lusb -l9p -lbio  -ll4io -ll4  -ll4com -lc -lgcc 
	cp  $@  $@.u
	$(STRIP)  $@
	cp  $(TARGET)  $(CMDBIN_DIR)

install: $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)


clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz

