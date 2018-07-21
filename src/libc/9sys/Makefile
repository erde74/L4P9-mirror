### WHERE #################################
INC_DIR      = ../../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES   += $(INC_DIR) 

CPPFLAGS   += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	   += -fsigned-char -funsigned-bitfields

####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

## compile rules ###################################################
include ../../Maketool

#### Linker ########################################################
# LINKBASE = 0x00900000
LDFLAGS 	+= -nostdlib

## library rules ###################################################
# LIB_OBJ  = $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))
# $(LIB): $(LIB_OBJ)
#	$(AR) -rcs $(LIB) $(LIB_OBJ)

####################################################################
# dependencies
# .depend:	$(SRCS)
#	@echo Building dependencies in `pwd`
#	@$(CC) $(CPPFLAGS) -o - -M $(SRCS) > $@

##########################################
clean::	
	$(RM) -f *~  *.o  

#########################################
LIB = ../libc.a

OBJ =   abort.o\
        access.o\
        announce_.o\
        convD2M.o\
        convM2D.o\
        convM2S.o\
        convS2M.o\
        cputime.o\
        ctime.o\
        dial_.o\
        dirfstat.o\
        dirfwstat.o\
        dirmodefmt.o\
        dirread.o\
        dirstat.o\
        dirwstat.o\
        fcallfmt.o\
        fork.o\
        getnetconninfo.o\
        getenv.o\
        getpid_.o\
        getppid.o\
        getwd.o\
        iounit.o\
        nsec.o\
        nulldir.o\
        postnote.o\
        privalloc.o\
        pushssl.o\
        pushtls.o\
        putenv.o\
        qlock-l4.o\
        read.o\
        read9pmsg.o\
        readv.o\
        rerrstr.o\
        sbrk-l4.o\
        setnetmtpt.o\
        sysfatal.o\
        syslog.o\
        sysname.o\
        time.o\
        times.o\
        tm2sec.o\
        truerand.o\
        wait.o\
        waitpid.o\
        werrstr.o\
        write.o\
        writev.o\
	_BACKTRACE.o

TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	$(AR) -rcsv $(LIB) $(OBJ)

#	@$(AR) rvc $@ $(OBJ)
#	@$(RANLIB) $@

