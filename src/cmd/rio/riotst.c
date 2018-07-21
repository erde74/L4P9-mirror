#include <u.h>
#include <libc.h>

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>

#define  CONSOLE  "/dev/cons" 

#define  DXX(n) {print("%s %s:%d ", (n==0)?"\n":"", __FUNCTION__, n);sleep(100);}

void main(int argc, char *argv[])
{
    L4_ThreadId_t  myTid = L4_Myself();
    int   srvfd;
    char  *wsys;
    char  attr[64];
    int   cc;
    int   pid;
    char *tt = "QWERTY123456 \n";

    register void *stkptr   asm("%esp");
    register void *frameptr asm("%ebp");
    print("==== riotest  [%X-%X] sp=%x fp=%x====\n", 
	  myTid, &myTid, stkptr, frameptr);

    pid = getpid();
    if ((wsys = getenv("wsys")) == nil)  
        exits(0);

    print(" riotst:wsys:'%s'\n", wsys);
    srvfd = open(wsys, ORDWR);
    free(wsys);
    if (srvfd < 0)  
        exits(0);

    rfork(RFNAMEG | RFFDG | RFENVG); 
    sprint(attr, "N %d 0 0 600 400", pid);

    cc = mount(srvfd, -1, "/mnt/wsys", 0, attr);
    print(" riotst:mount(%d, -1, /mnt/wsys, 0, '%s'):%d \n", srvfd, attr, cc);
    sleep(50);

    cc = bind("/mnt/wsys", "/dev", MBEFORE);
    print(" riotst:bind(/mnt/wsys, /dev, MBEFORE):%d \n", cc);
    sleep(50);
    //Cf.      mount(srvfd, -1, "/dev", MBEFORE, attr); 

    close(0);
    if(open(CONSOLE, OREAD) < 0){
        fprint(2, "can't open /dev/cons: %r\n");
    }
    close(1);
    if(open(CONSOLE, OWRITE) < 0){
        fprint(2, "can't open /dev/cons: %r\n");
    }
    close(2);
    dup(1, 2);

    //write(1, tt, strlen(tt)); 
    write(1, "Window-start \n", 14);

    print(" execl('/bin/rc', 'rc', '-i', nil) \n");
    sleep(100);
    execl("/bin/rc", "rc", "-i", nil);
    //    execl("/bin/qsh", "qsh", nil);
    print(" execl(...):Failed \n");

    // will not come here.... 
    exits(0);
};
