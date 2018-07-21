### WHERE #################################
INC_DIR      = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

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
# compile rules
include ../../Maketool

#### Linker ########################################################

LINKBASE = 0x00c00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)


####################################################################
clean::	
	$(RM) -f *~ *.s *.o *.i *.ii *.s $(LIB)

#########################################

TARGET  =  rio  riotst

OBJS =  read9pmsg.o  rio.o  data.o  util.o \
        fsys.o\
        scrl.o\
        time.o\
        wctl.o\
        wind.o\
        xfid.o\
	keyboard2.o \
	mouse2.o


all:     $(TARGET)


rio: $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -lactobj -ldraw -lframe -l9p -lbio -lcomplete -lc -lgcc -ll4  -ll4com -ldbg -ll4io 
	cp  rio  $(CMDBIN_DIR)

riotst: riotst.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ldraw -lframe -l9p -lbio -lc -lgcc -ll4  -ll4com -ldbg -ll4io 
	cp  riotst   $(CMDBIN_DIR)

rio.o: rio.c
data.o: data.c
fsys.o: fsys.c
scrl.o: scrl.c
time.o: time.c
util.o: util.c
wctl.o: wctl.c
wind.o: wind.c
xfid.o: xfid.c


install:  $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

clean::
	rm -f *~ *.o $(TARGET)

