#include <l4all.h>
#include <l_actobj.h>
#include <u.h>
#include <libc.h>

void wind(int minx, int miny, int maxx, int maxy, char *cmd, char *argv);

int main(int argc, char *argv[])
{
	int pid;

	pid = rfork(RFPROC | RFNAMEG);
	if (pid == -1)
		return 1;
	if (pid == 0) {	// child
		char *argv[] = { "/bin/rio", nil };
		exec(argv[0], argv);
		return 0;
	}
	sleep(10000);

if (0)
	{
		char *arg[] = { "ls", "env", nil };
		exec("/bin/ls", argv);
	}


	wind(0, 0, 161, 117, "stats", "-lmisce");
	wind(161, 0, 560, 117, "face", "-i");
	wind(20, 140, 610, 450, "firstwin", "");	// cat readme.rio ; exec rc -i

	sleep(99999999);
	return 0;
}

void wind(int minx, int miny, int maxx, int maxy, char *cmd, char *argv)
{
	int pid;
	pid = rfork(RFPROC | RFNAMEG);
	if (pid == -1)
		return;
	if (pid == 0) {	// child
		int   envfd, srvfd;
		char  wsys[20];
		char  attr[64];
		int   cc;
		int   pid;
		pid = getpid();
		if ((envfd = open("/env/wsys", OREAD))<0) return;
		if (read(envfd, wsys, sizeof wsys)<0) return;
		close(envfd);

	//	print("wsys:'%s'\n", wsys);
		srvfd = open(wsys, OREAD);
		sprint(attr, "N %d %d %d %d %d", pid, minx, miny, maxx, maxy);

	//	print(" mount(%d, '%s') \n", srvfd, attr);
      		cc = mount(srvfd, -1, "/mnt/wsys", 0, attr);
	//	print(" mount(%d, '%s'):%d \n", srvfd, attr, cc);

 		cc = bind("/mnt/wsys", "/dev", MREPL);
	//	print(" bind():%d \n", cc);

		exits(0);

	}
	if (pid > 0)
		sleep(10000);
	return;
}

