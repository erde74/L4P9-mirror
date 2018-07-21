### WHERE #################################
INC_DIR      = ../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 

CPPFLAGS  += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	  += -fsigned-char -funsigned-bitfields

###  SIGNED-CHAR

####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

### compile rules  #################################################

include  ../Maketool

#### Linker ########################################################
LDFLAGS 	+= -nostdlib

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

#########################################
LIB = lib9p.a

OBJ =   auth.o\
        dirread.o\
        fid.o\
        file.o\
        intmap.o\
        mem.o\
        req.o\
        parse.o\
        srv.o\
        thread.o\
        uid.o\
        util.o\
	srvthread-l4.o 

#        listen.o\
#        post.o\
#       rfork.o\


TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@


#	$(AR) -rcs $(LIB) $(OBJ)

####################################################################
clean::	
	$(RM) -f *~  *.o  *.a


