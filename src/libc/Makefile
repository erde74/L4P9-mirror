##############################################################
#          Makefile to make all lib-subdirectories           #
##############################################################

SUBDIRS = 386/  9sys/  9syscall-l4/  fmt/  port/  

# Recursive make (make -C ) is used, but 386/*.o {_tas.o, 
# setjmp-l4.o  cmpxchg.o} are not included in libc.a. Why ?
#
# "REMAKELIB = yes" remakes the libc.a
#

REMAKELIB =  no

####################################################################
AR       = ar
RANLIB   = ranlib
MV       = mv

ifeq ($(REMAKELIB), yes) ###########################################

OBJ = $(foreach dir, $(SUBDIRS), $(patsubst %.c,%.o,$(wildcard $(dir)/*.c)))
OBJ += $(foreach dir, $(SUBDIRS), $(patsubst %.S,%.o,$(wildcard $(dir)/*.S)))

LIB = libc2.a

all: $(SUBDIRS)  $(LIB)

.PHONY: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

$(LIB): $(OBJ)
	@$(AR) rvc $@ $(OBJ)
	@$(RANLIB) $@
	@$(MV)  libc.a  libc.a.save
	@$(MV)  libc2.a  libc.a

else ##############################################################

all: $(SUBDIRS)

.PHONY: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

# all:
#	cd 9sys; $(MAKE) 
#	cd 9syscall-l4; $(MAKE) 
#	cd fmt; $(MAKE) 
#	cd port; $(MAKE) 
#	cd 386; $(MAKE) 


endif #############################################################

clean::
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make clean) \
	done
	rm -f *~  libc.a  libc.a.save

