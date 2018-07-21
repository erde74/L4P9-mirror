#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

/*
 * simplistic permission checking.  assume that
 * each user is the leader of her own group.
 */
int
hasperm(File *f, char *uid, int p)
{
	int m;

	m = f->_dir.mode & 7;	/* other */   //% _dir.
	if((p & m) == p)
		return 1;

	if(strcmp(f->_dir.uid, uid) == 0) {    //% _dir.
	        m |= (f->_dir.mode>>6) & 7;    //% _dir.
		if((p & m) == p)
			return 1;
	}

	if(strcmp(f->_dir.gid, uid) == 0) {   //% _dir.
	        m |= (f->_dir.mode>>3) & 7;   //%  _dir.
		if((p & m) == p)
			return 1;
	}

	return 0;
}
