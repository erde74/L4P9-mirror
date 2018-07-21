### WHERE #################################
INC_DIR  = ../../../include
CRT0     = ../../../lib/crt0-ia32.o
LIB_DIR  = ../../../lib
 
####################################################################
# this should be the very first rule
_default: all
 
####################################################################
include  ../../Maketool

# SHELL    = bash
# RM       = rm
# LN_S     = ln -s
# CC       = gcc32
# LD       = ld
# OBJCOPY  = objcopy
# STRIP    = strip
# AR       = ar
# GZIP     = gzip
 
LIBGCC   = $(dir $(shell $(CC) --print-lib))
SFLAGS  += -DASSEMBLY
LDFLAGS += -L$(LIBGCC) -L$(LIB_DIR)
 
#### Compiler ######################################################
INCLUDES        += $(INC_DIR)
                    
CPPFLAGS        += -nostdinc -fno-builtin \
        $(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))
 
CFLAGS  += -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return
CFLAGS  += -funsigned-char -funsigned-bitfields
 


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
LINKBASE = 0x00900000
                                                                                     
LDFLAGS         += -nostdlib
# LDFLAGS       += -L$(LIBGCC)
# LDFLAGS       += -L$(L4MX_LIB_DIR)


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

####################################################################
# dependencies

# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

####################################################################

clean::	
	$(RM) -f *~ *.o *.i *.ii $(LIB)

#########################################
OBJ =   boot-config.o	\
	boot.o	       \
	local.o       \
	_aux.o 	     \
	_init.o     \
	_newns.o


TARGET  =   boot-init

all:     $(TARGET)

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^   -lc -lgcc -ll4   -ll4com -ll4io  -ldbg
	$(STRIP)  $@

ns: _newns.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^   -lc -lgcc -ll4   -ll4com -ll4io  -ldbg
	$(STRIP)  $@

# devproc.o: devproc.c
#	$(CC) $(CPPFLAGS) $(SFLAGS)  devproc.c

qio.s: qio.c
	$(CC) $(CPPFLAGS) $(SFLAGS) -S qio.c



clean::
	rm -f $(TARGET) $(TARGET).stripped $(TARGET).gz

