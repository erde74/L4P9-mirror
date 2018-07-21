### WHERE #################################
INC_DIR      = ../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return

####################################################################
SHELL	 = bash
RM	 = rm
LN_S	 = ln -s
CC	 = gcc32
LD	 = ld
OBJCOPY	 = objcopy
STRIP	 = strip
AR	 = ar
RANLIB   = ranlib
GZIP	 = gzip

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
# LDFLAGS  += -L$(LIBGCC) -L$(L4MX_LIB_DIR)


#### Compiler ######################################################
INCLUDES  += $(INC_DIR) 
		   
CPPFLAGS  += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	  += -fsigned-char -funsigned-bitfields

###  SIGNED-CHAR

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

####### striptease ################################################
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
	$(RM) -f *~  *.o  libdraw.a

#########################################
LIB = libmemlayer.a


OBJ =   draw.o\
        lalloc.o\
        layerop.o\
        ldelete.o\
        lhide.o\
        line.o\
        load.o\
        lorigin.o\
        lsetrefresh.o\
        ltofront.o\
        ltorear.o\
        unload.o\



TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@


#	$(AR) -rcs $(LIB) $(OBJ)

