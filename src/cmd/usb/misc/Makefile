### WHERE #################################
INC_DIR      = ../../../../include
CRT0       = ../../../../lib/crt0-ia32.o
LIB_DIR    = ../../../../lib

####################################################################
# this should be the very first rule
_default: all

####################################################################

INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
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
clean::	
	$(RM) -f *~ *.s *.o *.i *.ii *.s $(LIB)

#########################################

TARGET  =  usbmouse

OBJS =   usbmouse.o


all:  $(TARGET)


$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -lusb -lc  -lgcc -ll4io -ll4  -ll4com 

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).stripped $(TARGET).gz

