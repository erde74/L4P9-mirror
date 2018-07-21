#include  <stdio.h>

//---------------------------------------------------
/*    argblk:
 *    +--------+
 *    |  size  |
 *    +--------+
 *    | argc   |   ex. =2
 *    +--------+
 *    | argv   |   == 12
 *    +--------+
 *    | argp[0]|  argv[] pointer to argument
 *    +--------+
 *    | argp[1]|
 *    +--------+
 *    |  NULL  |
 *    +--------+
 *    | argstr |   argp[argc+1]
 *    |        |
 *    |        |
 *    +--------+
 */

struct {
    int       size;
    int       argc;
    void*     argv;
    void*     argp[32];
} argblk;


void mkargblk(char *cmdline)
{
    int    argc = 0, size = 0, i;
    char*  argstring;
    char   ch;
    int    len, flag = 0;

    printf("----%s-----\n", cmdline);
    for(len=0; cmdline[len]; len++);
    if (len>128) printf("too long\n");
  
    for(i=0; ch = cmdline[i]; i++) { // count the ARGC
        if (ch == ' ') {
	    flag = 0;
	}
	else if (ch >= '!' && ch <= '~') {
	  if (flag == 0) argc++;
	  flag = 1;
	}
	else 
	  break;
    }
    size = i;
  
    argblk.argc = argc;
    argblk.argv = (void*)12;
  
    argstring = (char*)&argblk.argp[argc+1];
    argstring[0] = 0;
    for (i=0; argstring[i] = cmdline[i]; i++);
    argstring[127] = 0;

    argc = 0;
    flag = 0;
    for(i=0; ch = argstring[i]; i++) {
        if (ch == ' ') {
	    flag = 0;
	    argstring[i] = 0;
	}
	else if (ch >= '!' && ch <= '~') {
	    if (flag == 0){
	      argblk.argp[argc++] = (void*)((int)&argstring[i] - (int)&argblk);
	    }
	    flag = 1;
	}
	else 
	    break;
    }

    size += 3*sizeof(int) +  (argc+1)*sizeof(void*);
    argblk.size = size;

    printf("size=%d argc=%d argv=%d \n", argblk.size, argblk.argc, argblk.argv);
    for (i = 0; i<argblk.argc; i++)
        printf("[%d]%s\n", i, (int)argblk.argp[i]+(int)&argblk);
}


main()
{
  mkargblk("aa -b -c -df ssss");

  mkargblk("bb -db  -dc   -ddf ssss -z");

  mkargblk("cc -ccc  -c -cdf  sss -x -y   ssss");
}
