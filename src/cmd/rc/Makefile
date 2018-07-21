### WHERE #################################
INC_DIR      = ../../../include
// INC_DIR_APE      = ../../../include/ape  //%
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

####################################################################
LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 
// INCLUDES  += $(INC_DIR_APE)    //%

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
include  ../../Maketool

#### Linker ########################################################
LINKBASE = 0x00C00000

LIBGCC   = $(dir $(shell $(CC) --print-lib))
LDFLAGS 	+= -nostdlib -L$(LIBGCC) -L$(LIB_DIR)

####################################################################
TARGET  =  rc

COMMONOFILES =  code.o\
        exec.o\
        getflags.o\
        glob.o\
        here.o\
        io.o\
        lex.o\
        pcmd.o\
        pfnc.o\
        simple.o\
        subr.o\
        trap.o\
        tree.o\
        var.o\
        havefork.o\

OFILES=$(COMMONOFILES) plan9.o  y.tab.o

HFILES=  rc.h    x.tab.h     io.h    exec.h    fns.h


all:     $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

$(TARGET): $(OFILES) 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^   -lc -lgcc -ll4   -ll4com -ll4io  -ldbg
	cp  $@  $@.u
	$(STRIP)  $@

x.tab.h: y.tab.h
	cmp -s x.tab.h y.tab.h || cp y.tab.h x.tab.h


install:  $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz


