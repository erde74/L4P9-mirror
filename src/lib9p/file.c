//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

/*
 * To avoid deadlock, the following rules must be followed.
 * Always lock child then parent, never parent then child.
 * If holding the free file lock, do not lock any Files.
 */
struct Filelist
{
	File *f;
	Filelist *link;
};

struct Readdir
{
	File *dir;
	Filelist *fl;
};

static QLock filelk;
static File *freefilelist;

static File*
allocfile(void)
{
	int i, a;
	File *f;
	enum { N = 16 };

	qlock(&filelk);
	if(freefilelist == nil){
		f = emalloc9p(N*sizeof(*f));
		for(i=0; i<N-1; i++)
			f[i].aux = &f[i+1];
		f[N-1].aux = nil;
		f[0].allocd = 1;
		freefilelist = f;
	}

	f = freefilelist;
	freefilelist = f->aux;
	qunlock(&filelk);

	a = f->allocd;
	memset(f, 0, sizeof *f);
	f->allocd = a;
	return f;
}

static void
freefile(File *f)
{
	Filelist *fl, *flnext;

	for(fl=f->filelist; fl; fl=flnext){
		flnext = fl->link;
		assert(fl->f == nil);
		free(fl);
	}

	free(f->_dir.name);  //% _dir
	free(f->_dir.uid);   //% _dir
	free(f->_dir.gid);   //% _dir
	free(f->_dir.muid);  //% _dir
	qlock(&filelk);
	assert(f->_ref.ref == 0);  //% _ref
	f->aux = freefilelist;
	freefilelist = f;
	qunlock(&filelk);
}

static void
cleanfilelist(File *f)
{
	Filelist **l;
	Filelist *fl;
	
	/*
	 * can't delete filelist structures while there
	 * are open readers of this directory, because
	 * they might have references to the structures.
	 * instead, just leave the empty refs in the list
	 * until there is no activity and then clean up.
	 */
	if(f->readers.ref != 0)  //% ???
		return;
	if(f->nxchild == 0)
		return;

	/*
	 * no dir readers, file is locked, and
	 * there are empty entries in the file list.
	 * clean them out.
	 */
	for(l=&f->filelist; (fl=*l); ){    //%
		if(fl->f == nil){
			*l = (*l)->link;
			free(fl);
		}else
			l = &(*l)->link;
	}
	f->nxchild = 0;
}

void
closefile(File *f)
{
        if(decref(&f->_ref) == 0){   //% <- f
		f->tree->destroy(f);
		freefile(f);
	}
}

static void
nop(File* _x)
{
}

int
removefile(File *f)
{
	File *fp;
	Filelist *fl;
	
	fp = f->parent;
	if(fp == nil){
		werrstr("no parent");
		closefile(f);
		return -1;
	}

	if(fp == f){
		werrstr("cannot remove root");
		closefile(f);
		return -1;
	}

	wlock(&f->_rwlock);  //%  f
	wlock(&fp->_rwlock);  //% fp
	if(f->nchild != 0){
		werrstr("has children");
		wunlock(&fp->_rwlock);  //% fp
		wunlock(&f->_rwlock);   //% f
		closefile(f);
		return -1;
	}

	if(f->parent != fp){
		werrstr("parent changed underfoot");
		wunlock(&fp->_rwlock);   //% fp
		wunlock(&f->_rwlock);  //% f
		closefile(f);
		return -1;
	}

	for(fl=fp->filelist; fl; fl=fl->link)
		if(fl->f == f)
			break;
	assert(fl != nil && fl->f == f);

	fl->f = nil;
	fp->nchild--;
	fp->nxchild++;
	f->parent = nil;
	wunlock(&f->_rwlock);  //% f

	cleanfilelist(fp);
	wunlock(&fp->_rwlock); //% fp

	closefile(fp);	/* reference from child */
	closefile(f);	/* reference from tree */
	closefile(f);
	return 0;
}

File*
createfile(File *fp, char *name, char *uid, ulong perm, void *aux)
{
	File *f;
	Filelist **l, *fl;
	Tree *t;

	if((fp->_dir.qid.type&QTDIR) == 0){    //% _dir
		werrstr("create in non-directory");
		return nil;
	}

	wlock(&fp->_rwlock);  //% fp
	/*
	 * We might encounter blank spots along the
	 * way due to deleted files that have not yet
	 * been flushed from the file list.  Don't reuse
	 * those - some apps (e.g., omero) depend on
	 * the file order reflecting creation order. 
	 * Always create at the end of the list.
	 */
	for(l=&fp->filelist; (fl=*l); l=&fl->link){
	  if(fl->f && strcmp(fl->f->_dir.name, name) == 0){  //% _dir
	                wunlock(&fp->_rwlock);  //% fp
			werrstr("file already exists");
			return nil;
		}
	}
	
	fl = emalloc9p(sizeof *fl);
	*l = fl;

	f = allocfile();
	f->_dir.name = estrdup9p(name);            //% ...
	f->_dir.uid = estrdup9p(uid ? uid : fp->_dir.uid);  //%
	f->_dir.gid = estrdup9p(fp->_dir.gid);  //%
	f->_dir.muid = estrdup9p(uid ? uid : "unknown");  //%
	f->aux = aux;
	f->_dir.mode = perm; //%

	t = fp->tree;
	lock(&t->genlock);
	f->_dir.qid.path = t->qidgen++;   //% _dir
	unlock(&t->genlock);
	if(perm & DMDIR)
	  f->_dir.qid.type |= QTDIR;  //%
	if(perm & DMAPPEND)
	  f->_dir.qid.type |= QTAPPEND;  //%
	if(perm & DMEXCL)
	  f->_dir.qid.type |= QTEXCL;  //%

	f->_dir.mode = perm;  //%
	f->_dir.atime = f->_dir.mtime = time(0);  //%
	f->_dir.length = 0;   //%
	f->parent = fp;
	incref(&fp->_ref);  //% fp
	f->tree = fp->tree;

	incref(&f->_ref);	/* being returned */  //%
	incref(&f->_ref);	/* for the tree */ //%
	fl->f = f;
	fp->nchild++;
	wunlock(&fp->_rwlock);  //% fp

	return f;
}

static File*
walkfile1(File *dir, char *elem)
{
	File *fp;
	Filelist *fl;

	rlock(&dir->_rwlock);  //% dir
	if(strcmp(elem, "..") == 0){
		fp = dir->parent;
		incref(&fp->_ref);  //% fp
		runlock(&dir->_rwlock);  //% dir
		closefile(dir);
		return fp;
	}

	fp = nil;
	for(fl=dir->filelist; fl; fl=fl->link)
	  if(fl->f && strcmp(fl->f->_dir.name, elem)==0){  //% _dir
			fp = fl->f;
			incref(&fp->_ref);  //%
			break;
		}

	runlock(&dir->_rwlock);  //%
	closefile(dir);
	return fp;
}

File*
walkfile(File *f, char *path)
{
	char *os, *s, *nexts;
	File *nf;

	if(strchr(path, '/') == nil)
		return walkfile1(f, path);	/* avoid malloc */

	os = s = estrdup9p(path);
	incref(&f->_ref);  //%
	for(; *s; s=nexts){
		if((nexts = strchr(s, '/')))
			*nexts++ = '\0';
		else
			nexts = s+strlen(s);
		nf = walkfile1(f, s);
		closefile(f);
		f = nf;
		if(f == nil)
			break;
	}
	free(os);
	return f;
}

/*          Tree
 *   t--> +----------+        +----------+
 *        | root  ---|------> |_dir.     |
 *        |(*destry) |        |_dir.name |  "/"
 *        |          |        | parent  -|--> SELF
 *        |          |        | filelist |
 *        +----------+ <------|-tree     |
 *                            |          |
 *                            +----------+
 */
		
Tree*
alloctree(char *uid, char *gid, ulong mode, void (*destroy)(File*))
{
	char *muid;
	Tree *t;
	File *f;

	t = emalloc9p(sizeof *t);
	f = allocfile();
	f->_dir.name = estrdup9p("/");   //% _dir ...
	if(uid == nil){
		uid = getuser();
		if(uid == nil)
			uid = "none";
	}
	uid = estrdup9p(uid);

	if(gid == nil)
		gid = estrdup9p(uid);
	else
		gid = estrdup9p(gid);

	muid = estrdup9p(uid);

	f->_dir.qid = (Qid){0, 0, QTDIR};  //% _dir ...
	f->_dir.length = 0;  //%
	f->_dir.atime = f->_dir.mtime = time(0);  //%
	f->_dir.mode = DMDIR | mode;  //%
	f->tree = t;
	f->parent = f;
	f->_dir.uid = uid;  //%
	f->_dir.gid = gid;  //%
	f->_dir.muid = muid; //%

	incref(&f->_ref);  //% f
	t->root = f;
	t->qidgen = 0;
	t->dirqidgen = 1;
	if(destroy == nil)
		destroy = nop;
	t->destroy = destroy;

	return t;
}

static void
_freefiles(File *f)
{
	Filelist *fl, *flnext;

	for(fl=f->filelist; fl; fl=flnext){
		flnext = fl->link;
		_freefiles(fl->f);
		free(fl);
	}

	f->tree->destroy(f);
	freefile(f);
}

void
freetree(Tree *t)
{
	_freefiles(t->root);
	free(t);
}

Readdir*
opendirfile(File *dir)
{
	Readdir *r;

	rlock(&dir->_rwlock);  //% dir
	if((dir->_dir.mode & DMDIR)==0){  //% 
	  runlock(&dir->_rwlock);   //%
		return nil;
	}
	r = emalloc9p(sizeof(*r));

	/*
	 * This reference won't go away while we're 
	 * using it because file list entries are not freed
	 * until the directory is removed and all refs to
	 * it (our fid is one!) have gone away.
	 */
	r->fl = dir->filelist;
	r->dir = dir;
	incref(&dir->readers);
	runlock(&dir->_rwlock);   //%
	return r;
}

long
readdirfile(Readdir *r, uchar *buf, long n)
{
	long x, m;
	Filelist *fl;

	for(fl=r->fl, m=0; fl && m+2<=n; fl=fl->link, m+=x){
		if(fl->f == nil)
			x = 0;
		else if((x=convD2M(&fl->f->_dir, buf+m, n-m)) <= BIT16SZ) //% fl->f
			break;
	}
	r->fl = fl;
	return m;
}

void
closedirfile(Readdir *r)
{
	if(decref(&r->dir->readers) == 0){
	        wlock(&r->dir->_rwlock);  //% r->dir
		cleanfilelist(r->dir);
		wunlock(&r->dir->_rwlock);  //% r->dir
	}
	free(r);
}
