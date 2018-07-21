##############################################################
#          Makefile to make all lib-subdirectories           #
##############################################################

SUBDIRS = lib/    usbd/  usbsfs/   # misc/

all: $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	make -C $@


install:
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make install) \
	done


clean::
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make clean) \
	done
	rm -f libc.a 
	rm -f *~
