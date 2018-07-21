//%%%%%%%%%
#include <u.h>
#include <libc.h>

//%---------------------
//  #include  <l4all.h>
//  #include  <l4/l4io.h>
//  #define    printf  l4printf 

static void USED(void * x, ...) {}
static void SET(void * x, ...) {}

#define STATIC static   // static added.

//%--------------------

STATIC char *e;
STATIC ulong mode = 0777L;

STATIC void
usage(void)
{
	fprint(2, "usage: mkdir [-p] [-m mode] dir...\n");
	exits("usage");
}

STATIC int
makedir(char *s)
{
	int f;

	if(access(s, AEXIST) == 0){
		fprint(2, "mkdir: %s already exists\n", s);
		e = "error";
		return -1;
	}
	f = create(s, OREAD, DMDIR | mode);
	if(f < 0){
		fprint(2, "mkdir: can't create %s: %r\n", s);
		e = "error";
		return -1;
	}
	close(f);
	return 0;
}

STATIC void
mkdirp(char *s)
{
	char *p;

	for(p=strchr(s+1, '/'); p; p=strchr(p+1, '/')){
		*p = 0;
		if(access(s, AEXIST) != 0 && makedir(s) < 0)
			return;
		*p = '/';
	}
	if(access(s, AEXIST) != 0)
		makedir(s);
}


#ifdef QSH //%------------------------------------

int qsh_mkdir(char * path)
{
    return makedir(path);
}


#else //-----------------------------------------
void
main(int argc, char *argv[])
{
	int i, pflag;
	char *m;

	pflag = 0;
	ARGBEGIN{
	default:
		usage();
	case 'm':
		m = ARGF();
		if(m == nil)
			usage();
		mode = strtoul(m, &m, 8);
		if(mode > 0777)
			usage();
		break;
	case 'p':
		pflag = 1;
		break;
	}ARGEND

	for(i=0; i<argc; i++){
		if(pflag)
			mkdirp(argv[i]);
		else
			makedir(argv[i]);
	}
	exits(e);
}
#endif //--------------------------------
