//%%%%%%%%% TOY DEV : an example Dev program for training. %%%%%%%%

#include        <u.h>
#include        "../port/lib.h"
#include        "../pc/mem.h"
#include        "../pc/dat.h"
#include        "../pc/fns.h"
#include        "../port/error.h"

#include  <l4/l4io.h>
#define   _DBGFLG  0 
#include  <l4/_dbgpr.h>

enum
{
	OPERM	= 0x3,		/* mask of all permission types in open mode */
	Nrnode	= 512,
	Maxsize	= 64*1024*1024,
	Maxfdata	= 8192,
};

typedef struct Rnode Rnode;

struct Rnode
{
	short	active;
	short	open;
        Rnode    *parent;
        Rnode    *child; 
        Rnode    *next;   //%   sibling-link, or free-list
	Qid	qid;
	long	perm;
	char	*name;
	ulong	atime;
	ulong	mtime;
	char	*user;
	char	*group;
	char	*muid;
	char	*data;
	long	ndata;
};

enum
{
	Pexec =	1,
	Pwrite= 2,
	Pread = 4,
};

static Rnode  rnodetbl[Nrnode];
static Rnode  *freep = nil; 

static Rnode  *get_rnode( )
{
    Rnode  *r;
    r = freep;
    if ( r == nil) 
        return  nil;
    else {
        freep = r->next;
	r->next = nil;
	return  r;
    }
}

static void put_rnode(Rnode *r)
{
  r->next = freep;
  freep = r;
}

static ulong	pathcnt = 0;	/* incremented for each new file */
static uchar   statbuf[4096];

static uint	rnodestat(Rnode*, uchar*, uint);
static void	*erealloc(void*, ulong);
static void	*emalloc(ulong);
static char	*estrdup(char*);
static int	perm(Rnode*, int);
static Rnode*   create_file(Rnode*, char*, int, ulong, char*, int);
static Rnode*   create_dir(Rnode*, char*, int, ulong);

static char	Enoauth[] =	"ramfs: authentication not required";
static char	Enotexist[] =	"file does not exist";
static char	Enotowner[] =	"not owner";
static char	Eisopen[] = 	"file already open for I/O";
static char	Excl[] = 	"exclusive use file already open";
static char	Ename[] = 	"illegal name";
static char	Eversion[] =	"unknown 9P version";
static char	Enotempty[] =	"directory not empty";
// char	Eperm[] =	"permission denied";
// char	Enotdir[] =	"not a directory";
// char	Einuse[] =	"file in use";
// char	Eexist[] =	"file exists";
// char	Eisdir[] =	"file is a directory";


#if 0 //--------------------------
struct Dev {
  int     dc;
  char*   name;
  void    (*reset)(void);
  void    (*init)(void);
  :
  void    (*power)(int);  /* power mgt: power(1) => on, power (0) => off */
  int     (*config)(int, char*, DevConf*);        // returns nil on error
};
 
struct Walkqid {
  Chan    *clone;
  int     nqid;
  Qid     qid[1];
};
#endif //------------------------

static char homepage[] =
    "LP49 web site: http://research.nii.ac.jp/H2O/LP49/";
static char rootfs[] = 
    "This component is planned to become a root file system.";
static char file8k[4096*2];


static char rcmain[] =   
   #include  "root-rc-rcmain.txt"
  ;

static char lib_namespace[] =
   #include  "root-lib-namespace.txt"
  ;

/* This will become the root file system. */

static void setup_dirs(Rnode *r)
{
  Rnode *rdir,   *mnt, *work, *rc, *expfs, *homedir, *libdir; 
    rc   = create_dir(r, "rc", 0, 0777);
      create_file(rc, "rcmain", 0, 0777, rcmain, strlen(rcmain));    

    libdir   = create_dir(r, "lib", 0, 0777);
      create_file(libdir, "namespace", 0, 0777, 
		  lib_namespace, strlen(lib_namespace));    

    mnt = create_dir(r, "mnt", 0, 0777);
    {
      expfs = create_dir(mnt, "exportfs", 0, 0777);
      {
        rdir = create_dir(expfs, "0", 0, 0777);
        rdir = create_dir(expfs, "1", 0, 0777);
        rdir = create_dir(expfs, "2", 0, 0777);
      }
      //rdir = create_dir(mnt, "u9fs", 0, 0777);
      rdir = create_dir(mnt, "temp", 0, 0777);
      rdir = create_dir(mnt, "wsys", 0, 0777);
    }

    rdir = create_dir(r, "tmp", 0, 0777);
    work = create_dir(r, "work", 0, 0777);
    {
      int  j, k = ' ';
      char  ch;
      create_file(work, "HomePage", 0, 0666, homepage, strlen(homepage));
      create_file(work, "RootFS", 0, 0666, rootfs, strlen(rootfs));
      for(j=0; j<sizeof(file8k); ){
	  if ( k >= '~') k = ' ';
	  file8k[j++] = k++;
      }
      create_file(work, "file8k", 0, 0666, file8k, sizeof(file8k));
    }

    rdir = create_dir(r, "usr", 0, 0777);
    homedir = create_dir(r, "home", 0, 0777);
    {
      rdir = create_dir(homedir, "eve", 0, 0777);
      rdir = create_dir(homedir, "adm", 0, 0777);
    }
}

static void pr_rnode(Rnode *r)
{
    Rnode  *child;
    for( ; r; r = r->next) {
      l4printf("Rnode<%s %x> ", r->name, r->qid.path);
      child = r->child; 
      if (child) {
	l4printf("{");  pr_rnode(child); l4printf("}");
      }
    }
} 

static Chan*  rfsattach(char*  spec) 
{
DBGPRN("> %s(%s) \n", __FUNCTION__, spec);
	Chan  *c;
	Rnode   *root;
	char  buf[16] = {0};
	char  tc = 'R';   

	// Followings correspond:  "c = devattach('R', spec);"
	c = newchan( );
	mkqid(&c->qid, 0, 0, QTDIR);
	c->type = devno(tc, 0);     

	sprint(buf, "#%c%s", tc, (spec)?spec:"");
	c->path = newpath(buf);

	root = &rnodetbl[0];
	c->_u.aux = root;

	return  c;
}

static void rfsreset( )
{
DBGPRN("> %s() \n", __FUNCTION__);
	Rnode  *root;
	int  i;

	// Setup rnode free list
	memset(rnodetbl, 0, sizeof (rnodetbl));
	freep = &rnodetbl[0];
	for (i = 0; i < Nrnode -1; i++)
	    rnodetbl[i].next = &rnodetbl[i+1];
	rnodetbl[Nrnode-1].next = nil;

	// Setup  root ,... 	
	root = get_rnode();
	root->active = 1;
	root->name = "ramfsroot";
	root->user = root->group = root->muid = eve;
	root->perm = DMDIR | 0777;
	mkqid(&root->qid, 0, 0, QTDIR);

	setup_dirs(root);
	// pr_rnode(root); //DBG
}

static void rfsinit( )
{
}

static void rfsshutdown( )
{
}

static Walkqid*  rfswalk(Chan* c, Chan* nc, char* names[], int nname)
{
        Rnode *rnodep, *r;
	char  *err = nil;
	ulong t;
	int   i;
	int   alloc = 0;

if (_DBGFLG) {
    DPB("> %s(c:%d(%s) nc:%d(%s) names{", __FUNCTION__, c->fid, c->path->s, 
	(nc==0)?0:(nc->fid),  (nc==0)?"":nc->path->s);
    for(i=0; i<nname; i++)DPB("%s:", names[i]);  //%
    DPB("})\n");
}

	Walkqid *wq = malloc(sizeof(Walkqid) + (nname-1)*sizeof(Qid));

	rnodep = c->_u.aux;
	wq->nqid = 0;
	if(nc == nil){
		nc = devclone(c);
		nc->type = 0;
		alloc = 1;
	}
	wq->clone = nc;  

	t = time(0); 
	for(i=0; i<nname; i++) {  
	     if((rnodep->qid.type & QTDIR) == 0){ DD(3);
	         err = Enotdir;
		 break;
	     }
	     if(rnodep->active == 0){ DD(4);
	         err = Enotexist;
		 break;
	     }
	     rnodep->atime = t;

	     if(strcmp(names[i], ".") == 0){
		 wq->qid[wq->nqid++] = nc->qid;
		 continue;
	     }

	     if(strcmp(names[i], "..") == 0){
	         rnodep = rnodep->parent;
		 if(!perm(rnodep, Pexec)){
                   err = Eperm;
		   break;
	         }
		 wq->qid[wq->nqid++] = rnodep->qid;
		 nc->qid = rnodep->qid;
		 continue;
	     }

	     for(r=rnodep->child; r; r = r->next) {
		 if(strcmp(names[i], r->name)==0) {
		     rnodep = r;
		     wq->qid[wq->nqid++] = rnodep->qid;
		     nc->qid = rnodep->qid;
		     nc->_u.aux = r;  // Here or in open() ?
		     break;
		 }
	     }
	     if (r == nil)  { DD(5);
	       //err = "notfound";
	       break;
	     }
	}

#if 1 //???? 100101
	if ((wq->nqid < nname) || err) { 
DBGPRN("rfswalk<nqid:%d nname:%d> \n" ,wq->nqid,  nname);

	    if (alloc)  cclose(wq->clone);
	    wq->clone = nil;
	    //	    err = "pathname";
	    //	    free(wq);
	    //	    wq = nil;
	}
	else
#endif 

	if (wq->clone)  {
	    wq->clone->type = c->type;
	}
	    
if (_DBGFLG) {
    DPB("< rfswalk(c:%d(%s) nc:%d(%s))=> Walkqid{cloneC:%d qid{", 
	c->fid, c->path->s, (nc==0)?0:(nc->fid), (nc==0)?"":nc->path->s, 
	(wq)?wq->clone->fid:-1);
    for(i=0; i< wq->nqid; i++) DPB("%d:", (ulong)wq->qid[i].path);
    DPB("}}\n");
}

	return  wq;
}

static Chan*  rfsopen(Chan  *c, int  omode)
{
	Rnode *r;
	int  mode, trunc;
	char *err;
DBGPRN("> %s(c:%d(%s) %x) \n", __FUNCTION__, c->fid, c->path->s, omode);

	r = c->_u.aux;
	//if(r->open)  { err = Eisopen; goto Err; }

	if(r->active == 0) { 
	    err = Enotexist;
	    goto Err; 
	}

	if ((r->perm & DMEXCL) && (r->open)){ err = Excl; goto Err; }

	mode = omode;
	if(r->qid.type & QTDIR){
	  //l4printf("%x:%x  mode=%d:%d \n", r->qid.type, QTDIR, mode, OREAD);

	  if(mode != OREAD) { err = Eperm; goto Err; }
	  else  {
DBGPRN("< %s( ) => c:%d(%s)\n", __FUNCTION__, c->fid, c->path->s);
	    return c;   
	  }
	}
	if(mode & ORCLOSE){
		/* can't remove root; must be able to write parent */
	  //	if(r->qid.path==0 || !perm(f, &rnode[r->parent], Pwrite))
	  { err = Eperm;  goto Err; }
	}
	trunc = mode & OTRUNC;
	mode &= OPERM;
	if(mode==OWRITE || mode==ORDWR || trunc)
	        if(!perm(r, Pwrite)) { err = Eperm;  goto Err; }

	if(mode==OREAD || mode==ORDWR)
		if(!perm(r, Pread))  { err = Eperm;  goto Err; }

#if 0 //% To allow program on DOS file to run
	if(mode==OEXEC)
		if(!perm(r, Pexec))   { err = Eperm;  goto Err; }
#endif //%
	if(trunc && (r->perm&DMAPPEND)==0){
		r->ndata = 0;
		if(r->data)
			free(r->data);
		r->data = 0;
		r->qid.vers++;
	}
	r->open++;
	c->mode = mode;  
DBGPRN("< %s( ) => c:%d(%s)\n", __FUNCTION__, c->fid, c->path->s);
	return c;

 Err: 
	l4printf("Err open: %s \n", err);
	return  nil;
}

static Rnode * create_rnode(Rnode *dir, char *name, int omode, ulong  prm)
{
        Rnode    *r;
	Rnode  **here;
	char    *emsg = nil;
        //DBGPRN("> %s(%x %s %x %x) \n", __FUNCTION__, dir, name, omode, prm);

	if((dir->qid.type & QTDIR) == 0)
	    {  emsg = Enotdir;   goto Fin; }

	here = &dir->child;
	for(r = dir->child; r; r = r->next) { 
	    if(strcmp(name, r->name) == 0)
		  {  emsg = Einuse;  goto Fin; }
	    if(r->next == nil) { 
	        here = &r->next; 
		break; 
	    }
	}

	r = get_rnode();
	*here = r;

	r->active = 1;
	r->next = nil;
	r->qid.path = ++pathcnt;
	r->qid.vers = 0;
	if (prm & DMDIR)
	    r->qid.type |= QTDIR;
	r->parent = dir;
	free(r->name);
	r->name = estrdup(name);
	r->user = r->group = r->muid = "eve";

	if(prm & DMDIR)
	    prm = (prm & ~0777) | (dir->perm & prm & 0777);
	else
	    prm = (prm & (~0777|0111)) | (dir->perm & prm & 0666);

	r->perm = prm;  
	r->data = nil; 
	r->ndata = 0;

	r->mtime = r->atime = time(0);
	dir->mtime = r->atime;  

 Fin:
	if (emsg != nil) 
	  l4printf("< %s(%s):%s \n", __FUNCTION__, name, emsg);

//DBGPRN("< %s(%s):%s %x\n", __FUNCTION__, name, emsg, r->qid.type);
	return  r;
}

static Rnode *  create_dir(Rnode *dir, char *name, int omode, ulong  prm)
{
        Rnode    *r;
	prm = prm | DMDIR;
	r = create_rnode(dir, name, omode, prm);
	return r;
}

static Rnode *  create_file(Rnode *dir, char *name, int omode, ulong  prm, 
			    char *data, int len)
{
        Rnode    *r;
	r = create_rnode(dir, name, omode, prm);
	if (r == nil)   return  nil;

	r->data = malloc(len + 16);
	strncpy(r->data, data, len);
	r->ndata = len;
	return r;
}


static Chan*  rfscreate(Chan*  c, char *name, int omode, ulong  prm)  //% ONERR
{
        Rnode  *r, *dir;
	Rnode  **here;
	char *emsg = nil;
DBGPRN("> %s(c:%d %s %x %x) \n", __FUNCTION__, c->fid, name, omode, prm);

	dir = c->_u.aux;

	//if(dir->open)  //???
	//  { emsg = Eisopen; goto Fin;}

	if(dir->active == 0)
	  { emsg = Enotexist;  goto Fin; }
	
	if((dir->qid.type & QTDIR) == 0)
	  {  emsg = Enotdir;   goto Fin; }

	/* parent must be writable */
	if(!perm(dir, Pwrite))
	  {  emsg = Eperm;  goto Fin; }

	if(strcmp(name, ".")==0 || strcmp(name, "..")==0)
	  {  emsg = Ename;  goto Fin; }

	here = &dir->child;
	for(r = dir->child; r; r = r->next) { 
	    if(strcmp(name, r->name) == 0)
		  {  emsg = Einuse;  goto Fin; }
	    if(r->next == nil) { 
	        here = &r->next; 
		break; 
	    }
	}

	r = get_rnode();
	if (r == nil) 
	  {  emsg = "Out of memory";  goto Fin; }	

	*here = r;

	r->active = 1;
	r->next = nil;
	r->qid.path = ++pathcnt;
	r->qid.vers = 0;
	r->qid.type = 0; //% ?
	if (prm & DMDIR)
	    r->qid.type |= QTDIR;
	r->parent = dir;
	free(r->name);
	r->name = estrdup(name);
	r->user = r->group = r->muid = "eve";

	if(prm & DMDIR)
	    prm = (prm & ~0777) | (dir->perm & prm & 0777);
	else
	    prm = (prm & (~0777|0111)) | (dir->perm & prm & 0666);
	r->perm = prm;
	r->ndata = 0;

	r->mtime = r->atime = time(0);
	dir->mtime = r->atime;
	// f->rnode = r;
	// rhdr.qid = r->qid;

	c->mode = omode & OPERM;  //% ??
	r->open++;

#if 1 //% ??
	c->qid.path = r->qid.path;
	c->qid.vers = r->qid.vers;
	c->qid.type = r->qid.type;
	// c->mode = openmode(omode);
	c->_u.aux = r;
#endif //%

 Fin:
	if (emsg) 
	  l4printf("< %s(%s):%s \n", __FUNCTION__, name, emsg);

	return  c;  //%
}


static Block*  rfsbread(Chan* c, long _x, ulong _y) { return nil; }

static long  rfsread(Chan *c,  void *buf,  long len, vlong offset)
{
        Rnode *rp,  *r;
        DBGPRN("> %s(c:%d(%s) 0x%x %d %d) \n", __FUNCTION__, 
	       c->fid, c->path->s,  buf, len, offset);

	long off;
	int n, m, cnt, count;
	static char dmybuf[512];

	rp = c->_u.aux;

	if(rp->active == 0){
	    return -1; //Enotexist;
	}
	m = n = 0;
	count = 0;
	off = offset;
	cnt = len;

	// Offset must be supported.

	if(rp->qid.type & QTDIR) { 
	    for (r = rp->child; (off>0 && r); r = r->next) {
		    if (! r->active)  
		        continue;
		    off -= rnodestat(r, dmybuf, sizeof(dmybuf));
		    if (off <= 0) {
		        DBGPRN("< rfsread( )C=> %d\n", off);
		        return  0;
		    }
	    }

	    for ( ; r; r = r->next) {
	        // l4printgetc("@ %x ", r);
	        if (! r->active)  
		    continue;

		m = rnodestat(r, buf + n, cnt - n);
		if ( m == 0) { 
		    DBGPRN("< rfsread( )=> 0\n");
		    return 0;
		}
		n += m;
		off += m;
	    }
	    DBGPRN("< rfsread( )A=> %d\n", n);
	    return n;
	}

	if(off >= rp->ndata)
		return 0;
	rp->atime = time(0);
	n = cnt;
	if(off+n > rp->ndata)
		n = rp->ndata - off;

	memcpy(buf, rp->data + off, n);

	DBGPRN("< rfsread( )B=> %d\n", n);
	return n;
}


static long    rfsbwrite(Chan* c, Block* bp, ulong _x){ return 0; }

static long   rfswrite(Chan *c, void *buf, long len, vlong offset)
{
	Rnode  *rp;
	ulong off;
	int   cnt;
	char *err;
        DBGPRN("> %s(c:%d(%s) 0x%x %d %d) \n", __FUNCTION__, 
	       c->fid, c->path->s, buf, len, offset);

	rp = c->_u.aux;
	// DPR("## %s ", rp->name);

	if(rp->active == 0) { err = Enotexist; goto Err; }

	off = offset;
	if(rp->perm & DMAPPEND)
		off = rp->ndata;
	cnt = len;
	if(rp->qid.type & QTDIR) { err = Eisdir; goto Err; }

	if(off+cnt >= Maxsize)		/* sanity check */
	  { err = "write too big"; goto Err; }

	if(off+cnt > rp->ndata)
		rp->data = erealloc(rp->data, off+cnt);
	if(off > rp->ndata)
		memset(rp->data + rp->ndata, 0, off - rp->ndata);
	if(off+cnt > rp->ndata)
		rp->ndata = off+cnt;
	memmove(rp->data+off, buf, cnt);
	rp->qid.vers++;
	rp->mtime = time(0);

	return cnt;

 Err:
	l4printf("Err rfswrite: %s \n", err);
	return 0;
}

static int emptydir(Rnode *dr)
{
	Rnode *r;
	for(r=dr->child; r; r = r->next)
		if(r->active)
			return 0;
	return 1;
}

static char *realremove(Rnode *r)
{
	if(r->qid.type & QTDIR && !emptydir(r))
		return Enotempty;
	r->ndata = 0;
	if(r->data)
		free(r->data);
	r->data = 0;
	r->parent = 0;
	memset(&r->qid, 0, sizeof r->qid);
	free(r->name);
	r->name = nil;
	r->active = 0;
	return nil;
}

#if 0
char *rclunk(Fid *f)
{
	char *e = nil;

	if(f->open)
		f->rp->open--;
	if(f->rclose)
		e = realremove(f->ram);
	f->active = 0;
	f->open = 0;
	f->ram = 0;
	return e;
}
#endif

static int  rfsremove(Chan* c)  //% ONERR
{
	Rnode *rp;
	rp = c->_u.aux;
	rp->active = 0;
	rp->open = 0;

	if(rp->qid.path == 0 || !perm(rp->parent, Pwrite))
	    return  -1; //Eperm;  // -1

	rp->parent->mtime = time(0);
	c->_u.aux = nil;

	realremove(rp);
	return  0;
}

static int rfsstat(Chan *c, uchar* dp, int n)
{
        DBGPRN("> %s(%x %x %x) \n", __FUNCTION__, c, dp, n);
	int  len;
        Rnode  *rp = c->_u.aux;
	if(rp->active == 0){
	    return 0;   //Enotexist;
	}
	len = rnodestat(rp, statbuf, sizeof statbuf);
	memmove(dp, statbuf, len);
	return  len;
}


static int  rfswstat(Chan *c, uchar* x, int y)
{
        l4printf("%s(): Not yet implemented\n", __FUNCTION__); 
#if 0
	Rnode *r, *s;
	Dir dir;

	if(f->ram->active == 0){
		return Enotexist;
	}
	convM2D(thdr.stat, thdr.nstat, &dir, (char*)statbuf);
	r = f->ram;

	// To change length, must have write permission on file.
	if(dir.length!=~0 && dir.length!=r->ndata){
	 	if(!perm(f, r, Pwrite))
			return Eperm;
	}

	// To change name, must have write permission in parent
	// and name must be unique.
	if(dir.name[0]!='\0' && strcmp(dir.name, r->name)!=0){
	 	if(!perm(f, &ram[r->parent], Pwrite))
			return Eperm;
		for(s=ram; s<&ram[nram]; s++)
			if(s->active && s->parent==r->parent)
			if(strcmp(dir.name, s->name)==0)
				return Eexist;
	}

	// To change mode, must be owner or group leader.
	// Because of lack of users file, leader=>group itself.
	if(dir.mode!=~0 && r->perm!=dir.mode){
		if(strcmp(f->user, r->user) != 0)
		if(strcmp(f->user, r->group) != 0)
			return Enotowner;
	}

	// To change group, must be owner and member of new group,
	// or leader of current group and leader of new group.
	// Second case cannot happen, but we check anyway.
	if(dir.gid[0]!='\0' && strcmp(r->group, dir.gid)!=0){
		if(strcmp(f->user, r->user) == 0)
	//	if(strcmp(f->user, dir.gid) == 0)
			goto ok;
		if(strcmp(f->user, r->group) == 0)
		if(strcmp(f->user, dir.gid) == 0)
			goto ok;
		return Enotowner;
		ok:;
	}

	/* all ok; do it */
	if(dir.mode != ~0){
		dir.mode &= ~DMDIR;	/* cannot change dir bit */
		dir.mode |= r->perm&DMDIR;
		r->perm = dir.mode;
	}
	if(dir.name[0] != '\0'){
		free(r->name);
		r->name = estrdup(dir.name);
	}
	if(dir.gid[0] != '\0')
		r->group = estrdup(dir.gid);
	if(dir.length!=~0 && dir.length!=r->ndata){
		r->data = erealloc(r->data, dir.length);
		if(r->ndata < dir.length)
			memset(r->data+r->ndata, 0, dir.length-r->ndata);
		r->ndata = dir.length;
	}
	ram[r->parent].mtime = time(0);
#endif
	return 0;
}

static int  rfsclose(Chan *c)  //ONERR
{
        Rnode  *r;
	r = c->_u.aux;
	r->open--;
	return  0; //%
}

static uint rnodestat(Rnode *r, uchar *buf, uint nbuf)
{
        int  n;
	Dir  dir;
	dir.name = r->name;
	dir.type = 'R'; 
	dir.dev = 0;  //?
	dir.qid = r->qid;
	dir.mode = r->perm;
	dir.length = r->ndata;
	dir.uid = r->user;
	dir.gid = r->group;
	dir.muid = r->muid;
	dir.atime = r->atime;
	dir.mtime = r->mtime;
	n = convD2M(&dir, buf, nbuf);

	if (n>2)  return n;
	else      return 0;
}


static int perm(Rnode *r, int p)
{
	if((p) & r->perm)	return 1;
	if(((p<<3) & r->perm))	return 1;
	if(((p<<6) & r->perm))	return 1;
	return 0;
}


static void  * emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(!p)	DBGPRN("out of memory\n");
	memset(p, 0, n);
	return p;
}

static void *erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(!p)	DBGPRN("out of memory\n");
	return p;
}

static char *estrdup(char *q)
{
	char *p;
	int n;

	n = strlen(q)+1;
	p = malloc(n);
	if(!p)	DBGPRN("out of memory\n");
	memmove(p, q, n);
	return p;
}


Dev ramfsdevtab = {
  'R',
  "ramfs",
 
  rfsreset,
  rfsinit,
  rfsshutdown,
  rfsattach,
  rfswalk,
  rfsstat,
  rfsopen,
  rfscreate,
  rfsclose,
  rfsread,
  rfsbread,
  rfswrite,
  rfsbwrite,
  rfsremove,
  rfswstat,
};
