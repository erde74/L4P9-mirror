### WHERE #################################
INC_DIR      = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin
BOOT_DIR = ../../../rootfs/boot

####################################################################
# this should be the very first rule
_default: all

####################################################################
LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 

CPPFLAGS  += -DCORE  -nostdinc  -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields


####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

####################################################################
# compile rules
include ../../Maketool

#### Linker ########################################################
LINKBASE = 0x00C00000

LIBGCC   = $(dir $(shell $(CC) --print-lib))
LDFLAGS 	+= -nostdlib -L$(LIBGCC) -L$(LIB_DIR)

####################################################################

TARGET  =  9660srv

OBJS =  main.o\
        9660srv.o\
        xfile.o\
        iobuf.o\
        data.o


all:     $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)
#	cp  $(TARGET).gz  $(BOOT_DIR)

9660srv: $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -ll4io  -lc -lgcc -ll4  -ll4  -ll4com  -ll4io
	cp    $@  $@.u
	$(STRIP)  $@


install:  $(TARGET)  $(TARGET).gz
	cp  $(TARGET)  $(CMDBIN_DIR)
#	cp  $(TARGET).gz  $(BOOT_DIR)

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz

