### WHERE #################################
INC_DIR      = ../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return \
		   -I../libmp/port
		   

####################################################################
include  ../Maketool


LIBGCC	 = $(dir $(shell $(CC) --print-lib))
# SFLAGS   += -DASSEMBLY
# LDFLAGS  += -L$(LIBGCC) -L$(L4MX_LIB_DIR)
# DEFINES  += __ARCH__=$(ARCH)   __PLATFORM__=$(PLATFORM)

#### Compiler ######################################################
INCLUDES  += $(INC_DIR) 
		   
CPPFLAGS  += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	  += -fsigned-char -funsigned-bitfields

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
LDFLAGS 	+= -nostdlib
# LDFLAGS 	+= -L$(LIBGCC)

####################################################################
# library rules
# LIB_OBJ  = $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))

# $(LIB): $(LIB_OBJ)
#	$(AR) -rcs $(LIB) $(LIB_OBJ)

####################################################################
# striptease

# %.stripped:	%
#	@chmod a+r $^
#	@chmod a-x $^
#	@cp -fp $^ $@
#	$(STRIP) $@

####################################################################
# dependencies
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

####################################################################
clean::	
	$(RM) -f *~  *.o $(LIB)

#########################################
LIB = libip.a

OBJ =   bo.o		\
		classmask.o	\
		eipfmt.o	\
		equivip.o	\
		ipaux.o		\
		myetheraddr.o \
		parseether.o  \
		parseip.o	\
		ptclbsum.o	\
		readipifc.o	

# testreadipifc.o


TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	$(AR) rvcs $@ $(OBJ)
#	@$(RANLIB) $@


#	$(AR) -rcs $(LIB) $(OBJ)


