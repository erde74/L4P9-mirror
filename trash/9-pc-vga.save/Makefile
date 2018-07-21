### WHERE #################################
INC_DIR  = ../../../include
CRT0     = ../../../lib/crt0-ia32.o
LIB_DIR  = ../../../lib

### this should be the very first rule #########
_default: all

################################################


LIBGCC	 = $(dir $(shell $(CC) --print-lib))
SFLAGS	+= -DASSEMBLY
LDFLAGS += -L$(LIBGCC) -L$(LIB_DIR)

#### Compiler #####################################
INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields

#### some evil magic ############################

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

### compile rules ################################

include ../../Maketool


#### Linker ######################################
LINKBASE = 0x00a00000

LDFLAGS 	+= -nostdlib
# LDFLAGS 	+= -L$(LIBGCC)

### striptease ###################################
# %.stripped:	%
#	@chmod a+r $^
#	@chmod a-x $^
#	@cp -fp $^ $@
#	$(STRIP) $@

#### dependencies ################################
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

##################################################

OBJ_VGA = devvga.o vga.o  vgax.o  vgavmware.o   screen.o \
	cga.o  devdraw.o  devmouse.o  \
	vga-dummy.o 

#  devdraw.c and  devmouse.c belong to the portable directory.
#  ../../port/devdraw.o  ../../port/devmouse.o  \


all:     vga


vga:  $(OBJ_VGA)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -lc -ll4 -ll4com -ll4io  -lgcc -lsec \
	-lcontrol -ldraw -lmemdraw -lmemlayer -lmemdraw -ldraw -lcontrol -lc


clean::
	$(RM) -f *~   *.o 

