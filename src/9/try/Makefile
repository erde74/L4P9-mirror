### WHERE #################################
INC_DIR      = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
# LIB_DIR    = /home/maruyama/L4/pistachio-070717/user/BUILDDIR/lib
LIB_DIR    = ../../../lib

####################################################################
# this should be the very first rule
_default: all

####################################################################

INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
CFLAGS		+= -fsigned-char -funsigned-bitfields


LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)

#### some evil magic ########################################

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

### compile rules ############################################
include   ../../Maketool

#### Linker ###################################################

LINKBASE = 0x01000000

LDFLAGS 	+= -nostdlib


#### dependencies ##############################################

clean::	
	$(RM) -f *~  *.o *.i *.ii  $(LIB)

#########################################
TARGET  =  pingpong2.gz  irqtry.gz 

all:     $(TARGET)

#func: func.o l-l4.o
#	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
#	$(CRT0) $^ -ll4 -ll4com -ll4io -lc -lgcc


pingpong2: pingpong2.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io  

irqtry: irqtry.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io  -lc -lgcc

pv: pv.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc 

athread: athread-l4.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4  -lc -ll4io   -lgcc

l4print_svr: l4print_svr.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc 

###################
%.s:	%.c
	$(CC) $(CPPFLAGS) -S $(CFLAGS) -o $@ -c $<

func.s:   func.c
	$(CC) $(CPPFLAGS) -S $(CFLAGS) -o $@ -c $<

func:   func.c l-l4.s
	$(CC)   $(CFLAGS)  -o $@  func.c l-l4.s

#######
clean::
	rm -f $(TARGET) pingpong2  irqtry  athread

