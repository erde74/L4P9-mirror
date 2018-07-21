//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>

char	*dest = "system";
int	mountflag = MREPL;

void	error(char *);
void	rpc(int, int);
void	post(char*, int);
void	mountfs(char*, int);

//%-------------------------------------------
#define  _exits  exits
//%-------------------------------------------

void
usage(void)
{
	fprint(2, "usage: %s [-abcC] tcp!host!port [srvname [mtpt]]\n", argv0);
	exits("usage");
}


void
main(int argc, char *argv[])
{
	int fd;
	char *srv, *mtpt;
	char dir[1024];
	char err[ERRMAX];
	char *p, *p2;
	int domount, reallymount, try, sleeptime;

	domount = 0;
	reallymount = 0;
	sleeptime = 0;

	ARGBEGIN{
	case 'a':
		mountflag |= MAFTER;
		domount = 1;
		reallymount = 1;
		break;
	case 'b':
		mountflag |= MBEFORE;
		domount = 1;
		reallymount = 1;
		break;
	case 'c':
		mountflag |= MCREATE;
		domount = 1;
		reallymount = 1;
		break;
	case 'C':
		mountflag |= MCACHE;
		domount = 1;
		reallymount = 1;
		break;
	case 'm':
		domount = 1;
		reallymount = 1;
		break;
	case 'q':
		domount = 1;
		reallymount = 0;
		break;
	case 's':
		sleeptime = atoi(EARGF(usage()));
		break;
	default:
		usage();
		break;
	}ARGEND

	if((mountflag & MAFTER) && (mountflag & MBEFORE))
		usage();

	switch(argc){
	case 1:	/* calculate srv and mtpt from address */
		p = strrchr(argv[0], '/');
		p = p ? p+1 : argv[0];
		srv = smprint("/srv/%s", p);
		p2 = strchr(p, '!');
		p2 = p2 ? p2+1 : p;
		mtpt = smprint("/n/%s", p2);
		break;
	case 2:	/* calculate mtpt from address, srv given */
		srv = smprint("/srv/%s", argv[1]);
		p = strrchr(argv[0], '/');
		p = p ? p+1 : argv[0];
		p2 = strchr(p, '!');
		p2 = p2 ? p2+1 : p;
		mtpt = smprint("/n/%s", p2);
		break;
	case 3:	/* srv and mtpt given */
		domount = 1;
		reallymount = 1;
		srv = smprint("/srv/%s", argv[1]);
		mtpt = smprint("%s", argv[2]);
		break;
	default:
		srv = mtpt = nil;
		usage();
	}

	try = 0;
	dest = *argv;

	print("srv{domount=%d dest='%s' srv='%s' mtpt='%s'} \n", 
	      domount, dest, srv, mtpt);

Again:
	try++;

	if(access(srv, 0) == 0){
		if(domount){
			fd = open(srv, ORDWR);
			if(fd >= 0)
				goto Mount;
			remove(srv);
		}
		else{
			fprint(2, "srv: %s already exists\n", srv);
			exits(0);
		}
	}

	//%  dest = netmkaddr(dest, 0, "564");
	//%  dest = netmkaddr(dest, 0, "9fs");
	fd = dial(dest, 0, dir, 0);

	if(fd < 0) {
		fprint(2, "SRV: dial %s: %r\n", dest);
		exits("dial");
	}

	fprint(2, "sleep...");
	sleep(5*1000);

	post(srv, fd);

Mount:
	if(domount == 0 || reallymount == 0)
		exits(0);

         if(mount(fd, -1, mtpt, mountflag, "") < 0){
		err[0] = 0;
		errstr(err, sizeof err);
		if(strstr(err, "Hangup") || strstr(err, "hungup") || strstr(err, "timed out")){
			remove(srv);
			if(try == 1)
				goto Again;
		}
		fprint(2, "srv %s: mount failed: %s\n", dest, err);
		exits("mount");
	}
	exits(0);
}

void
post(char *srv, int fd)
{
	int f;
	char buf[128];

	fprint(2, "post...\n");
	f = create(srv, OWRITE, 0666);
	if(f < 0){
		sprint(buf, "create(%s)", srv);
		error(buf);
	}
	sprint(buf, "%d", fd);
	if(write(f, buf, strlen(buf)) != strlen(buf))
		error("write");
}

void
error(char *s)
{
	fprint(2, "srv %s: %s: %r\n", dest, s);
	exits("srv: error");
}
