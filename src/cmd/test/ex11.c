#include <u.h>
#include <libc.h>

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>


void alice()
{
    int i;
    for (i = 0; i<10; i++){
        print("I am Alice. \n");
        sleep(300);
    }
}

void bob()
{
    int i;
    for (i = 0; i<10; i++){
      print("                 I am Bob. \n");
      sleep(500);
    }
    exits(0);
}


void main(int argc, char *argv[])
{
    L4_ThreadId_t  myTid = L4_Myself();
    int   i, num;
    int   tasknr, localnr, version;
    char  msg[100]={0};
    char *progname;

    num = atoi(argv[1]);
    print("==== EX11  [%X-%X] ====\n", myTid, &myTid);

    for(i=0; i<num; i++){
        unsigned  pid;
	unsigned  flag = RFNAMEG | RFFDG | RFENVG | RFPROC;  

	pid = rfork(flag);
	if (pid == 0) { // child process
	    int  j = i;
	    for(;;){
		sleep(1000);
	        print("<%d>  ",  j);
	    }
	}
	else { // Parent process
	    print("Process[%d] is forked  \n", pid);    
	    sleep(20);
	}
    }

    sleep(10000);

    // L4_Sleep (L4_Never);
    exits(0);
};
