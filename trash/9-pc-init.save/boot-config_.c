//%%%%%%%%%%%%%%%%%%%%%

#include  <u.h>
#include  <libc.h>
#include  "boot.h"   //%

Method  method[] = {
  //%    {"tcp",  configtcp, connecttcp, },
  //%    {"il",   configil,  connectil, },
  //%  {"local", configlocal, connectlocal, }
};

int  cpuflag = 0;
char  *rootdir = "/root";
char  *bootdisk = "#S/sdC0/";
extern  void  boot(int, char **);

void main(int argc, char ***argv)
{
    boot(argc, argv);
}

int  (*cfs)(int) = 0;

