#include <l4all.h>
#include <l4/l4io.h>
#include <lp49/lp49.h>
#include  <u.h>
#include  <libc.h>


void check_clock(int  which, char *msg);  // which : 'a', 'b',,,,
static void print_clocks(char  which, int diff);

int  sizelist[] = {16, 256, 1024, 4096, 8192};

char buf[1024*10];

void fread()
{
  int  fd, n, sz, i, j;
    print("==== /work/file8k read time ====\n" );
    fd = open("/work/file8k", OREAD);
    if (fd < 0)   exits(0);

    print_clocks('f', 1);
	
    for(i=0; i<4; i++)
      for (j = 0; j < sizeof(sizelist)/4; j++){
        sz = sizelist[j];
	print("Size=%d  \t", sz);
	check_clock('f', ">pread");
	n = pread(fd, buf, sz, 0);
	if (n != sz) print("n != sz \n");
	check_clock('f', "<pread");
	print_clocks('f', 1);
      }
}

void fwrite()
{
    int  fd, n, sz, i, j;
    print("==== /work/file8k write time ====\n" );
    fd = create("/work/zzz", ORDWR, 0777);
    if (fd < 0)   exits(0);

    for(i=0; i<4; i++)
      for (j = 0; j < sizeof(sizelist)/4; j++){
        sz = sizelist[j];
	print("Size=%d  \t", sz);
	check_clock('f', ">pread");
	n = pwrite(fd, buf, sz, 0);
	if (n != sz) print("n != sz \n");
	check_clock('f', "<pread");
	print_clocks('f', 1);
      }
}

void remoteread()
{
  int  fd, n, sz, i, j;
    print("==== /n/work/file8k read time ====\n" );
    fd = open("/n/work/file8k", OREAD);
    if (fd < 0)   exits(0);

    print_clocks('f', 1);
	
    for(i=0; i<4; i++)
      for (j = 0; j < sizeof(sizelist)/4; j++){
        sz = sizelist[j];
	print("Size=%d  \t", sz);
	check_clock('f', ">pread");
	n = pread(fd, buf, sz, 0);
	if (n != sz) print("n != sz \n");
	check_clock('f', "<pread");
	print_clocks('f', 1);
      }
}


//-----------------------
static uchar  getuchar()
{
    char cbuf[4]; 
    uchar  ch;
    read(0, cbuf, 1);  
    ch = cbuf[0];
    return  ch;
}

void main(int argc, char *argv[])
{
  uchar ch, i;
    print("==== FILE BENCHIMARK ====\n" );

    for (i = 0; i<argc; i++)
      print("argv[%d]:  %s\n", i, argv[i]);

    if(strcmp(argv[1], "0") == 0)
      exits(0);
    else 
    if(strcmp(argv[1], "1") == 0)
      fread();
    else
    if(strcmp(argv[1], "2") == 0)
      fwrite();
    else
    if(strcmp(argv[1], "3") == 0)
      remoteread();

#if 0
    while(1) {
      print("Which one ?  fread:'1' fwrite:'2'  exit:'0' RETURN\n"); 
      ch = getuchar();
      print("[%c]\n", ch);

      if (ch == '1')	   fread();
      else if (ch == '2')  fwrite();
      else if (ch == '0')  break;
    }
#endif
    sleep(1000);
    exits(0);
};
