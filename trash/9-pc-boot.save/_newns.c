//%%%%%%%%%%%%%%%%%%%% for Test use  %%%%%

#include <u.h>
#include <libc.h>
//% #include <bio.h>


// fprint(2,   <-->    printf(


enum
{
        ANAMELEN = 28,
	NARG	= 15,		/* max number of arguments */
	MAXARG	= 10*ANAMELEN,	/* max length of an argument */
};

static int	setenv(char*, char*);
static char	*expandarg(char*, char*);
static int	splitargs(char*, char*[], char*, int);
static int	nsfile(char*, char **);
static int	nsop(char*, int, char*[]);
static int	callexport(char*, char*);


int newnsdebug = 1;


char * nsdata[] =
{
    "## root ##",
    "mount -aC #s/boot /root ",    // $rootspec

    "bind -a /root /",          // $rootdir
    "bind -c /root/mnt /mnt",
                                                                                       
    "## kernel devices ##",
    "bind #c /dev",
    "bind #d /fd",
    "bind -c #e /env",
    "bind #p /proc",
    "bind -c #s /srv",
    //%    "bind -a #??    /dev",
    "bind -a #S /dev",
    
    "## mount points ##",
//%    "mount /srv/slashn /n",
                                                                                       
    "## authentication ##",
//%    "mount -a /srv/factotum /mnt",
                                                                                       
    "## standard bin ##",
    "# bind /$cputype/bin /bin",
    "bind -a /rc/bin /bin",
                                                                                       
    "## internal networks ##",
    "## mount -a /srv/ip /net  ##",
    "bind -a #l /net",
    "bind -a #I /net",

//%    "mount -a /srv/cs /net",
//%    "mount -a /srv/dns /net",
//%    "mount -a /srv/net /net",
    
    "bind -c /usr/$user/tmp /tmp",
    "cd /usr/$user",
                                                                                       
    "## . /lib/namespace.local ##",
    "## . /lib/namespace.$sysname ##",

    0
};



int _newns(char *user)
{
	char home[4*ANAMELEN];
	int afd;
	int cdroot;
	char *path;

	afd = -1;

	{
		rfork(RFENVG|RFCNAMEG);
		setenv("user", user);
		snprint(home, 2*ANAMELEN, "/usr/%s", user);
		setenv("home", home);
	}

	cdroot = nsfile("newns", nsdata);

#if 0 //%
	/* make sure we managed to cd into the new name space */
	if(!cdroot){
		path = malloc(1024);
		if(path == nil || getwd(path, 1024) == 0 || chdir(path) < 0)
			chdir("/");
		if(path != nil)
			free(path);
	}
#endif //%
	return 0;
}



static int nsfile(char *fn, char *nsdata[])
{
	int argc;
	char *cmd, *argv[NARG+1], argbuf[MAXARG*NARG];
	int cdroot;
	int  i = 0;

	cdroot = 0;

	while( (cmd = nsdata[i++]) ) {   
		while(*cmd==' ' || *cmd=='\t')
			cmd++;
		if(*cmd == '#')
			continue;

		argc = splitargs(cmd, argv, argbuf, NARG);
		if(argc)
			cdroot |= nsop(fn, argc, argv);
	}

	return cdroot;
}



static int famount(int fd, char *mntpt, int flags, char *aname)
{
	int afd;
	afd = -1;
	return mount(fd, afd, mntpt, flags, aname);
}


static int nsop(char *fn, int argc, char *argv[])
{
	char *argv0;
	ulong flags;
	int fd, i;
	int cdroot;

	cdroot = 0;
	flags = 0;
	argv0 = 0;

	if (newnsdebug){
		for (i = 0; i < argc; i++)
			fprint(2, "%s ", argv[i]);
		fprint(2, "\n");
	}

	ARGBEGIN{
	case 'a':
		flags |= MAFTER;
		break;
	case 'b':
		flags |= MBEFORE;
		break;
	case 'c':
		flags |= MCREATE;
		break;
	case 'C':
		flags |= MCACHE;
		break;
	}ARGEND

	if(!(flags & (MAFTER|MBEFORE)))
		flags |= MREPL;

	if(strcmp(argv0, ".") == 0 && argc == 1){
#if 0 //% NotYet
		b = Bopen(argv[0], OREAD);
		if(b == nil)
			return 0;
		cdroot |= nsfile(fn, b, rpc);
		Bterm(&b->_Biobufhdr); 
#endif //%

	}
	else if(strcmp(argv0, "clear") == 0 && argc == 0){
		rfork(RFCNAMEG);

	}
	else if(strcmp(argv0, "bind") == 0 && argc == 2){
//%		if(bind(argv[0], argv[1], flags) < 0 && newnsdebug)
			fprint(2, "%s: bind: %s %s: %r\n", fn, argv[0], argv[1]);

	}
	else if(strcmp(argv0, "unmount") == 0){
		if(argc == 1)
			unmount(nil, argv[0]);
		else if(argc == 2)
			unmount(argv[0], argv[1]);

	}
	else if(strcmp(argv0, "mount") == 0){

		fd = open(argv[0], ORDWR);
		if(argc == 2){
			if(famount(fd, argv[1], flags, "") < 0 && newnsdebug)
				fprint(2, "%s: mount: %s %s: %r\n", fn, argv[0], argv[1]);

		}else if(argc == 3){
			if(famount(fd, argv[1], flags, argv[2]) < 0 && newnsdebug)
				fprint(2, "%s: mount: %s %s %s: %r\n", 
				       fn, argv[0], argv[1], argv[2]);
		}
		close(fd);

	}
	else if(strcmp(argv0, "import") == 0){
#if 0 //%
		fd = callexport(argv[0], argv[1]);
		if(argc == 2)
			famount(fd, argv[1], flags, "");
		else if(argc == 3)
			famount(fd, argv[2], flags, "");
		close(fd);
#endif //%
	}
	else if(strcmp(argv0, "cd") == 0 && argc == 1){
		if(chdir(argv[0]) == 0 && *argv[0] == '/')
			cdroot = 1;
	}
	return cdroot;
}


#if 0 //%
static int callexport(char *sys, char *tree)
{
	char *na, buf[3];
	int fd;

	na = netmkaddr(sys, 0, "exportfs");
	if((fd = dial(na, 0, 0, 0)) < 0)
		return -1;

	if( write(fd, tree, strlen(tree)) < 0 || 
	    read(fd, buf, 3) != 2 ||  buf[0]!='O' || buf[1]!= 'K'){
		close(fd);
		return -1;
	}
	return fd;
}
#endif //%


static char* unquote(char *s)
{
	char *r, *w;
	int inquote;
	
	inquote = 0;
	for(r=w=s; *r; r++){
		if(*r != '\''){
			*w++ = *r;
			continue;
		}
		if(inquote){
			if(*(r+1) == '\''){
				*w++ = '\'';
				r++;
			}else
				inquote = 0;
		}else
			inquote = 1;
	}
	*w = 0;
	return s;
}


static int splitargs(char *p, char *argv[], char *argbuf, int nargv)
{
	char *q;
	int i, n;

	n = gettokens(p, argv, nargv, " \t\r");
	if(n == nargv)
		return 0;
	for(i = 0; i < n; i++){
		q = argv[i];
		argv[i] = argbuf;
		argbuf = expandarg(q, argbuf);
		if(argbuf == nil)
			return 0;
		unquote(argv[i]);
	}
	return n;
}


static char* nextdollar(char *arg)
{
	char *p;
	int inquote;
	
	inquote = 0;
	for(p=arg; *p; p++){
		if(*p == '\'')
			inquote = !inquote;
		if(*p == '$' && !inquote)
			return p;
	}
	return nil;
}


/*
 * copy the arg into the buffer,
 * expanding any environment variables.
 * environment variables are assumed to be
 * names (ie. < ANAMELEN long)
 * the entire argument is expanded to be at
 * most MAXARG long and null terminated
 * the address of the byte after the terminating null is returned
 * any problems cause a 0 return;
 */

static char * expandarg(char *arg, char *buf)
{
	char env[3+ANAMELEN], *p, *x;
	int fd, n, len;

	n = 0;
	while((p = nextdollar(arg))){
		len = p - arg;
		if(n + len + ANAMELEN >= MAXARG-1)
			return 0;

		memmove(&buf[n], arg, len);
		n += len;
		p++;
		arg = strpbrk(p, "/.!'$");

		if(arg == nil)
			arg = p+strlen(p);
		len = arg - p;

		if(len == 0 || len >= ANAMELEN)
			continue;

		strcpy(env, "#e/");
		strncpy(env+3, p, len);
		env[3+len] = '\0';

		fd = open(env, OREAD);
		if(fd >= 0){
			len = read(fd, &buf[n], ANAMELEN - 1);
			// some singleton environment variables have trailing NULs. 
			// lists separate entries with NULs; we arbitrarily take 
			// the first element 

			if(len > 0){
				x = memchr(&buf[n], 0, len);
				if(x != nil)
					len = x - &buf[n];
				n += len;
			}
			close(fd);
		}
	}

	len = strlen(arg);
	if(n + len >= MAXARG - 1)
		return 0;
	strcpy(&buf[n], arg);
	return &buf[n+len+1];
}


static int setenv(char *name, char *val)
{
	int f;
	char ename[ANAMELEN+6];
	long s;

	sprint(ename, "#e/%s", name);
	f = create(ename, OWRITE, 0664);
	if(f < 0)
		return -1;
	s = strlen(val);
	if(write(f, val, s) != s){
		close(f);
		return -1;
	}
	close(f);
	return 0;
}


//-----------------------------------------
void  _exits(char* s)
{
   
}
