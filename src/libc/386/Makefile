### WHERE #################################
INC_DIR      = ../../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES   += $(INC_DIR) 

CPPFLAGS   += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	   += -fsigned-char -funsigned-bitfields

## some evil magic #################################################
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

## compile rules ###################################################
include ../../Maketool

#### Linker ########################################################
LDFLAGS 	+= -nostdlib

## library rules ###################################################
# LIB_OBJ  = $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))
# $(LIB): $(LIB_OBJ)
#	$(AR) -rcs $(LIB) $(LIB_OBJ)

###################################################################
LIB = ../libc.a

OBJ =   setjmp-l4.o _tas.o cmpxchg.o

TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	$(AR) -rcsv  $(LIB) $(OBJ)

#	@$(AR) rvc $@ $(OBJ)
#	@$(RANLIB) $@

clean::	
	$(RM) -f *~  *.o  
