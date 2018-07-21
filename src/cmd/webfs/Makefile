### WHERE #################################
INC_DIR      = ../../../include
// INC_DIR_APE      = ../../../include/ape  //%
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

####################################################################

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 
// INCLUDES  += $(INC_DIR_APE)    //%
		   
CPPFLAGS  += -DCORE  -nostdinc  -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields


####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules
include  ../../Maketool


#### Linker ########################################################

LINKBASE = 0x00C00000

LIBGCC   = $(dir $(shell $(CC) --print-lib))
LDFLAGS 	+= -nostdlib -L$(LIBGCC) -L$(LIB_DIR)


####################################################################

TARGET  =  webfs

SCHEMEOFILES=\
        file.o\
        ftp.o\
        http.o\

OFILES=\
        buf.o\
        client.o\
        cookies.o\
        fs.o\
        http.o\
        io.o\
        main.o\
        plumb.o\
        url.o\
        util.o\
#       $SCHEMEOFILES




all:     $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

$(TARGET): $(OFILES) 
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lplumb   -lc -lgcc -ll4   -ll4com -ll4io  -ldbg
	$(STRIP)  $@


install:  $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).stripped $(TARGET).gz


