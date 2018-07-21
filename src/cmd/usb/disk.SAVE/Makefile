### WHERE #################################
INC_DIR      = ../../../../include
CRT0       = ../../../../lib/crt0-ia32.o
LIB_DIR    = ../../../../lib

####################################################################
# this should be the very first rule
_default: all

####################################################################
SHELL	 = bash
RM	 = rm
LN_S	 = ln -s
CC	 = gcc32
LD	 = ld
OBJCOPY	 = objcopy
STRIP	 = strip
AR	 = ar
GZIP	 = gzip

#### Compiler ######################################################
INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields

SFLAGS	+= -DASSEMBLY

####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules

%.o:    %.S
	$(CC) $(CPPFLAGS) $(SFLAGS) -o $@ -c $<

%.o:	%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

%.gz:   %
	$(GZIP)  $<

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

TARGET  =  disk

OBJS = disk.o   scsireq.o  pmsrv-l4.o

all:     $(TARGET)


$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -lusb -l9p -lbio -lc -lgcc -ll4io -ll4  -ll4  -ll4com

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).stripped $(TARGET).gz

