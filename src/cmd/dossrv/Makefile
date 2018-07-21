### WHERE #################################
INC_DIR      = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin
BOOT_DIR   = ../../../rootfs/boot

####################################################################
# this should be the very first rule
_default: all

####################################################################
LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 

CPPFLAGS  +=  -nostdinc  -fno-builtin \
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
TARGET  =  dossrv

OBJS =  xfssrv.o        xfile.o        dosfs.o        dossubs.o\
        iotrack.o        lock.o        chat.o        devio.o

OBJS2 =  xfssrv2.o        xfile.o        dosfs.o        dossubs.o\
        iotrack.o        lock.o        chat.o        devio.o

all:     dossrv.gz   dos2
	cp  dos2   $(CMDBIN_DIR)
	cp  dossrv.gz  $(BOOT_DIR)

dossrv: $(OBJS)	../lib/dbg9p.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -ll4io  -lc -lgcc -ll4  -ll4  -ll4com  -ldbg
	cp    $@  $@.u
	$(STRIP)  $@

dos2: $(OBJS2)	../lib/dbg9p.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -ll4io  -lc -lgcc -ll4  -ll4  -ll4com  -ldbg
	cp    $@  $@.u
	$(STRIP)  $@

install:  $(TARGET)  $(TARGET).gz
	cp  dos2  $(CMDBIN_DIR)
	cp  dossrv.gz  $(BOOT_DIR)

clean::
	rm -f *~ *.o dossrv dossrv.u dossrv.gz  dos2  dos2.u 

