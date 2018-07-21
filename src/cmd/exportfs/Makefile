### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

####################################################################

CFLAGS	+= -Wall -Wno-format -O -freg-struct-return # -fomit-frame-pointer 
CFLAGS		+= -fsigned-char -funsigned-bitfields

INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))


####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

####################################################################
# compile rules
#include ../../Maketool

#### Linker ########################################################

LINKBASE = 0x00C00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib -L$(LIBGCC) -L$(LIB_DIR)


####################################################################
# library rules
# LIB_OBJ	= $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))

# $(LIB): $(LIB_OBJ)
#	$(AR) -rcs $(LIB) $(LIB_OBJ)

####################################################################
# striptease

# %.stripped:	%
#	@chmod a+r $^
#	@chmod a-x $^
#	@cp -fp $^ $@
#	$(STRIP) $@

# dependencies ####################################################
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

####################################################################
OBJ =   exportfs.o  exportsrv.o  pattern.o  


TARGET  =  exportfs

all:  $(TARGET)

exportfs:  $(OBJ)  ../lib/dbg9p.o   # __vfprint.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lauth -lbio -lregexp  -lc -ll4io -lgcc -ll4 -ll4com 
	cp   $@  $@.u
	strip  $@
	cp  $@	 $(CMDBIN_DIR) 

install: $(TARGET)
	cp  $(TARGET)	$(CMDBIN_DIR) 

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz


