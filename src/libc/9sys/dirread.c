//%%%%%%%%%%%
#include <u.h>
#include <libc.h>
#include <fcall.h>

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>

//%-------
#undef  DIRMAX
#define DIRMAX  2048
//%-------

static
long
dirpackage(uchar *buf, long ts, Dir **d)
{
	char *s;
	long ss, i, n, nn, m;
	DBGPRN("> %s(%x %x %x)\n", __FUNCTION__, buf, ts, *d);

	*d = nil;
	if(ts <= 0)
		return 0;

	/*
	 * first find number of all stats, check they look like stats, & size all associated strings
	 */
	ss = 0;
	n = 0;
	for(i = 0; i < ts; i += m){
		m = BIT16SZ + GBIT16(&buf[i]);
		if(statcheck(&buf[i], m) < 0)
			break;
		ss += m;
		n++;
	}

	if(i != ts)
		return -1;

	*d = malloc(n * sizeof(Dir) + ss);
	if(*d == nil)
		return -1;

	/*
	 * then convert all buffers
	 */
	s = (char*)*d + n * sizeof(Dir);
	nn = 0;
	for(i = 0; i < ts; i += m){
		m = BIT16SZ + GBIT16((uchar*)&buf[i]);
		if(nn >= n || convM2D(&buf[i], m, *d + nn, s) != m){
			free(*d);
			*d = nil;
			return -1;
		}
		// DBGPRN("# Dir \"%s\" qid=%x  ",
		//     (*d+nn)->name, (uint)((*d+nn)->qid.path));
		nn++;
		s += m;
	}
	// DBGPRN("<%d>\n", nn);
	return nn;
}

long
dirread(int fd, Dir **d)
{
	uchar *buf;
	long ts;
	DBGPRN("> %s(%x %x) \n", __FUNCTION__, fd, *d);

	buf = malloc(DIRMAX);
	if(buf == nil)
		return -1;
	ts = read(fd, buf, DIRMAX);
	if(ts >= 0)
		ts = dirpackage(buf, ts, d);
	free(buf);
	return ts;
}

long
dirreadall(int fd, Dir **d)
{
	uchar *buf, *nbuf;
	long n, ts;
	DBGPRN("> %s(%x %x) \n", __FUNCTION__, fd, *d);

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
