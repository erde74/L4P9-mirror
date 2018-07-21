### WHERE #################################
INC_DIR      = ../../include

####################################################################
# this should be the very first rule
_default: all

####################################################################
# Platform specific parts

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return

####################################################################
SHELL	 = bash
RM	 = rm
LN_S	 = ln -s
CC	 = gcc
LD	 = ld
OBJCOPY	 = objcopy
STRIP	 = strip
AR	 = ar
RANLIB   = ranlib
GZIP	 = gzip

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
# SFLAGS   += -DASSEMBLY
# LDFLAGS  += -L$(LIBGCC) -L$(L4MX_LIB_DIR)
# DEFINES  += __ARCH__=$(ARCH)   __PLATFORM__=$(PLATFORM)

#### Compiler ######################################################
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

%.o:    %.S
	$(CC) $(CPPFLAGS) $(SFLAGS) -o $@ -c $<

%.o:	%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

%.gz:   %
	$(GZIP)  $<

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
	$(RM) -f *~  *.o  libdraw.a

#########################################
LIB = libdraw.a


OBJ =   alloc.o\
        allocimagemix.o\
        arith.o\
        bezier.o\
        border.o\
        buildfont.o\
        bytesperline.o\
        chan.o\
        cloadimage.o\
        computil.o\
        creadimage.o\
        debug.o\
        defont.o\
        draw.o\
        drawrepl.o\
        egetrect.o\
        ellipse.o\
        emenuhit.o\
        event.o\
        fmt.o\
        font.o\
        freesubfont.o\
        getdefont.o\
        getrect.o\
        getsubfont.o\
        icossin.o\
        icossin2.o\
        init.o\
        line.o\
        menuhit.o\
        mkfont.o\
        newwindow.o\
        openfont.o\
        poly.o\
        loadimage.o\
        readcolmap.o\
        readimage.o\
        readsubfont.o\
        rectclip.o\
        replclipr.o\
        rgb.o\
        scroll.o\
        string.o\
        stringsubfont.o\
        stringwidth.o\
        subfont.o\
        subfontcache.o\
        subfontname.o\
        unloadimage.o\
        window.o\
        writecolmap.o\
        writeimage.o\
        writesubfont.o\
        keyboard.o\
        mouse.o\
#        stringbg.o\


TARGET  =   $(OBJ)

all:     $(LIB)

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@


#	$(AR) -rcs $(LIB) $(OBJ)


# ramfs  ???
