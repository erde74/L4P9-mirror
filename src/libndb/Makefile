### WHERE #################################
INC_DIR      = ../../include .

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
LIB = libndb.a

OBJ =	csgetval.o csipinfo.o dnsquery.o ipattr.o ndbaux.o ndbcache.o ndbcat.o \
	ndbconcatenate.o ndbdiscard.o ndbfree.o ndbgetipaddr.o ndbgetval.o \
	ndbhash.o ndbipinfo.o ndblookval.o ndbopen.o ndbparse.o ndbreorder.o \
	ndbsubstitute.o

TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@


#	$(AR) -rcs $(LIB) $(OBJ)


# ramfs  ???

clean::	
	$(RM) -f *~  *.o  $(LIB)

