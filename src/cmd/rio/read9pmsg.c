//%%%
#include <u.h>
#include <libc.h>
#include <fcall.h>

//%------------------------
#include  <l4all.h>
#include  <l4/l4io.h>
#define   _DBGFLG  0
#define  DBGPRN  if(_DBGFLG)print

static void _dump_mem(char *title, unsigned char *start, unsigned size)
{
    int  i, j, k;
    unsigned char c;
    char  buf[128];
    char  elem[32];

    if(size == 0){
        print(" .");
        return ;
    }

    print(" dump_mem<%s %x %d>\n", title, start, size);
    if (size>1024) size = 1024;

    for(i=0; i<size; i+=16) {
        buf[0] = 0;
	for (j=i; j<i+16; j+=4) {
	    if (j%4 == 0) strcat(buf, " ");

	    for(k=3; k>=0; k--) {
	        c = start[j+k];
		l4snprintf(elem, 32, "%.2x", c);
		strcat(buf, elem);
	    }
	}
	
	strcat(buf, "  ");
	for (j=i; j<i+16; j++) {
	    c = start[j];
	    if ( c >= ' ' && c < '~')
	        l4snprintf(elem, 32, "%c", c);
	    else
	        l4snprintf(elem, 32, ".");
	    strcat(buf, elem);
	}
	print("%s\n", buf);
    }
}
//%--------------------------

int
read9pmsg(int fd, void *abuf, uint n)
{
	int m, len;
	uchar *buf;

	buf = abuf;
	m = readn(fd, buf, BIT32SZ);	/* read count */
	if(m != BIT32SZ){
	        if(m < 0) {   
		        print("  Err1-read9pmsg:-1 \n"); 
			return -2; // -1
		}
		print("  Err2-read9pmsg:0 \n"); 
		return 0;
	}

//_dump_mem("9p", buf, 0);
sleep(50);

	len = GBIT32(buf);
	if(len <= BIT32SZ || len > n){
	        _dump_mem("##", buf, 16);
		werrstr("read9pmsg: bad length in 9P2000 message header (len=%d n=%d)\n", 
			len, n);
		print("  Err3-read9pmsg:-1 \n"); 
		return -3; // -1
	}
	len -= BIT32SZ;
	m = readn(fd, buf+BIT32SZ, len);
	if(m < len) {   
	        print("  Err4-readp9msg:0 \n"); 
		return -4;  // 0
	}

	DBGPRN("< %s()-> %d \n", __FUNCTION__,  BIT32SZ+m); 
	if((BIT32SZ+m)<0)print("\n  readp9msg:%d \n", BIT32SZ+m); 
	return BIT32SZ+m;
}
