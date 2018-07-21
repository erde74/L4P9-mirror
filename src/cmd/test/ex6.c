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

#define  EBPCHK(s)  // print("EBP-%s:%x \n", s, _EBP)

#define  STKCHK(ss) {register uint *stkptr  asm("%esp");   \
    register uint *frmptr  asm("%ebp");  \
    print("%s<sp:%X,fp:%X,rtn:%X> ", ss, stkptr, frmptr, frmptr[1]); }


void main(int argc, char *argv[])
{
    L4_ThreadId_t  myTid = L4_Myself();
    int   i;
    int   tasknr, localnr, version;
    char  msg[100]={0};
    char *progname;
    register void *stkptr   asm("%esp");
    register void *frameptr asm("%ebp");

    print("==== EX6  [%X-%X] sp=%x fp=%x====\n", myTid, &myTid, stkptr, frameptr);

    for(i = 0; i<argc; i++) {
        strcat(msg, " : "); 
	strcat(msg, argv[i]);
    }

    print("argc=%d &argc=%X &argv=%X\n", argc, &argc, &argv);
    for(i = 0; i<argc; i++)
       print("argv[%d] %X %X =%s\n", i, &argv[i], argv[i], argv[i]);
    progname = argv[1];

    {
        unsigned  pid;
	unsigned  flag = RFFDG | RFREND | RFPROC;  
	int  i;

	STKCHK("A");
	pid = rfork(flag);
	print("fork();%d \n", pid);    
	sleep(100);

	if (pid == 0) {
	    print("I am a forked child: %X. \n", L4_Myself());
	    STKCHK("B");
	    L4_Yield();
	    sleep(400);
	    
	    if (progname) {
	        print("Child process: program '%s' is starting. \n", progname);   
	        exec(progname, 0);
	    }
	    else {
	        bob();
	    }
	    L4_Sleep(L4_Never);
	}
	else {
	    EBPCHK("fork-50"); //
	    STKCHK("C");
	    print("I am a parent; forked child is:%d \n",  pid);
	    alice();
	}
    }


#if 0
      //------- exec  <progname>  ----------------
    if (!strcmp(cmd, "exec")) {
	char *name = strtok(NULL, " ");
	int  i;
	argv[0] = name;

	for (i = 1; i<16 ; i++) {
	    argv[i] = strtok(NULL, " &\n");
	    if (argv[i] == NULL)
	        break;
	}

	PR("# exec(");
	for(i=0; i<16; i++) {
	    PR("%s ", argv[i]);
	    if(argv[i] == 0) break;
	}
	PR(")\n");
	rc = exec(name, argv);
	EPR(rc);
    }
#endif

    // L4_Sleep (L4_Never);
    exits(0);
};
