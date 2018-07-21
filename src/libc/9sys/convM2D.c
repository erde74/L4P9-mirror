#include	<u.h>
#include	<libc.h>
#include	<fcall.h>


int
statcheck(uchar *buf, uint nbuf)
{
	uchar *ebuf;
	int i;

	ebuf = buf + nbuf;

	if(nbuf < STATFIXLEN || nbuf != BIT16SZ + GBIT16(buf))
		return -1;

	buf += STATFIXLEN - 4 * BIT16SZ;

	for(i = 0; i < 4; i++){
		if(buf + BIT16SZ > ebuf)
			return -1;
		buf += BIT16SZ + GBIT16(buf);
	}

	if(buf != ebuf)
		return -1;

	return 0;
}

static char nullstring[] = "";

uint
convM2D(uchar *buf, uint nbuf, Dir *d, char *strs)
{
	uchar *p, *ebuf;
	char *sv[4];
	int i, ns;

	if(nbuf < STATFIXLEN)
		return 0; 

	p = buf;
	ebuf = buf + nbuf;

	p += BIT16SZ;	/* ignore size */
	d->type = GBIT16(p);
	p += BIT16SZ;
	d->dev = GBIT32(p);
	p += BIT32SZ;
	d->qid.type = GBIT8(p);
	p += BIT8SZ;
	d->qid.vers = GBIT32(p);
	p += BIT32SZ;
	d->qid.path = GBIT64(p);
	p += BIT64SZ;
	d->mode = GBIT32(p);
	p += BIT32SZ;
	d->atime = GBIT32(p);
	p += BIT32SZ;
	d->mtime = GBIT32(p);
	p += BIT32SZ;
	d->length = GBIT64(p);
	p += BIT64SZ;

	for(i = 0; i < 4; i++){
		if(p + BIT16SZ > ebuf)
			return 0;
		ns = GBIT16(p);
		p += BIT16SZ;
		if(p + ns > ebuf)
			return 0;
		if(strs){
			sv[i] = strs;
			memmove(strs, p, ns);
			strs += ns;
			*strs++ = '\0';
		}
		p += ns;
	}

	if(strs){
		d->name = sv[0];
		d->uid = sv[1];
		d->gid = sv[2];
		d->muid = sv[3];
	}else{
		d->name = nullstring;
		d->uid = nullstring;
		d->gid = nullstring;
		d->muid = nullstring;
	}
	
	return p - buf;
}


/* <Input>
 *  buf
 *  +-----------------------------------------+
 *  |                                         |
 *  +-----------------------------------------+
 *
 * <Result>
 *  d
 *  +--------+      strs
 *  |        |      +------------------------------------+
 *  |        |      |name...\0|uid...\0|gid...\0|muid..\0| 
 *  :        :      +---------+--------+--------+--------+
 *  |        |      ^         ^
 *  +--------+      |         |
 *  | name   |------          | 
 *  +--------+                |
 *  | uid    |---------------- 
 *  +--------+
 *  | gid    |
 *  +--------+
 *  | muid   |
 *  +--------+
 *
 *   struct Dir {
 *     ushort  type;   
 *     uint    dev;    
 *     Qid     qid;   
 *     ulong   mode;  
 *     ulong   atime; 
 *     ulong   mtime; 
 *     vlong   length;
 *     char    *name; 
 *     char    *uid;  
 *     char    *gid;  
 *     char    *muid; 
 *   } Dir;
 *
 *   #define QIDSZ   (BIT8SZ+BIT32SZ+BIT64SZ)
 *
 *   STATFIXLEN includes leading 16-bit count.
 *   The count, however, excludes itself; total size is BIT16SZ+count 
 *  #define STATFIXLEN   (BIT16SZ+QIDSZ+5*BIT16SZ+4*BIT32SZ+1*BIT64SZ) 
 */
