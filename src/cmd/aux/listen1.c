#include <u.h>
#include <libc.h>
#include <auth.h>

#if 1 //%----------------------------
#define  DBGPRN  if(0)l4printf
#endif //%---------------------------

int verbose;
int trusted;

void
usage(void)
{
	fprint(2, "usage: listen1 [-tv] address cmd args...\n");
	exits("usage");
}

void
becomenone(void)
{
	int fd;

	fd = open("#c/user", OWRITE);
	if(fd < 0 || write(fd, "none", strlen("none")) < 0)
		sysfatal("can't become none: %r");
	close(fd);
	if(newns("none", nil) < 0)
		sysfatal("can't build namespace: %r");
}

char*
remoteaddr(char *dir)
{
	static char buf[128];
	char *p;
	int n, fd;

	snprint(buf, sizeof buf, "%s/remote", dir);
	fd = open(buf, OREAD);
	if(fd < 0)
		return "";
	n = read(fd, buf, sizeof(buf));
	close(fd);
	if(n > 0){
		buf[n] = 0;
		p = strchr(buf, '!');
		if(p)
			*p = 0;
		return buf;
	}
	return "";
}


char data[60], dir[40], ndir[40];  //%
char *progname;   //%

void
main(int argc, char **argv)
{
  //%	char data[60], dir[40], ndir[40];
	int ctl, nctl, fd;
	int  rc; //%

	ARGBEGIN{
	default:
		usage();
	case 't':
		trusted = 1;
		break;
	case 'v':
		verbose = 1;
		break;
	}ARGEND

	if(argc < 2)
		usage();
#if 1 //%
	verbose = 1;  //% while testing
	trusted = 1;  //% while testing 
	progname = argv[1];
#endif //%

	if(!verbose){
		close(1);
		fd = open("/dev/null", OWRITE);
		if(fd != 1){
			dup(fd, 1);
			close(fd);
		}
	}

	if(!trusted)
		becomenone();
{
  int  i;
  DBGPRN("listen1 starting ( ");
  for(i=0; i<argc; i++)DBGPRN("%s  ", argv[i]);
  DBGPRN(")\n");
}
	sleep(200); //% Festina Lente

	ctl = announce(argv[0], dir);
	if(ctl < 0)
		sysfatal("announce %s: %r", argv[0]);

DBGPRN(" listen1:<announce(%s, %s):%d\n", argv[0], dir, ctl); //%%

	for(;;){
		nctl = listen(dir, ndir);
		if(nctl < 0)
			sysfatal("listen %s: %r", argv[0]);

DBGPRN(" listen1:<child-listen(%s, %s):%d\n", dir, ndir, nctl); //%%

		switch(rfork(RFFDG|RFPROC|RFNOWAIT|RFENVG|RFNAMEG|RFNOTEG)){
		case -1:
			reject(nctl, ndir, "host overloaded");
			close(nctl);
			continue;

		case 0:		  
DBGPRN(" listen1:>child_accept(%d, '%s')\n", nctl, ndir); //%%

			fd = accept(nctl, ndir);
DBGPRN(" listen1:<child:accept(%d, '%s') fd:%d\n", nctl, ndir, fd); //%%
			if(fd < 0){
				fprint(2, "accept %s: can't open  %s/data: %r", argv[0], ndir);
				_exits(0);
			}
			print("listen1:incoming call for %s from %s in %s\n", 
			      argv[0], remoteaddr(ndir), ndir);

			fprint(nctl, "keepalive");
			close(ctl);
			close(nctl);
			//%?	putenv("net", ndir);

			sprint(data, "%s/data", ndir);
DBGPRN(" listen1:>bind(%s /dev/cons MREPL)\n", data); //% 
			rc = bind(data, "/dev/cons", MREPL);   //% original is MREPL
DBGPRN(" listen1:<bind(%s /dev/cons MREPL):%d\n", data, rc); //% 

			dup(fd, 0);
			dup(fd, 1);
			dup(fd, 2);
			close(fd);

DBGPRN(" listen1:>exec('%s' '%s')\n", /*argv[1]*/ progname, *(argv+1));  //%

			exec(/*argv[1]*/ progname, argv+1);
			fprint(2, "exec: %r");
			exits(nil);

		default:

DBGPRN(" listen1:parent <dir:%s ndir:%s>\n", dir, ndir); //%%

		        //%	close(nctl);
			sleep(10000); //%% ? Festina Lente
			break;
		}
	}
}
