//%%%%%%%%%%%   src/libc/9sys/dirread.c 
#include <u.h>
#include <libc.h>
#include <fcall.h>

//%---------------------------------
#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b

#undef  DIRMAX
#define DIRMAX  2048
//%------------------------------
/*
 *    Following is used instead of "BIT16SZ + GBIT16(&buf[i])".   (KM)
 */
static int dir_size(uchar *buf)
{
    uchar  *start = buf;
    int    i, sz;

    buf += STATFIXLEN - 4 * BIT16SZ;
    for(i = 0; i < 4; i++){
        buf += BIT16SZ + GBIT16(buf);
    }
    sz = buf - start;  // PRN("Dir-size:%d \n", sz); 
    return sz;
}


static long dirpackage(uchar *buf, long ts, Dir **d)
{
	char *s;
	long ss, i, n, nn, m, m2;
	DBGPRN(">%s(%x %d %x)\n", __FUNCTION__, buf, ts, *d);

	*d = nil;
	if(ts <= 0)
		return 0;

	/*
	 * first find number of all stats, check they look like stats, & size all associated strings
	 */
	ss = 0;
	n = 0;
	for(i = 0; i < ts; i += m){
#if 1 //-----------------------
		m2 = BIT16SZ + GBIT16(&buf[i]);
		m = dir_size(&buf[i]);
		//  l4printf("%s  %d%c \t", &buf[i+43], m, (m==m2)?' ':'!');
#else //-----------------------
		m = BIT16SZ + GBIT16(&buf[i]);
		if(statcheck(&buf[i], m) < 0) 
		  	break;
#endif //---------------------
		ss += m;
		n++;
	}
	DBGPRN("STATFIXLEN=%d DirSz=%d QidSz=%d\n", STATFIXLEN, sizeof(Dir), sizeof(Qid));

	if(i != ts) {	        
	        PRN("dirpackage(i(=%d) : ts(=%d)) \n", i, ts);
		return -1;
	}

	*d = malloc(n * sizeof(Dir) + ss);
	if(*d == nil) 
		return -1;

	/*
	 * then convert all buffers
	 */
	s = (char*)*d + n * sizeof(Dir);
	nn = 0;
	for(i = 0; i < ts; i += m){
#if 1 //-----------------------------------
		m = dir_size(&buf[i]);
#else //-----------------------------------
		m = BIT16SZ + GBIT16((uchar*)&buf[i]);
#endif //---------------------------------
		if(nn >= n || convM2D(&buf[i], m, *d + nn, s) != m){
			free(*d);
			*d = nil;
			PRN("dirpackage#10\n"); 
			return -1;
		}
		// DBGPRN("# Dir \"%s\" qid=%x  ",
		//     (*d+nn)->name, (uint)((*d+nn)->qid.path));
		nn++;
		s += m;
	}
	DBGPRN("<%s():%d>\n", __FUNCTION__, nn);
	return nn;
}

long dirread(int fd, Dir **d)
{
	uchar *buf;
	long ts;
	DBGPRN(">%s(%x %x) \n", __FUNCTION__, fd, *d);

	buf = malloc(DIRMAX);
	if(buf == nil)
		return -1;
	ts = read(fd, buf, DIRMAX);
	if(ts >= 0)
		ts = dirpackage(buf, ts, d);
	free(buf);
	return ts;
}


long dirreadall(int fd, Dir **d)
{
	uchar *buf, *nbuf;
	long n, ts;
	DBGPRN(">%s(%x %x) \n", __FUNCTION__, fd, *d);

	buf = nil;
	ts = 0;

	for(;;){
		nbuf = realloc(buf, ts+DIRMAX);
		if(nbuf == nil){
			free(buf);
			return -1;
		}
		buf = nbuf;
		n = read(fd, buf+ts, DIRMAX);  
		DBGPRN(" dirreadall<%d:%d>%s \n", n, buf[ts+1]<<8+buf[ts],  &buf[43+ts]); 
		// dump_mem("* ", buf+ts, 64); //%

		if(n <= 0)
			break;
		ts += n;
	}

	if(ts >= 0)
		ts = dirpackage(buf, ts, d);
	free(buf);
	if(ts == 0 && n < 0)
		return -1;
	return ts;
}
