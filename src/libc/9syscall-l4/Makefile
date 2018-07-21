### WHERE #################################
INC_DIR      = ../../../include

#### this should be the very first rule ################
_default: all

#### Platform specific parts ###########################
CFLAGS	+= -Wall -Wno-format -O  -freg-struct-return

LIBGCC	 = $(dir $(shell $(CC) --print-lib))

INCLUDES  += $(INC_DIR) 

CPPFLAGS  += -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	  += -fsigned-char -funsigned-bitfields

#### some evil magic ########################################
# ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
# DEFINES	+= NO_FRAME_POINTER
# endif	

##### compile rules ########################################
include  ../../Maketool

#### Linker ####################################################
LDFLAGS 	+= -nostdlib

################################################################
clean::	
	$(RM) -f *~  *.o  

#########################################
# SRCS  = $(wildcard *.c)

SRC2 = 	alarm_.c	await_.c	bind_.c		brk__.c \
	chdir_.c	close_.c	create_.c	dup_.c  \
	errstr_.c	exits_.c	fauth_.c	fd2path_.c \
	fstat_.c	fversion_.c	fwstat_.c	mount_.c \
	noted_.c	notify_.c	open_.c		oseek_.c \
	pipe_.c		pread_.c	pwrite_.c	remove_.c \
	rendezvous_.c 	seek_.c 	stat_.c 	unmount_.c \
	wstat_.c  \
	_read_.c	_write_.c	segattach_.c

#
# 	segattach_.c 	segbrk_.c 	segdetach_.c	segflush_.c	segfree_.c
# 	semacquire_.c	semrelease_.c
#
#	_errstr_.c 	_fsession_.c	_fstat_.c	_fwstat_.c
#	_mount_.c	_stat_.c		_wait_.c		_wstat_.c

OBJ2  = $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(SRC2)))

OBJS =  syscall_iii.o   sleep-l4.o \
	covering_fpage.o  _put_get.o  give_take.o \
	rfork-L4.o   spawn-l4.o  link-server.o \
	$(OBJ2)

#	syscalltry-l4.o   \

LIB = ../libc.a

TARGET  =   $(OBJS)

all:     $(LIB)

$(SRC2): ipc-l4.h  syscall-l4.c
	./Splitsrc  syscall-l4.c

newsrc:
	./Splitsrc syscall-l4.c

$(OBJS): ipc-l4.h   syscall-l4.c  $(SRC2)

$(LIB): $(OBJS)  
	$(AR) -rcsv $(LIB) $(OBJS)

#	@$(AR) rvc $@ $(OBJS)
#	@$(RANLIB) $@

rfork-l4tmp.s: rfork-l4tmp.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -S  $<

rf2.s: rf2.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -S  $<

