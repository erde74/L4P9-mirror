//%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>

#define	DEFB  	(8*1024)

#if  1  //%-------------------------------------------
static void  SET(void *_x, ...){ }
static void  USED(void *_x, ...){ }

#define STATIC  static  //%  static added.
#endif  //%------------------------------------------

STATIC  int	failed;
STATIC  int	gflag;
STATIC  int	uflag;
STATIC  int	xflag;
STATIC  void	copy(char *from, char *to, int todir);
STATIC  int	copy1(int fdf, int fdt, char *from, char *to);

#ifdef QSH //%---------------------------- 

STATIC int is_pathchar(char ch)
{
    if ((ch >= 'a' && ch <= 'z')  ||
	(ch >= 'A' && ch <= 'Z')  ||
	(ch >= '0' && ch <= '9')  ||
	(ch >= '-' && ch <= '/'))
        return  1;
    else return 0;
}

extern void dbg_dir(Dir* dirp); //%%

int qsh_cp(char * argslist)
{
    int  i, jj=0, pchar=0, todir=0;
    char  ch;
    char *arg[64];
    char *target;
    Dir  *dirb;

    for(i=0; ch=argslist[i]; i++) {
        if (is_pathchar(ch)) {
 	    if (pchar == 0) {
	        arg[jj++] = &argslist[i];
		if (jj >= 63)
		    return -1;
		pchar = 1;
	    }
	}
	else {
	    argslist[i] = 0;
	    pchar = 0;
	}
    }

    target = arg[jj-1];

    dirb = dirstat(target);
    // dbg_dir(dirb); //%%

    if(dirb!=nil && (dirb->mode&DMDIR))
        todir=1;
    if(jj>2 && !todir){
        fprint(2, "cp: %s not a directory\n", target);
	return -1;
    }
    for(i=0; i<jj-1; i++)
        copy(arg[i], target, todir);
}

#else //-----------------------------------
void
main(int argc, char *argv[])
{
	Dir *dirb;
	int todir, i;

	ARGBEGIN {
	case 'g':
		gflag++;
		break;
	case 'u':
		uflag++;
		gflag++;
		break;
	case 'x':
		xflag++;
		break;
	default:
		goto usage;
	} ARGEND

	todir=0;
	if(argc < 2)
		goto usage;
	dirb = dirstat(argv[argc-1]);
	if(dirb!=nil && (dirb->mode&DMDIR))
		todir=1;
	if(argc>2 && !todir){
		fprint(2, "cp: %s not a directory\n", argv[argc-1]);
		exits("bad usage");
	}
	for(i=0; i<argc-1; i++)
		copy(argv[i], argv[argc-1], todir);
	if(failed)
		exits("errors");
	exits(0);

usage:
	fprint(2, "usage:\tcp [-gux] fromfile tofile\n");
	fprint(2, "\tcp [-x] fromfile ... todir\n");
	exits("usage");
}
#endif //------------------------------------------

STATIC int
samefile(Dir *a, char *an, char *bn)
{
	Dir *b;
	int ret;

	ret = 0;
	b=dirstat(bn);
	if(b != nil)
	if(b->qid.type==a->qid.type)
	if(b->qid.path==a->qid.path)
	if(b->qid.vers==a->qid.vers)
	if(b->dev==a->dev)
	if(b->type==a->type){
		fprint(2, "cp: %s and %s are the same file\n", an, bn);
		ret = 1;
	}
	free(b);
	return ret;
}

STATIC void
copy(char *from, char *to, int todir)
{
	Dir *dirb, dirt;
	char name[256];
	int fdf, fdt, mode;

	if(todir){
		char *s, *elem;
		elem=s=from;
		while(*s++)
			if(s[-1]=='/')
				elem=s;
		sprint(name, "%s/%s", to, elem);
		to=name;
	}

	if((dirb=dirstat(from))==nil){
		fprint(2,"cp: can't stat %s: %r\n", from);
		failed = 1;
		return;
	}
	// dbg_dir(dirb); //%

	mode = dirb->mode;
	if(mode&DMDIR){
		fprint(2, "cp: %s is a directory\n", from);
		free(dirb);
		failed = 1;
		return;
	}
	if(samefile(dirb, from, to)){
		free(dirb);
		failed = 1;
		return;
	}
	mode &= 0777;
	fdf=open(from, OREAD);
	if(fdf<0){
		fprint(2, "cp: can't open %s: %r\n", from);
		free(dirb);
		failed = 1;
		return;
	}
	fdt=create(to, OWRITE, mode);
	if(fdt<0){
		fprint(2, "cp: can't create %s: %r\n", to);
		close(fdf);
		free(dirb);
		failed = 1;
		return;
	}
	if(copy1(fdf, fdt, from, to)==0 && (xflag || gflag || uflag)){
		nulldir(&dirt);
		if(xflag){
			dirt.mtime = dirb->mtime;
			dirt.mode = dirb->mode;
		}
		if(uflag)
			dirt.uid = dirb->uid;
		if(gflag)
			dirt.gid = dirb->gid;
		if(dirfwstat(fdt, &dirt) < 0)
			fprint(2, "cp: warning: can't wstat %s: %r\n", to);
	}			
	free(dirb);
	close(fdf);
	close(fdt);
}

STATIC int
copy1(int fdf, int fdt, char *from, char *to)
{
        static char  buf[DEFB];   //%	char *buf;
 
	long n, n1, rcount;
	int rv;
	char err[ERRMAX];

	//%	buf = malloc(DEFB);
	/* clear any residual error */
	err[0] = '\0';
	errstr(err, ERRMAX);
	rv = 0;
	for(rcount=0;; rcount++) {
		n = read(fdf, buf, DEFB);
		if(n <= 0)
			break;
		n1 = write(fdt, buf, n);
		if(n1 != n) {
			fprint(2, "cp: error writing %s: %r\n", to);
			failed = 1;
			rv = -1;
			break;
		}
	}
	if(n < 0) {
		fprint(2, "cp: error reading %s: %r\n", from);
		failed = 1;
		rv = -1;
	}

	//%	free(buf);  //%%% Bug in free() ?

	return rv;
}
