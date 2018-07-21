### WHERE #################################
INC_DIR      = ../../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts

CFLAGS	+= -Wall -Wno-format -O -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 

CPPFLAGS  += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	  += -fsigned-char -funsigned-bitfields

####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

####################################################################
# compile rules

include ../../Maketool

#### Linker ########################################################
LDFLAGS 	+= -nostdlib

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
	$(RM) -f *~  *.o 

#########################################
LIB = ../libc.a

OBJ =   dofmt.o\
        dorfmt.o\
        errfmt.o\
        fltfmt.o\
        fmt.o\
        fmtfd.o\
        fmtlock.o\
        fmtprint.o\
        fmtquote.o\
        fmtrune.o\
        fmtstr.o\
        fmtvprint.o\
        fprint.o\
        print.o\
        runefmtstr.o\
        runeseprint.o\
        runesmprint.o\
        runesnprint.o\
       runesprint.o\
        runevseprint.o\
        runevsmprint.o\
        runevsnprint.o\
        seprint.o\
        smprint.o\
        snprint.o\
        sprint.o\
        vfprint.o\
        vseprint.o\
        vsmprint.o\
        vsnprint.o\


TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	$(AR) -rcsv $(LIB) $(OBJ)

#	@$(AR) rvc $@ $(OBJ)
#	@$(RANLIB) $@





