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

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules

include  ../Maketool


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
	$(RM) -f *~  *.o 

#########################################
LIB = libString.a

OBJ =   s_alloc.o\
	s_append.o\
	s_array.o\
	s_copy.o\
        s_getline.o\
        s_grow.o\
        s_memappend.o\
        s_nappend.o\
        s_parse.o\
        s_putc.o\
        s_rdinstack.o\
        s_read.o\
        s_read_line.o\
        s_reset.o\
        s_terminate.o\
        s_tolower.o\
        s_unique.o\


TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@

