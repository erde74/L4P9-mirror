##############################################################
#          Makefile to make all lib-subdirectories           #
##############################################################
CMDBIN_DIR  = ../rootfs/bin
BOOTBIN_DIR = ../rootfs/boot

include    Maketool

SUBDIRS = libl4com/  \
	libl4io/    \
	libc/       \
	libip/      \
	libsec/     \
	lib9p/      \
	libString   \
	libdbg/     \
	libdraw/    \
	libframe/   \
	libndb/     \
	libactobj   \
	9/hvm/      \
	9/pc/       \
	9/init/      \
	9/qsh/      \
	cmd/  	    

SUBDIRS2 =  9/hvm/      \
	9/pc/       \
	9/qsh/      \
	9/init/      \
	cmd/  	    


CMDS  =   dos2  echosrv  exportfs \
	import  ping  usbsfs  echo  \
	ex1  ex2   ex4  \
	ext2srv  listen1  usbd  sh  init

all: $(SUBDIRS)


.PHONY: $(SUBDIRS)
$(SUBDIRS):
	make -C $@


install:
	@for d in $(SUBDIRS2); do \
	  (cd $${d} && make install) \
	done
	$(ZIP)  $(CMDBIN_DIR)/cmd.zip   $(CMDBIN_DIR)/$(CMDS)

cd:
	mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
	  -boot-load-size 4 -boot-info-table -o ../lp49-boot.cd  ../rootfs

clean::
	@for d in $(SUBDIRS); do \
	  (cd $${d} && make clean) \
	done
	rm -f libc.a 
	rm -f *~
