### WHERE #################################
INC_DIR      = ../../../include
# CRT0       = ../../../lib/crt0-ia32-boot.o
CRT0         = ../../../lib/crt0-ia32.o
LIB_DIR      = ../../../lib
BOOT_DIR     = ../../../rootfs/boot 

####################################################################
# this should be the very first rule
_default: all

####################################################################
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

####################################################################
LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)

INCLUDES	+= $(INC_DIR) 

CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS		+= -funsigned-char -funsigned-bitfields

####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

#### compile rules ##################################################
include  ../../Maketool

#%.o:    %.S
#	$(CC) $(CPPFLAGS) $(SFLAGS) -o $@ -c $<

#%.o:	%.c
#	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

#%.gz:   %
#	$(GZIP) -c $< > $@

#### Linker ########################################################

LINKBASE = 0x00a00000

LDFLAGS 	+= -nostdlib
# LDFLAGS 	+= -L$(LIBGCC)
# LDFLAGS 	+= -L$(L4MX_LIB_DIR)

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
OBJ =  thread.o   startup.o  mx-pager.o  l4-bootinfo.o  \
       main.o   proc2proc-copy.o  l4print_svr.o  

TARGET  =  hvm

all:     $(TARGET).gz
	cp  $(TARGET).gz  $(BOOT_DIR)

hvm: $(OBJ)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4 -ll4io 
	cp  $@  $@.u
	$(STRIP)  $@

install: $(TARGET).gz
	cp  $(TARGET).gz  $(BOOT_DIR)


clean::
	$(RM) -f *~  *.o *.i  $(TARGET) $(TARGET).u  $(TARGET).stripped $(TARGET).gz

