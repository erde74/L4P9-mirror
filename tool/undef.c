#include <stdio.h>
#include <string.h>

main(int argc, char **argv)
{
  char   u[100], v[100], w[100];
  char   buf[256];
  char * mark = ": undefined reference to ";
  int    marklen = strlen(mark);
  char * cp;
  FILE * fp;
  char * line = NULL;
  int    len = 0, i=0;
  char * filename = argv[1];
  //  printf("%s\n", filename);  printf("%d \n", marklen);
 
  fp = fopen(filename,  "r");
  if (fp == NULL)    return 0;
  while ((len = getline(&line, &len, fp)) != -1) {

    for (cp = line; *cp && *(cp+marklen) ; cp++)
      {
#if 1
	if (strncmp(cp, mark, marklen) == 0) {
	  printf("%s", cp+marklen);
	  break;
	  }
#else 
	u[0] = v[0] = w[0] = 0; 
	if (sscanf(line, "%s: undefined reference to '%s' %s",  u, v, w) > 0)
	  printf("[%d] %s | %s | %s\n", i++, u, v, w);
#endif
      }
  }

  if (line)
    free(line);
  return 0;
}
