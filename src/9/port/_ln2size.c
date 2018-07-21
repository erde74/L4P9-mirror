#include <stdio.h>
typedef  unsigned int uint;


int ln2size(int size)
{
    int s, m;
    for (s=0, m=1; s<30; s++, m*=2)
      if (m >= size) return s;
    return 0;
}

int  get_fpage(uint adrs, uint size, uint *fpagebase, uint *fpagesize)
{
  uint  s1 = ln2size(size);
  uint  s1size;
  uint  s1mask;
  uint  pagebase;

  s1 = ln2size(size);
  if(s1<12) s1 = 12;

  for(; s1<20; s1++) {
    s1size = 1<<s1;
    s1mask = ~(s1size - 1);
    pagebase = adrs & 0xFFFFF000 & s1mask;
    //  printf("[%d %x %x ] ", s1size, s1mask, pagebase);

    if((pagebase <= adrs) && ((adrs+size) <= (pagebase + s1size))) {
	  *fpagebase = pagebase;
	  *fpagesize = s1size;
	  printf(" <%x %x %x> -> <%x %x>\n", 
		 adrs, size, adrs+size, pagebase, s1size);
	  return 1;
    }
  }
  return  0;

}

void main()
{
  uint  u, v;
  get_fpage(10000, 560, &u, &v);
  get_fpage(200000, 5600, &u, &v);
  get_fpage(300000, 4000, &u, &v);
  get_fpage(400000, 5600, &u, &v);
  get_fpage(500000, 8000, &u, &v);
  get_fpage(600000, 60000, &u, &v);
  get_fpage(700000, 5600, &u, &v);

  get_fpage(15000, 560, &u, &v);
  get_fpage(400000, 5600, &u, &v);
  get_fpage(380000, 4000, &u, &v);
  get_fpage(490000, 5600, &u, &v);
  get_fpage(5000000, 80000, &u, &v);
  get_fpage(650000, 60000, &u, &v);
  get_fpage(780000, 5600, &u, &v);


}
