#include <u.h>
#include <libc.h>

#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>


void main(int argc, char *argv[])
{
    int  fd[2];
    int  rc, i;
    char   buf[10];

#if 0
    rc = pipe(fd);
    print("pipe:%d\n", rc);
#else // Where is bug ?
    rc = bind("#|", "/mnt/temp", MREPL);
    print("bind:%d\n", rc);
    //    _d("nss");

    fd[0] = open("/mnt/temp/data", OREAD);
    print("open:%d\n", fd[0]);


    fd[1] = open("/mnt/temp/data1", OWRITE);
    print("open:%d\n", fd[1]);
#endif    

    //     sleep(100000);

    for (i = 0; i<10; i++){
      write(fd[1], "AAAA ", 5);
      read(fd[0], buf, 10);
      buf[5] = 0;
      fprint(2, "==== %s ====\n", buf);
      sleep(1000);
    }
    exits(0);
};
