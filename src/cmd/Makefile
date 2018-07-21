### WHERE #################################
INC_DIR    = ../../include
CRT0       = ../../lib/crt0-ia32.o
LIB_DIR    = ../../lib
CMDBIN_DIR = ../../rootfs/bin

SUBDIRS = test  simple dossrv 9660srv  ext2srv  \
	ip  usb  rc  exportfs  aux  lesson   rio

####################################################################
# this should be the very first rule
_default: all

####################################################################

CFLAGS	+= -Wall -Wno-format -O -freg-struct-return
CFLAGS		+= -fsigned-char -funsigned-bitfields

INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

SFLAGS	+= -DASSEMBLY

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib -L$(LIBGCC) -L$(LIB_DIR)


####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules
include ../Maketool


#### Linker ########################################################

LINKBASE = 0x00C00000
# LINKBASE = 0x00900000



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
# OBJ =  zz.o


all:   $(SUBDIRS) 

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	make -C $@


install:
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make install) \
	done


clean::
	rm -f *~ *.o   cmd.zip
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make clean) \
	done

