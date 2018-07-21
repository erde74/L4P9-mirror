
#include <l4all.h>

L4_Word_t get_hex(void)
{
  L4_Word_t val = 0;
  char c, t;
  int i = 0;

  while ((t = c = getc()) != '\r')
  {
      switch(c)
      {
      case '0' ... '9':
	  c -= '0';
	  break;
  
      case 'a' ... 'f':
	  c -= 'a' - 'A';
      case 'A' ... 'F':
	  c = c - 'A' + 10;
	  break;
      default:
	  continue;
      };
      val <<= 4;
      val += c;
      
      /* let the user see what happened */
      l4putc(t);

      if (++i == 8)
	  break;
  };
  return val;
}
