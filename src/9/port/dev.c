//%%%%   dev.c   %%%%%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

//%---------------------------------------------
#include        "../errhandler-l4.h"

#define   _DBGFLG  0 
#include  <l4/_dbgpr.h>
#define   PRN  l4printf_b
#define  print  l4printf

char *_dbg_names(char **names, int num);
char *_dbg_dirtabnames(Dirtab *tab, int num);

extern int   l4printf(const char *fmt, ...);
//%---------------------------------------------

extern ulong	kerndate;

void mkqid(Qid *q, vlong path, ulong vers, int type)
{
  //   DBGPRN("> %s  \n", __FUNCTION__);
	q->type = type;
	q->vers = vers;
	q->path = path;
}


int devno(int c, int user)
{
	int i;
	//   DBGPRN("> %s  \n", __FUNCTION__);
	for(i = 0; devtab[i] != nil; i++) {
		if(devtab[i]->dc == c)
			return i;
	}
	if(user == 0)
		panic("devno %C 0x%ux", c, c);

	return -1;
}


void devdir(Chan *c, Qid qid, char *n, vlong length, char *user, long perm, Dir *db)
{
        DBGPRN("%s(%s, %x, %s) \n", 
	    __FUNCTION__, (c && c->path)?c->path->s:"-", (ulong)qid.path, n );

	db->name = n;
	if(c->flag&CMSG)
		qid.type |= QTMOUNT;
	db->qid = qid;
	db->type = devtab[c->type]->dc;
	db->dev = c->dev;
	db->mode = perm;
	db->mode |= qid.type << 24;
	db->atime = seconds();
	//%	db->mtime = kerndate;
	db->length = length;
	db->uid = user;
	db->gid = eve;
	db->muid = user;
}


/*
 * (here, Devgen is the prototype; devgen is the function in dev.c.)
 * 
 * a Devgen is expected to return the directory entry for ".."
 * if you pass it s==DEVDOTDOT (-1).  otherwise...
 * 
 * there are two contradictory rules.
 * 
 * (i) if c is a directory, a Devgen is expected to list its children
 * as you iterate s.
 * 
 * (ii) whether or not c is a directory, a Devgen is expected to list
 * its siblings as you iterate s.
 * 
 * devgen always returns the list of children in the root
 * directory.  thus it follows (i) when c is the root and (ii) otherwise.
 * many other Devgens follow (i) when c is a directory and (ii) otherwise.
 * 
 * devwalk assumes (i).  it knows that devgen breaks (i)
 * for children that are themselves directories, and explicitly catches them.
 * 
 * devstat assumes (ii).  if the Devgen in question follows (i)
 * for this particular c, devstat will not find the necessary info.
 * with our particular Devgen functions, this happens only for
 * directories, so devstat makes something up, assuming
 * c->name, c->qid, eve, DMDIR|0555.
 * 
 * devdirread assumes (i).  the callers have to make sure
 * that the Devgen satisfies (i) for the chan being read.
 */

/*
 * the zeroth element of the table MUST be the directory itself for ..
 */
int devgen(Chan *c, char *name, Dirtab *tab, int ntab, int i, Dir *dp)
{
        DBGPRN(">%s(\"%s\" \"%s\" %x %x %x %x) \n", 
	       __FUNCTION__, c->path->s, name, tab, ntab, i, dp);
#if 1 //----- this may be easier to understand. -------------- 

	if(tab == 0)
		return -1;

	if(i == DEVDOTDOT){
	        devdir(c, tab[0].qid, tab[0].name, 
		       tab[0].length, eve, tab[0].perm, dp);
		return  1;
	}
	else 
	if(name){
		for(i=1; i<ntab; i++)
		        if(strcmp(tab[i].name, name) == 0){
			        devdir(c, tab[i].qid, tab[i].name, 
				       tab[i].length, eve, tab[i].perm, dp);
				return  1;
			}
		return -1;
	}
	else{
		/* skip over the first element, that for . itself */
		if(i+1 >= ntab)
			return -1;
		devdir(c, tab[i+1].qid, tab[i+1].name, 
		       tab[i+1].length, eve, tab[i+1].perm, dp);
		return  1;
	}
#else //original-------------------------
	if(tab == 0)
		return -1;
	if(i == DEVDOTDOT){
		/* nothing */
	}else if(name){
		for(i=1; i<ntab; i++)
			if(strcmp(tab[i].name, name) == 0)
				break;
		if(i==ntab)
			return -1;
		tab += i;
	}else{
		/* skip over the first element, that for . itself */
		i++;
		if(i >= ntab)
			return -1;
		tab += i;
	}
	devdir(c, tab->qid, tab->name, tab->length, eve, tab->perm, dp);
	return 1;
#endif //---------------------------------------------
}

void devreset(void)
{
}

void devinit(void)
{
}

void devshutdown(void)
{
}

Chan* devattach(int tc, char *spec)
{
	Chan *c;
	char *buf;

        DBGPRN("> %s(\'%c\' %s)  \n", __FUNCTION__, tc, spec);

	c = newchan();
	mkqid(&c->qid, 0, 0, QTDIR);
	c->type = devno(tc, 0);
	if(spec == nil)
		spec = "";
	buf = smalloc(4+strlen(spec)+1);
	sprint(buf, "#%c%s", tc, spec);
	// sprint(buf, "#%C%s", tc, spec); //original
	// DBGPRN("*** %s ***\n", buf);
	c->path = newpath(buf);
	free(buf);
	// pr_chan(c); 
	return c;
}


Chan* devclone(Chan *c)
{
	Chan *nc;
        DBGPRN("> %s(\"%s\")  \n", __FUNCTION__, c->path->s);
	if(c->flag & COPEN)
		panic("clone of open file type %C\n", devtab[c->type]->dc);

	nc = newchan();

	nc->type = c->type;
	nc->dev = c->dev;
	nc->mode = c->mode;
	nc->qid = c->qid;
	nc->offset = c->offset;
	nc->umh = nil;
	nc->_u.aux = c->_u.aux;  //%
	nc->mqid = c->mqid;
	nc->mcp = c->mcp;
	return nc;
}


Walkqid* devwalk(Chan *c, Chan *nc, char **name, int nname, Dirtab *tab, int ntab, Devgen *gen) //% ONERR
{
	int i, j, alloc;
	Walkqid *wq;
	char *n;
	Dir dir;

        DBGPRN(">%s(%s %x %s %s %x)\n", __FUNCTION__, 
	       c->path->s, nc, _dbg_names(name, nname), _dbg_dirtabnames(tab, ntab), gen);

	if(nname > 0){
#if 1 //%
	        int  rc = isdir(c);
		IF_ERR_RETURN(rc, ONERR, ONERRNIL);
#else //%
	        isdir(c);
#endif //%
	}

	alloc = 0;
	wq = smalloc(sizeof(Walkqid)+(nname-1)*sizeof(Qid));

	if(WASERROR()){
	_ERR_1:   //%
		if(alloc && wq->clone!=nil)
			cclose(wq->clone);
		free(wq);
		return nil;
	}

	if(nc == nil){
		nc = devclone(c);
		nc->type = 0;	/* device doesn't know about this channel yet */
		alloc = 1;
	}
	wq->clone = nc;

	for(j=0; j<nname; j++){
		if(!(nc->qid.type&QTDIR)){
			if(j==0)
			        ERROR_GOTO(Enotdir, _ERR_1);  //%
			goto Done;
		}

		n = name[j];
		if(strcmp(n, ".") == 0){
    Accept:
			wq->qid[wq->nqid++] = nc->qid;
			continue;
		}

		if(strcmp(n, "..") == 0){
			if((*gen)(nc, nil, tab, ntab, DEVDOTDOT, &dir) != 1){
				print("devgen walk .. in dev%s %llux broken\n",
					devtab[nc->type]->name, nc->qid.path);
				ERROR_GOTO("broken devgen", _ERR_1); //%
			}

			nc->qid = dir.qid;
			//%  "goto Accept;" is replaced by:
			wq->qid[wq->nqid++] = nc->qid;  //%
                        continue;                       //%
		}

		/*
		 * Ugly problem: If we're using devgen, make sure we're
		 * walking the directory itself, represented by the first
		 * entry in the table, and not trying to step into a sub-
		 * directory of the table, e.g. /net/net. Devgen itself
		 * should take care of the problem, but it doesn't have
		 * the necessary information (that we're doing a walk).
		 */

		if(gen==devgen && nc->qid.path!=tab[0].qid.path){
#if 1 //%------------
		        if(j == 0) {
		              goto _ERR_1; //% save err-msg
			}
			kstrcpy(up->errstr, Enonexist, ERRMAX);
			goto Done;

#else //%-----------
			goto Notfound;
#endif //%----------
		}

		for(i=0;; i++) {
			switch((*gen)(nc, n, tab, ntab, i, &dir)){
			case -1:
			Notfound:

				if(j == 0) {
#if 0 //%-----------------------
				        goto _ERR_1; //% save err-msg
#else //---------------------
				        ERROR_GOTO(Enonexist, _ERR_1); //% Thanks to Mr. Sato 
#endif //---------------------					
				}
				kstrcpy(up->errstr, Enonexist, ERRMAX);
				goto Done;

			case 0:
				continue;

			case 1:
				if(strcmp(n, dir.name) == 0){
					nc->qid = dir.qid;

					goto Accept;  
				}
				continue;
			}
		}
	}
	/*
	 * We processed at least one name, so will return some data.
	 * If we didn't process all nname entries succesfully, we drop
	 * the cloned channel and return just the Qids of the walks.
	 */
Done:
	POPERROR();
	if(wq->nqid < nname){
		if(alloc)
			cclose(wq->clone);
		wq->clone = nil;
	}else if(wq->clone){
		/* attach cloned channel to same device */
		wq->clone->type = c->type;
	}
	return wq;
}


int devstat(Chan *c, uchar *db, int n, Dirtab *tab, int ntab, Devgen *gen) //% ONERR
{
	int i;
	Dir dir;
	char *p, *elem;
        DBGBRK("> %s (%s %x %x %x %x %x ) \n", 
	       __FUNCTION__, c->path->s, db, n, tab, ntab, gen);

	for(i=0;; i++){
		switch((*gen)(c, nil, tab, ntab, i, &dir)){
		case -1:  // DBGPRN("@ -1 \n");
			if(c->qid.type & QTDIR){
				if(c->path == nil)
					elem = "???";
				else if(strcmp(c->path->s, "/") == 0)
					elem = "/";
				else
					for(elem=p=c->path->s; *p; p++)
						if(*p == '/')
							elem = p+1;
				devdir(c, c->qid, elem, 0, eve, DMDIR|0555, &dir);
				
				n = convD2M(&dir, db, n);   
				if(n == 0)
				        ERROR_RETURN(Ebadarg, ONERR); //% 
				
				return n;
			}

			ERROR_RETURN(Enonexist, ONERR);  //% 

		case 0: // DBGPRN("@ 0 \n");
			break;

		case 1: // DBGPRN("@ 1 \n");
			if(c->qid.path == dir.qid.path) {
				if(c->flag&CMSG)
					dir.mode |= DMMOUNT;

				n = convD2M(&dir, db, n);
				if(n == 0)
				        ERROR_RETURN(Ebadarg, ONERR);  //%
				return n;
			}
			break;
		}
	}
}


long devdirread(Chan *c, char *d, long n, Dirtab *tab, int ntab, Devgen *gen) //% ONERR
{
	long m, dsz;
	Dir dir;
        DBGPRN(">%s(%s %x %x %x %x %x) \n", 
	       __FUNCTION__, c->path->s, d, n, tab, ntab, gen);

	for(m=0; m<n; c->dri++) {
		switch((*gen)(c, nil, tab, ntab, c->dri, &dir)){
		case -1:
			return m;

		case 0:
			break;

		case 1:
			dsz = convD2M(&dir, (uchar*)d, n-m);
			if(dsz <= BIT16SZ){	/* <= not < because this isn't stat; read is stuck */
				if(m == 0)
				        ERROR_RETURN(Eshort, ONERR);  //% 
				return m;
			}
			m += dsz;
			d += dsz;
			break;
		}
	}

	return m;
}


/*
 * error(Eperm) if open permission not granted for up->user.
 */
int devpermcheck(char *fileuid, ulong perm, int omode)  //% ONERR <- void
{
	ulong t;
	static int access[] = { 0400, 0200, 0600, 0100 };
        DBGPRN(">%s(%s %x %x)  \n", __FUNCTION__, up->user, perm, omode);
	if(strcmp(up->user, fileuid) == 0)
		perm <<= 0;
	else
	if(strcmp(up->user, eve) == 0)
		perm <<= 3;
	else
		perm <<= 6;

	t = access[omode&3];
	DBGPRN("  t=%x  t&perm=%x \n", t, t&perm);
	if((t&perm) != t) {
	        //% ERROR_RETURN(Eperm, ONERR);  //% 
	        l4printf("devpermcheck: permission denied\n"); //%%%%%
	}
	return  1;
}

Chan* devopen(Chan *c, int omode, Dirtab *tab, int ntab, Devgen *gen)  //% ONERRNIL
{
	int i;
	Dir dir;
        DBGPRN(">%s(%s %x %x %x %x) \n", 
	       __FUNCTION__, c->path->s, omode, tab, ntab, gen);

	for(i=0;; i++) {
		switch((*gen)(c, nil, tab, ntab, i, &dir)){
		case -1:
			goto Return;
		case 0:
			break;
		case 1:
			if(c->qid.path == dir.qid.path) {
			        int   rc; //%
				rc = devpermcheck(dir.uid, dir.mode, omode);
				IF_ERR_RETURN(rc, ONERR, nil);

				goto Return;
			}
			break;
		}
	}
Return:
	c->offset = 0;

	if((c->qid.type&QTDIR) && omode!=OREAD)
	        ERROR_RETURN(Eperm, nil);  //%
	c->mode = openmode(omode);
	c->flag |= COPEN;
	return c;
}

Chan * devcreate(Chan* _w, char* _x, int _y, ulong _z)  //% ONERR <- void
{
        ERROR_RETURN(Eperm, nil); //% 
}

Block* devbread(Chan *c, long n, ulong offset)  //% ONERRNIL
{
	Block *bp;
	int    len;  //%
        DBGPRN(">%s(%s %d %d) \n", __FUNCTION__, c->path->s, n, offset);

	bp = allocb(n);
	if(bp == 0)
	        ERROR_RETURN(Enomem, nil);  //%
	if(WASERROR()) {
	_ERR_1:   //%
		freeb(bp);
		NEXTERROR_RETURN(nil);
	}
	
	
	len = devtab[c->type]->read(c, bp->wp, n, offset);
	IF_ERR_GOTO(len, ONERR, _ERR_1);

	bp->wp += len;

	//% bp->wp += devtab[c->type]->read(c, bp->wp, n, offset);

	POPERROR();
	return bp;
}

long devbwrite(Chan *c, Block *bp, ulong offset) 
{
	long n;
        DBGPRN(">%s(%s %c %d)  \n", __FUNCTION__, c->path->s, bp, offset);

	if(WASERROR()) {
	_ERR_1:  //%
		freeb(bp);
		NEXTERROR_RETURN(ONERR); //% 
	}

	n = devtab[c->type]->write(c, bp->rp, BLEN(bp), offset);
	IF_ERR_GOTO(n, ONERR, _ERR_1);  //%

	POPERROR();
	freeb(bp);

	return n;
}

int devremove(Chan* _x)  //% ONERR <- void
{
        ERROR_RETURN(Eperm, ONERR); //%
}

int
devwstat(Chan* _x, uchar* _y, int _z) //% ONERR
{
        ERROR_RETURN(Eperm, ONERR); //%
	return 0;
}

int devpower(int _x)  //% ONERR <- void
{
        ERROR_RETURN(Eperm, ONERR);  //%
}

int devconfig(int _x, char * _y, DevConf * _z) //% ONERR
{
        ERROR_RETURN(Eperm, ONERR);  //%
	return 0;
}
