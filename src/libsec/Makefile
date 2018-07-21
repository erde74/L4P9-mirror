##############################################################
#          Makefile to make all lib-subdirectories           #
##############################################################

SUBDIRS = port/

all: $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	make -C $@

clean::
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make clean) \
	done
	rm -f libc.a 
	rm -f *~
