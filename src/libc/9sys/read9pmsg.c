//%%%
#include <u.h>
#include <libc.h>
#include <fcall.h>

//%------------------------
#include  <l4all.h>
#include  <l4/l4io.h>
#define   _DBGFLG  0
#define  DBGPRN  if(_DBGFLG)print
//%--------------------------

int
read9pmsg(int fd, void *abuf, uint n)
{
	int m, len;
	uchar *buf;
	DBGPRN("> %s(%d %x %d)\n", __FUNCTION__, fd, abuf, n);

	buf = abuf;

	/* read count */
	m = readn(fd, buf, BIT32SZ);
	if(m != BIT32SZ){
	        if(m < 0) {   
		        print("%s-Er1 ", __FUNCTION__); 
			return -1;
		}
		print("%s-Er2 ", __FUNCTION__); 
		return 0;
	}

	len = GBIT32(buf);
	if(len <= BIT32SZ || len > n){
	  /*print*/werrstr("read9pmsg: bad length in 9P2000 message header (len=%d n=%d)\n", len, n);
		return -1;
	}
	len -= BIT32SZ;
	m = readn(fd, buf+BIT32SZ, len);
	if(m < len) {   
	        print("%s-Er4 ", __FUNCTION__); 
		return 0;
	}

	DBGPRN("< %s()-> %d \n", __FUNCTION__,  BIT32SZ+m); 
	return BIT32SZ+m;
}
