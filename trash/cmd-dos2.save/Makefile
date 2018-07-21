### WHERE #################################
INC_DIR      = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

####################################################################

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

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
include  ../../Maketool


#### Linker ########################################################

LINKBASE = 0x00C00000

LIBGCC   = $(dir $(shell $(CC) --print-lib))
LDFLAGS 	+= -nostdlib -L$(LIBGCC) -L$(LIB_DIR)


####################################################################

TARGET  =  dos2

OBJS =  xfssrv.o\
        ../dossrv/xfile.o\
        ../dossrv/dosfs.o\
        ../dossrv/dossubs.o\
        ../dossrv/iotrack.o\
        ../dossrv/lock.o\
        ../dossrv/chat.o\
        ../dossrv/devio.o\
	../print9pmsg.o

all:     $(TARGET)

dos2: $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ll4io  -lc -lgcc -ll4  -ll4  -ll4com  -ldbg
	cp $@ $@.u
	$(STRIP) $@

install: $(TARGET)
	cp $(TARGET)  $(CMDBIN_DIR)

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz


