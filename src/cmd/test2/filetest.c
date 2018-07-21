/*
 *   file bind:#f,/dev mount:/tmp,/dos/srv,/dev/fd0disk creat:test1 write:100
 *   file @1
 */

#include <u.h>
#include <libc.h>

#define NULL 0
int getopt(int nargc, char * const *nargv, const char *ostr);
extern int optind;
#define OPTIONS "s"

struct context {
    char path[3][100];
    int npath;
    int flag;
    int mode;
    char *buf;
    char *bufp;
    int bufsz;
    IOchunk chunk[10];
    int nchunk;

    int len;
    int times;
    int displen;

    int	fd;
    int size;
    int rc;

    char *cmd;
    int  disp;
};

int readn(int fd, char *buf, int len)
{
    int fd2;
    FILE *fp;
    char *rp;
    int rc;

    fd2 = dup(fd);
    fp = fdopen(fd2, "r");
    rp = fgets(buf, len, fp);
    fclose(fp);

    if (rp != NULL)
	rc = strlen(buf);
    else
	rc = 0;

    return rc;
}

void main(int argc, char *argv[])
{
    char c;
    int errflg = 0;
    int type = 0;
    int flag = 0;
    int i;
    struct context *tp;
    int silent = 0;

    while ((c = getopt(argc, argv, OPTIONS)) != -1) {
	switch (c) {
	case 's':
	    silent = 1;
	    break;
	case 'h':
	    break;
	case '?':
	    errflg++;
	    print("error occur:option %s\n", argv[optind - 1]);
	    break;
	}
    }
    if (errflg != 0) {
	_exits(0);
    }
    argc -= optind;
    argv += optind;

    if ((tp = (struct context *)malloc(sizeof(struct context))) == NULL) {
	print("cant alloc mem\n");
	_exits(1);
    }
    memset(tp, 0, sizeof(struct context));
    tp->fd = -1;
    if ((tp->buf = (char *)malloc(100)) == NULL) {
	print("cant alloc mem\n");
	_exits(1);
    }
    tp->bufsz = 100;

    for (i = 0; i < argc; i++) {
	if (argv[i][0] == '@') {
	    cmd_file(tp, argv[i] + 1);
	} else {
	    interp(tp, argv[i]);
	}
    }

    free(tp->buf);
    free(tp);

    exits(0);
}

int cmd_file(struct context *tp, char *file)
{
    char buf[100];
    int fd;
    int len;

    if ((fd = open(file, O_RDONLY)) == -1) {
	print("error\n");
	return -1;
    }
    while ((len = readn(fd, buf, 100)) > 0) {
	if (*buf == '#')
	    continue;
	*(buf + len - 1) = '\0';
	interp(tp, buf);
    }
    return 0;
}

enum {
    KW_NONE,
    KW_BIND, KW_MOUNT, 
    KW_CREATE, KW_OPEN,
    KW_READ, KW_READN, KW_READV, KW_PREAD, KW_PREADV,
    KW_WRITE, KW_WRITEN, KW_WRITEV, KW_PWRITE, KW_PWRITEV,
    KW_CLOSE,
    KW_CHDIR, KW_ACCESS, KW_STAT,
    KW_DUP, KW_REMOVE, KW_SEEK,
};

enum {
    DP_NONE, DP_BUF, DP_CHUNK, DP_DATA, DP_DATA2, DP_STAT
};

struct keyword {
    char *name;
    int  op;
    char *arg;	// p:path, f:flag, m:mode, n:len, d:data, v:dev, r,o,n:
    int  disp;
} keyword[] = {
    {"access",	KW_ACCESS,	"pm",	0},		// path, mode
    {"bind",	KW_BIND,	"pp",	0},		// device, path
    {"chdir",	KW_CHDIR,	"p",	0},		// path
    {"close",	KW_CLOSE,	"",	0},
    {"create",	KW_CREATE,	"pm",	0},		// path, mode
    {"mount",	KW_MOUNT,	"ppp",	0},		// server, old, new
    {"open",	KW_OPEN,	"pf",	0},		// path, flag
    {"pread",	KW_PREAD,	"no",	DP_BUF},	// len, offset
    {"preadv",	KW_PREADV,	"No",	DP_BUF},	// LEN, offset
    {"pwrite",	KW_PWRITE,	"do",	DP_DATA},	// data, offset
    {"pwritev",	KW_PWRITEV,	"Do",	DP_DATA2},	// DATA, offset
    {"read",	KW_READ,	"n",	DP_BUF},	// len
    {"readn",	KW_READN,	"n",	DP_BUF},	// len
    {"readv",	KW_READV,	"N",	DP_BUF},	// LEN
    {"stat",	KW_STAT,	"p",	DP_STAT},	// path
    {"write",	KW_WRITE,	"d",	DP_DATA},	// data
    {"writen",	KW_WRITEN,	"d",	DP_DATA},	// data
    {"writev",	KW_WRITEV,	"D",	DP_DATA2},	// DATA
};

#define numof(x)	(sizeof(x) / sizeof((x)[0]))

struct keyword *find_kw(char *name)
{
    struct keyword *p, *bp;
    int n, rc;

    bp = keyword;
    for (n = numof(keyword); n != 0; n >>= 1) {
	p = bp + (n >> 1);
	rc = strcmp(name, p->name);
	if (rc == 0) {
	    return p;
	} else if (rc > 0) {
	    bp = p + 1;
	    n--;
	}
    }

    return NULL;
}

int take_kw(char *s, char *c)
{
    int len = strlen(c);
    char *p;
    if ((p = memccpy(s, c, ':', len)) == NULL) {
	*(s + len) = '\0';
    } else {
	*(p - 1) = '\0';
    }

    return 0;
}

int set_context_arg(struct context *tp, char type, char *arg)
{
    char *p;
    int rc = 1;

    switch (type) {
    case 'p':
	strcpy(tp->path[tp->npath], arg);
	print("arg:p path=%s\n", tp->path[tp->npath]);
	tp->npath++;
	break;
    case 'n':
	tp->len = atoi(arg);
	print("arg:n len=%d\n", tp->size);
    case 'N':
	tp->chunk[tp->nchunk].len = atoi(arg);
	print("arg:N len=%d\n", tp->chunk[tp->nchunk].len);
	tp->nchunk++;
	rc = 0;
	break;
    case 'd':
	if ((p = strchr(arg, '+')) != NULL) {
	    *p = '\0';
	    p++;
	    tp->times = strtol(p, NULL, 0);
	} else {
	    tp->times = 1;
	}
	sprint(tp->buf, arg, '\n', '\n', '\n', '\n', '\n');
	tp->len = strlen(tp->buf);
	print("arg:d len=%d buf=%s\n", tp->len, tp->buf);
	break;
    case 'D':
	sprintf(tp->bufp, arg, '\n', '\n', '\n', '\n', '\n');
	tp->chunk[tp->nchunk].addr = tp->bufp;
	tp->chunk[tp->nchunk].len = strlen(tp->bufp);
	tp->bufp += strlen(tp->bufp);
	tp->nchunk++;
	rc = 0;
	break;
    case 'f':
	tp->flag = 0;
	if (*arg >= '0' && *arg <= '9') {
	    tp->flag = strtol(arg, NULL, 0);
	} else {
	    if (strstr(arg, "OREAD") != NULL)
		tp->flag |= OREAD;
	    if (strstr(arg, "OWRITE") != NULL)
		tp->flag |= OWRITE;
	}
	print("arg:f flag=0%o\n", tp->flag);
	break;
    case 'm':
	tp->mode = 0;
	if (*arg >= '0' && *arg <= '9') {
	    tp->mode = strtol(arg, NULL, 0);
	}
	print("arg:m mode=0%o\n", tp->mode);
	break;
    default:
	break;
    }

    return rc;
}

int parse_args(struct context *tp, struct keyword *kp, char *cmd)
{
    char *ap;
    char *cp = cmd;
    char *p;
    int n;
    char arg[100];

    if ((p = strchr(cmd, '#')) != NULL) {
	*p = '\0';
    }
    if ((cp = strchr(cmd, ':')) == NULL)
	return 0;
    cp++;
    tp->nchunk = 0;
    tp->bufp = tp->buf;
    for (ap = kp->arg; *ap != '\0'; ) {
	strdiv(arg, 100, &cp, ",");
	if (*arg == ',')
	    strdiv(arg, 100, &cp, ",");
	if (*arg == '\0')
	    break;
	if (set_context_arg(tp, *ap, arg) == 1)
	    ap++;
    }

    return 0;
}

int interp(struct context *tp, char *cmd)
{
    struct keyword *kp;
    char name[100];
    int i, off;
    char *tmp;
    struct stat sb;

    print("int:%s\n", cmd);
    take_kw(name, cmd);
    print("kw:%s\n", name);
    if ((kp = find_kw(name)) == NULL) {
	print("not kw:%s\n", cmd);
	return -1;
    }
    if (parse_args(tp, kp, cmd) < 0) {
	print("bad arg\n");
	return -1;
    }
    switch (kp->op) {
    case KW_STAT:
	tp->rc = stat(tp->path[0], &sb);
	break;
    case KW_ACCESS:
	tp->rc = access(tp->path[0], tp->mode);
	break;
    case KW_OPEN:
	tp->fd = tp->rc = open(tp->path[0], tp->flag, tp->mode);
	break;
    case KW_CREATE:
	tp->fd = tp->rc = create(tp->path[0], tp->mode);
	break;
    case KW_READ:
	tp->rc = read(tp->fd, tp->buf, tp->len);
	break;
    case KW_PREAD:
	tp->rc = pread(tp->fd, tp->buf, tp->len);
	break;
    case KW_READN:
	tp->rc = readn(tp->fd, tp->buf, tp->len);
	break;
    case KW_READV:
	off = 0;
	for (i = 0; i < tp->nchunk; i++) {
	    tp->chunk[i].addr = tp->buf + off;
	    off += tp->chunk[i].len;
	}
	tp->rc = readv(tp->fd, tp->chunk, tp->nchunk);
	break;
    case KW_PREADV:
	tp->rc = preadv(tp->fd, tp->chunk, tp->nchunk);
	break;
    case KW_WRITE:
	if (tp->buf[0] == '\0' && tp->times != 0) {
	    if ((tmp = (char *)malloc(tp->times)) == NULL) {
		print("error\n");
		return -1;
	    }
	    memset(tmp, 0, tp->times);
	    tp->rc = write(tp->fd, tmp, tp->times);
	    free(tmp);
	} else {
	    for (i = 0; i < tp->times; i++ ) {
		tp->rc = write(tp->fd, tp->buf, tp->len);
	    }
	}
	tp->times = 0;
	break;
    case KW_PWRITE:
	tp->rc = pwrite(tp->fd, tp->chunk, tp->nchunk);
	break;
    case KW_WRITEV:
	tp->rc = writev(tp->fd, tp->chunk, tp->nchunk);
	break;
    case KW_PWRITEV:
	tp->rc = pwritev(tp->fd, tp->chunk, tp->nchunk);
	break;
    case KW_CLOSE:
	close(tp->fd);
	break;
    default:
	break;
    }

    if (tp->rc == -1) {
	perror(cmd);
    }
    if (tp->rc >= 0) {
	if (kp->disp == DP_BUF) {
	    tp->buf[tp->rc] = '\0';
	    print("read:%s\n", tp->buf);
	}
    }

    return 0;
}
