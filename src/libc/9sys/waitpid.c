//%%%%%%%%%%%%%%
#include <u.h>
#include <libc.h>
#include <fcall.h>

int
waitpid(void)
{
        int n, pid;
	char buf[512], *fld[5];
	//print(">waitpid()\n");

	n = await(buf, sizeof buf-1);
	if(n <= 0){
	        print("<waitpid-1()\n");
		return -1;
	}
	buf[n] = '\0';
	if(tokenize(buf, fld, nelem(fld)) != nelem(fld)){
		werrstr("couldn't parse wait message");
		print("<waitpid-2()\n");
		return -1;
	}
	pid = atoi(fld[0]);
	//print("<waitpid():%d\n", pid);
	return  pid;
}

