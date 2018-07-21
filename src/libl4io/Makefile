
LIBS	= libl4io.a 
SRCS	= x86-putc.c  x86-i586-getc.c  print.c  req2printsrv.o
# SRCS	= x86-i586-putc.c  x86-i586-getc.c  print.c  req2printsrv.o
OBJS	= $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(SRCS)))

INCLUDES  = ../../include 
# CPPFLAGS += -DUSE_L4_TYPES 
# LIBGCC    = $(dir $(shell $(CC) --print-lib))

CFLAGS    = -Wall -O  -nostdinc -I$(INCLUDES)
CFLAGS   += -funsigned-char -funsigned-bitfields

include  ../Maketool
# LD        = /usr/bin/ld
# AR        = /usr/bin/ar
# CC        = /usr/bin/gcc32

#############################################################	
TARGET  = libl4io.a

%.o:    %.S
	$(CC) $(CPPFLAGS) $(SFLAGS) -o $@ -c $<

%.o:    %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

all:    $(TARGET)
#	cp $(TARGET) ..

$(TARGET):      $(OBJS) Makefile
	$(AR) cvrs $@ $(OBJS)

clean::
	rm -f $(TARGET) *~ *.s *.o *.i *.ii

#############################################################	

