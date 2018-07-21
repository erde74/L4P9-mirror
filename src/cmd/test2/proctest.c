#include <u.h>
#include <libc.h>

#define NULL 0

/*
 * type:  fork, rfotk
 * flag:  ...
 */

int getopt(int nargc, char * const *nargv, const char *ostr);
extern int optind;
extern char *optarg;

struct check_func {
    int tyep;
    int (*before)();
    int (*parent)();
    int (*child)();
};

enum { TYPE_FD };

int check_fd_before();
int check_fd_child();

struct check_func check_func[] = {
    {TYPE_FD, check_fd_before, NULL, check_fd_child}
};

struct context {
    int (*before)();
    int (*parent)();
    int (*child)();

    int fd;
    char *filename;
} context;

#define OPTIONS "t:f:"

void main(int argc, char *argv[])
{
    char c;
    int errflg = 0;
    int type = 0;
    int flag = 0;
    char *filename;
    int pid;
    int rc = 0;
    struct context *tp;

    while ((c = getopt(argc, argv, OPTIONS)) != -1) {
	switch (c) {
	case 't':
	    type = strtol(optarg, NULL, 0);
	    break;
	case 'f':
	    filename = optarg;
	    break;
	case 'h':
	    print("\n");
	    print("\n");
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

    if ((tp = (struct context *)malloc(sizeof( struct context))) == NULL) {
	print("malloc error\n");
	_exits(1);
    }
    memset(tp, 0, sizeof(struct context));
    if (set_context(tp, type) < 0) {
	_exits(1);
    }

    if (tp->before != NULL)
	(*tp->before)();

    if (type == 0) {
	pid = fork();
    } else if (type == 1) {
	pid = fork();
    }
    if (pid == -1) {
	print("error fork");
	_exits(1);
    }

    if (pid == 0) {
	if (tp->child != NULL)
	    rc = (*tp->child)();
	print("Child:rc=%d\n", rc);
    } else {
	if (tp->parent != NULL)
	    rc = (*tp->parent)();
	print("Parent:rc=%d\n", rc);
    }

    exits(0);
}

int set_context(struct context *tp, int type)
{
    switch (type) {
    case TYPE_FD:
	tp->before = check_fd_before;
	tp->parent = NULL;
	tp->child = check_fd_child;
	break;
    default:
	break;
    }

    return 0;
}

int check_fd_before(struct context *tp)
{
    print("check_fd_befor\n");
    tp->fd = open(tp->filename, O_WRONLY|O_CREAT, 0666);
    return 0;
}

int check_fd_child()
{
    print("check_fd_child\n");
    return 0;
}

int check_env()
{
    return 0;
}

int check_mount()
{
    return 0;
}
