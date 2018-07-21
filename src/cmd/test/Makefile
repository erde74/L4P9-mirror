### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin


####################################################################
# this should be the very first rule
_default: all

####################################################################
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

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
include  ../../Maketool


#### Linker ########################################################

LINKBASE = 0x00d00000

LIBGCC   = $(dir $(shell $(CC) --print-lib))

LDFLAGS 	+= -nostdlib
LDFLAGS 	+= -L$(LIBGCC)

####################################################################

TARGET  =  ex1  ex2   ex3     ex6   ex7  ex8  ex9  ex10  ex11 kdb


all:     $(TARGET)

ex1: ex1.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -lgcc  -lc
	$(STRIP) $@

ex2: ex2.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io  -lc -lgcc -lc
	$(STRIP) $@


ex3: ex3.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	$(STRIP) $@

#ex4: ex4.o thread-l4.o
#	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
#		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io -lc
#	$(STRIP) $@

ex6: ex6.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	$(STRIP) $@

ex7: ex7.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	cp ex7 $(CMDBIN_DIR)

ex8: ex8.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lactobj -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	cp ex8 $(CMDBIN_DIR)

ex9: ex9.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lactobj -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	cp ex9 $(CMDBIN_DIR)

ex10: ex10.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lactobj -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	cp ex10 $(CMDBIN_DIR)


ex11: ex11.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	cp ex11 $(CMDBIN_DIR)
	$(STRIP) $@

kdb: kdb.o 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io -lc -ll4 -lgcc  -ll4io -lc
	cp kdb $(CMDBIN_DIR)



mtest: minix-malloc.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io -lc
	$(STRIP) $@

#thread-l4: thread-l4.o 
#	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
#		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io -lc


install: $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

clean::
	rm -f $(TARGET)  *e~ *.c~  *.o *.gz $(TARGET).gz

