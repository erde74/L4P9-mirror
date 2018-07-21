### WHERE #################################
INC_DIR      = ../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return


LIBGCC	 = $(dir $(shell $(CC) --print-lib))
SFLAGS	+= -DASSEMBLY

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
include ../Maketool

#CC   =  gcc32
#### Linker ########################################################

# LINKBASE = 0x00900000

LDFLAGS 	+= -nostdlib
# LDFLAGS 	+= -L$(LIBGCC)
# LDFLAGS 	+= -L$(L4MX_LIB_DIR)

####################################################################
# library rules
# LIB_OBJ	= $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))

# $(LIB): $(LIB_OBJ)
#	$(AR) -rcs $(LIB) $(LIB_OBJ)


####################################################################
clean::	
	$(RM) -f *~ *.o *.i  $(LIB)

#########################################
LIB = libl4com.a

OBJ =   klib386.o l4-tidutcb.o  l4-support.o  \
	_incdec.o  boot-seq.o   srv-sync.o   ack2hvm.o


all: crt0-ia32.o  crt0-ia32-boot.o  $(LIB)

$(LIB): $(OBJ)
	$(AR) -rcs $(LIB) $(OBJ)

asm4.s: l4-tidutcb.c
	gcc  $(CPPFLAGS) $(CFLAGS) -o $@ -S $<
 
asm3.s: l4-tidutcb.c
	gcc32  $(CPPFLAGS) $(CFLAGS) -o $@ -S $<
 
clean::
	rm -f $(TARGET) $(TARGET).stripped $(TARGET).gz  

