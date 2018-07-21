### WHERE #################################
INC_DIR      = ../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

####################################################################
include  ../Maketool


LIBGCC	 = $(dir $(shell $(CC) --print-lib))


#### Compiler ######################################################
INCLUDES   += $(INC_DIR) 
		   
CPPFLAGS   += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	   += -fsigned-char -funsigned-bitfields

####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

## compile rules ###################################################
%.o:    %.S
	$(CC) $(CPPFLAGS) $(SFLAGS) -o $@ -c $<

%.o:	%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

%.gz:   %
	$(GZIP)  $<

#### Linker ########################################################
# LINKBASE = 0x00900000

LDFLAGS 	+= -nostdlib
# LDFLAGS 	+= -L$(LIBGCC)

## library rules ###################################################
# LIB_OBJ  = $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))
# $(LIB): $(LIB_OBJ)
#	$(AR) -rcs $(LIB) $(LIB_OBJ)

####################################################################
# dependencies
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

####################################################################
clean::	
	$(RM) -f *~  *.o  

#########################################

LIB = libdbg.a

OBJ =   dbg.o  dbgclock.o

TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@

#	$(AR) -rcs $(LIB) $(OBJ)
