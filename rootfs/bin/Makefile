##############################################################
#                    Makefile                                #
##############################################################


CMNDS  =   dos2  echosrv  exportfs  \
	import  ping  usbsfs  echo  \
	ex1  ex2     \
	ext2srv  listen1  usbd  sh  init

TARGET = cmd.zip


all:  $(CMNDS)
	zip  cmd.zip   $(CMNDS)


clean::
	rm -f cmd.zip
	rm -f *~

#	rm -f [a-z]* 
