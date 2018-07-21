### WHERE #################################
INC_DIR    = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib


####################################################################
# this should be the very first rule
_default: all

####################################################################
include  ../../Maketool

#### Compiler ######################################################


all:    cmd.zip


cmd.zip:
	$(ZIP)  cmd.zip  \
	  dos2  echosrv  exportfs \
	  import  ping  usbsfs  echo  \
	  ex1  ex2   ex4  \
	  ext2srv  listen1  usbd  sh  init

install: 	
	echo skip

clean::
	rm -f *~  cmd.zip





