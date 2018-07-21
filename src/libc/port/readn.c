#include <u.h>
#include <libc.h>

//int l4printf_c(int color, const char *fmt, ...);

long
readn(int f, void *av, long n)
{
	char *a;
	long m, t;

	a = av;
	t = 0;
	while(t < n){
		m = read(f, a+t, n-t);

		if (m<0) print("readnEr1:%d ", m);
		if(m <= 0){
			if(t == 0)
				return m;
			break;
		}
		t += m;
	}
	if (t<0) print("readnEr2:%d ", t);
	return t;
}
