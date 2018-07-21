### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin	  

#  SUBDIRS = dossrv  ext2srv  ip

####################################################################
# this should be the very first rule
_default: all

####################################################################

CFLAGS	+= -Wall -Wno-format -O -freg-struct-return
CFLAGS		+= -fsigned-char -funsigned-bitfields

INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules
include ../../Maketool

#### Linker ########################################################

LINKBASE = 0x00C00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib -L$(LIBGCC) -L$(LIB_DIR)



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

TARGET  =  ramfs.gz  	import 	init 	ls 	cat 	echo \
	mkdir 	cpu  	sh  	tar 	srv 	cp\
	calendar 	cal 	wc 	sum 	tail 	date \
	chmod 	chgrp 	mv 	cmp 	awd     sleep   pbd  \
	pwd	unmount	mtime	basename	cleanname    \
	md5sum	sha1sum	touch	tee	webfsget	strings \
	p	read	fortune	factor	pipefile	time	\
	iconv	rm	seq	unlnfs	freq	unicode	\
	primes	ps	getmap	uniq		comm	ascii	\
	split	crop	du	ns	news	fmt	fcp	\
	colors	kbmap	rx	lens	winwatch	mc	\
	history	resample	idiff	look	tr	xd	\
	test	html2ms	pr	dd	deroff	 \
	bind    mount

all:  $(TARGET) 
	cp  $(TARGET)  $(CMDBIN_DIR)


install:  $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)


# .PHONY: $(SUBDIRS)
# $(SUBDIRS):
#	make -C $@


ramfs:  ramfs.o   ../lib/dbg9p.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -ll4io  -lc -lgcc -ll4 -ll4com 
	$(STRIP)  $@

import:  import.o   ../lib/dbg9p.o  # ../ip/dial.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -lauth -lbio -ll4io  -lc -lgcc -ll4 -ll4com 
	$(STRIP)  $@

init:  init.o  # ../aux/_newns.o    #####
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^ -lauth -lbio   -lc -lgcc -ll4 -ll4io -ll4com 
#	$(STRIP)  $@


ls: ls.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -ll4io  -lgcc -ll4 -ll4com 
	$(STRIP)  $@

cat: cat.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4io -lbio -lc -ll4io -lgcc -ll4 -ll4com 
	$(STRIP)  $@

echo: echo.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4io -lbio -lc -ll4io -lgcc -ll4 -ll4com 
#	$(STRIP)  $@

mkdir: mkdir.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4io -lbio -lc -ll4io -lgcc -ll4 -ll4com 
	$(STRIP)  $@

ex1: ex1.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4io  -lc -lgcc -ll4 -ll4com 
	$(STRIP)  $@

cp: cp.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4io  -lc -lgcc -ll4 -ll4com -ll4io
	$(STRIP)  $@

cpu: cpu.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -ll4io  -lauth -lbio -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

tar: tar.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

sh: sh.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

date: date.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

chmod: chmod.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

chgrp: chgrp.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

mv: mv.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

cmp: cmp.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

awd: awd.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

calendar: calendar.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString -lbio -lregexp -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

cal: cal.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString -lbio -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

wc: wc.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

sum: sum.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

tail: tail.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString -lbio -lc -lgcc -ll4 -ll4com -ll4io 
	$(STRIP)  $@

sleep: sleep.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

pbd: pbd.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

pwd: pwd.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

unmount: unmount.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

mtime: mtime.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

basename: basename.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

cleanname: cleanname.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

md5sum: md5sum.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lsec -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

sha1sum: sha1sum.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lsec -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

touch: touch.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

tee: tee.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@


webfsget: webfsget.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

strings: strings.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

p: p.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

read: read.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

fortune: fortune.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

factor: factor.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

pipefile: pipefile.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

time: time.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

iconv: iconv.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lmemdraw -ldraw -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

rm: rm.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

seq: seq.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

unlnfs: unlnfs.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lsec -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

freq: freq.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

unicode: unicode.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

primes: primes.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

ps: ps.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

getmap: getmap.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

uniq: uniq.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

colors: colors.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lmemdraw -ldraw -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

comm: comm.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

ascii: ascii.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

split: split.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lregexp -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

crop: crop.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lmemdraw -ldraw  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

du: du.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

ns: ns.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

news: news.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

fcp: fcp.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

fmt: fmt.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

kbmap: kbmap.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lmemdraw -ldraw -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

rx: rx.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lauth -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

lens: lens.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lmemdraw -ldraw -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

winwatch: winwatch.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lregexp -lmemdraw -ldraw -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

mc: mc.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lmemdraw -ldraw -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

srv: srv.o  # ../ip/dial.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^ -lString  -lc -lgcc -ll4 -ll4com -ll4io -lc
	$(STRIP)  $@

history: history.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

resample: resample.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lmemdraw -ldraw -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

idiff: idiff.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

look: look.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

tr: tr.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

xd: xd.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

test: test.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

html2ms: html2ms.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

pr: pr.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

dd: dd.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

deroff: deroff.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

bind: bind.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

mount: mount.o
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
		$(CRT0) $^  -lbio -lc -lgcc -ll4 -ll4com -ll4io -lc -lgcc
	$(STRIP)  $@

clean::
	rm -f *~ *.o $(TARGET) $(TARGET).u $(TARGET).gz

#	@for d in $(SUBDIRS); do \
#	  (cd $${d} && make clean) \
#	done




