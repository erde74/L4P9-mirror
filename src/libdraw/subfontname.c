#include <u.h>
#include <libc.h>
#include <draw.h>

/*
 * Default version: convert to file name
 */

char*
subfontname(char *cfname, char *fname, int maxdepth)
{
	char *t, *u, *tmp1, *tmp2;
	int i;

	t = strdup(cfname);  /* t is the return string */
	if(strcmp(cfname, "*default*") == 0)
		return t;
	if(t[0] != '/'){
		tmp2 = strdup(fname);
		u = utfrrune(tmp2, '/');
		if(u)
			u[0] = 0;
		else
			strcpy(tmp2, ".");
		tmp1 = smprint("%s/%s", tmp2, t);
		free(tmp2);
		free(t);
		t = tmp1;
	}

	if(maxdepth > 8)
		maxdepth = 8;

	for(i=3; i>=0; i--){
		if((1<<i) > maxdepth)
			continue;
		/* try i-bit grey */
		tmp2 = smprint("%s.%d", t, i);
		if(access(tmp2, AREAD) == 0) {
			free(t);
			return tmp2;
		}
	}

	/* try default */
	if(access(t, AREAD) == 0)
		return t;

	return nil;
}
