### WHERE #################################
INC_DIR    = ../../../../include
CRT0       = ../../../../lib/crt0-ia32.o
LIB_DIR    = ../../../../lib  
BIN_DIR	   = ../../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

#### Compiler ######################################################

LIBGCC   = $(dir $(shell $(CC) --print-lib))

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return
CFLAGS		+= -fsigned-char -funsigned-bitfields

INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

# SFLAGS	+= -DASSEMBLY

####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules

include  ../../../Maketool

#### Linker ########################################################

LINKBASE = 0x00C00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib -L$(LIBGCC) -L$(LIB_DIR)  


####################################################################
# library rules
# LIB_OBJ	= $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))

# $(LIB): $(LIB_OBJ)
#	$(AR) -rcs $(LIB) $(LIB_OBJ)

####################################################################
# striptease

# %.stripped:	%
#	@chmod a+r $^
#	@chmod a-x $^
#	@cp -fp $^ $@
#	$(STRIP) $@

# dependencies ####################################################
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

####################################################################
sLIB = libhttps.a

TARGET  =  httpd

#        imagemap\
#        man2html\
#        save\
#        netlib_find\
#        netlib_history\
#        webls\
#        wikipost\

LIBOBJ=  anonymous.o\
        content.o\
        emem.o\
        hints.o\
	init.o\
        log.o\
        redirect.o\
        sendfd.o\
        authorize.o\


all:    $(LIB)  $(TARGET)

libhttps.a: $(LIBOBJ)
	@$(AR) rvc $@ $(LIBOBJ)
	@$(RANLIB) $@


httpd:  httpd.o libhttps.a
	$(LD)  $(LDFLAGS) -L./  -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -lhttps  -lhttpd -lauth -lbio -lbin -lc -lgcc -ll4 -ll4com -ll4io 
	cp  httpd  httpd.u
	$(STRIP)  httpd


install: $(TARGET)
	cp  $(TARGET)  $(BIN_DIR)

clean::
	rm -f *~ *.o *.a $(TARGET) $(TARGET).u $(TARGET).gz


