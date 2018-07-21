### WHERE #################################
INC_DIR  = ../../../include
CRT0     = ../../../lib/crt0-ia32.o
LIB_DIR  = ../../../lib
BOOT_DIR   = ../../../rootfs/boot
 
####################################################################
# this should be the very first rule
_default: all

####################################################################
include  ../../Maketool

# SHELL    = bash
# RM       = rm
# LN_S     = ln -s
# CC       = gcc32
# LD       = ld
# OBJCOPY  = objcopy
# STRIP    = strip
# AR       = ar
# GZIP     = gzip

LIBGCC   = $(dir $(shell $(CC) --print-lib))
SFLAGS  += -DASSEMBLY
LDFLAGS += -L$(LIBGCC) -L$(LIB_DIR)

#### Compiler ######################################################
INCLUDES        += $(INC_DIR)

CPPFLAGS        += -nostdinc -fno-builtin \
        $(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS  += -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return
CFLAGS  += -funsigned-char -funsigned-bitfields


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
	$(GZIP)  -c $< > $@
#	$(GZIP)  $<

#### Linker ########################################################
LINKBASE = 0x00900000
                                                                                     
LDFLAGS         += -nostdlib
# LDFLAGS       += -L$(LIBGCC)
# LDFLAGS       += -L$(L4MX_LIB_DIR)


####################################################################

clean::	
	$(RM) -f *~ *.o 

#########################################
OBJ =     init_.o   aux_.o  

TARGET  =   init

all:     $(TARGET).gz
	cp   $(TARGET).gz  $(BOOT_DIR)

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lauth -lbio -lc -lgcc -ll4   -ll4com -ll4io  -ldbg

###################
install:   $(TARGET).gz
	cp   $(TARGET).gz  $(BOOT_DIR)

clean::
	rm -f $(TARGET)   $(TARGET).gz 

