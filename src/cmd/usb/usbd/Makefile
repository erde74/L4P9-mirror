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

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields

SFLAGS	+= -DASSEMBLY

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
clean::	
	$(RM) -f *~ *.s *.o *.i *.ii *.s $(LIB)

#########################################

TARGET  =  usbd

OBJS =  hub.o  setup.o  usbd.o


all:  $(TARGET)


usbd: $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -lusb -l9p -lbio -lc -lgcc -ll4io -ll4  -ll4  -ll4com 
	cp  $@  $@.u
	$(STRIP)  $@
	cp  $(TARGET)  $(CMDBIN_DIR)

install: $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)


clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz

