### WHERE #################################
INC_DIR      = ../../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts

CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 

CPPFLAGS  += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	  += -fsigned-char -funsigned-bitfields

####################################################################
# some evil magic
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

####################################################################
# compile rules
include ../../Maketool

#### Linker ########################################################
LDFLAGS 	+= -nostdlib

####################################################################
# library rules
# LIB_OBJ  = $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(LIB_SRC)))

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
	$(RM) -f *~  *.o 

#########################################
LIB = ../libc.a

OBJ =   _assert.o\
        abs.o\
        asin.o\
        atan.o\
        atan2.o\
        atexit.o\
        atnotify.o\
        atof.o\
        atol.o\
        atoll.o\
        cistrcmp.o\
        cistrncmp.o\
        cistrstr.o\
        charstod.o\
        cleanname.o\
        crypt.o\
        ctype.o\
        encodefmt.o\
        execl.o\
        exp.o\
        fabs.o\
        floor.o\
        fmod.o\
        frand.o\
        frexp.o\
        getcallerpc.o\
        getfields.o\
        getuser.o\
        hangup.o\
        hypot.o\
        lnrand.o\
        lock-l4.o\
        log.o\
        lrand.o\
        memccpy.o\
        memchr.o\
        memcmp.o\
        memmove.o\
        memset.o\
        mktemp.o\
        muldiv.o\
        nan.o\
        needsrcquote.o\
        netcrypt.o\
        netmkaddr.o\
        nrand.o\
        ntruerand.o\
        perror.o\
        pow.o\
        pow10.o\
        qsort.o\
        quote.o\
        rand.o\
        readn.o\
        rune.o\
        runestrcat.o\
        runestrchr.o\
        runestrcmp.o\
        runestrcpy.o\
        runestrecpy.o\
        runestrdup.o\
        runestrncat.o\
        runestrncmp.o\
        runestrncpy.o\
        runestrrchr.o\
        runestrlen.o\
        runestrstr.o\
        runetype.o\
        sin.o\
        sinh.o\
        sqrt.o\
        strcat.o\
        strchr.o\
        strcmp.o\
        strcpy.o\
        strecpy.o\
        strcspn.o\
        strdup.o\
        strlen.o\
        strncat.o\
        strncmp.o\
        strncpy.o\
        strpbrk.o\
        strrchr.o\
        strspn.o\
        strstr.o\
        strtod.o\
        strtok.o\
        strtol.o\
        strtoll.o\
        strtoul.o\
        strtoull.o\
        tan.o\
        tanh.o\
        tokenize.o\
        toupper.o\
        utfecpy.o\
        utflen.o\
        utfnlen.o\
        utfrune.o\
        utfrrune.o\
        utfutf.o\
        u16.o\
        u32.o\
        u64.o\
        pool.o\
        malloc.o\
	argv0-l4.o \
	profile.o\


TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	$(AR) -rcsv $(LIB) $(OBJ)

#	@$(AR) rvc $@ $(OBJ)
#	@$(RANLIB) $@





