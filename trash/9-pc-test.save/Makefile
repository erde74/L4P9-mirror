### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib

####################################################################
# this should be the very first rule
_default: all

####################################################################
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

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

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
SFLAGS	+= -DASSEMBLY
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)

#### Compiler ######################################################
INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS		+= -funsigned-char -funsigned-bitfields

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

LINKBASE = 0x00d00000

LIBGCC   = $(dir $(shell $(CC) --print-lib))

LDFLAGS 	+= -nostdlib
LDFLAGS 	+= -L$(LIBGCC)

####################################################################

TARGET  =  ex1  ex2   ex3   ex4


all:     $(TARGET)

ex1: ex1.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -lgcc -ll4io
	$(STRIP) $@

ex2: ex2.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io  -lc -lgcc
	$(STRIP) $@


ex3: ex3.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -ll4 -lgcc  -ll4io
	$(STRIP) $@

ex4: ex4.o thread-l4.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io
	$(STRIP) $@

mtest: minix-malloc.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io
	$(STRIP) $@

#thread-l4: thread-l4.o 
#	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
#		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io

clean::
	rm -f $(TARGET)  *e~ *.c~  *.o *.gz $(TARGET).gz

