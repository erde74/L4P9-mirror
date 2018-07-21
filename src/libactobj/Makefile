### WHERE #################################
INC_DIR      = ../../include
CRT0       = ../../lib/crt0-ia32.o
LIB_DIR    = ../../lib
CMDBIN_DIR = ../../rootfs/bin


####################################################################
# this should be the very first rule
_default: all

####################################################################

INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
CFLAGS		+= -fsigned-char -funsigned-bitfields


LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)

#### some evil magic ########################################

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

### compile rules ############################################
include   ../Maketool

#### Linker ###################################################

LINKBASE = 0x01000000

LDFLAGS 	+= -nostdlib


#########################################
LIB = libactobj.a

OBJ = l_actobj.o

TARGET  =  $(OBJ)
TEST = aobjtest

all:     $(LIB)  $(TEST) 

l_actobj.o: l_actobj.h l_actobj.c

$(LIB): $(OBJ)
	$(AR) rvcs $@ $(OBJ)

$(TEST): l_actobj.o  aobjtest.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4  -lc -ll4io   -lgcc
	cp $(TEST)  $(CMDBIN_DIR)


install:  $(TEST)
	cp $(TEST)  $(CMDBIN_DIR)


###################
clean::
	rm -f *~ *.o $(LIB) $(TEST) 

