### WHERE #################################
INC_DIR      = ../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 
		   
CPPFLAGS  += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	  += -funsigned-char -funsigned-bitfields

####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules

include ../Maketool

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
LIB = libbio.a

OBJ =   bbuffered.o\
        bfildes.o\
        bflush.o\
        bgetrune.o\
        bgetc.o\
        bgetd.o\
        binit.o\
        boffset.o\
        bprint.o\
        bputrune.o\
        bputc.o\
        brdline.o\
        brdstr.o\
        bread.o\
        bseek.o\
        bwrite.o\
        bvprint.o\



TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@


#	$(AR) -rcs $(LIB) $(OBJ)


# ramfs  ???

clean::	
	$(RM) -f *~  *.o  $(LIB)

