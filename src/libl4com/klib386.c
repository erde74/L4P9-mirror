/*--------------------------------------------------------+ 
 |                  klib386.c                             |
 |                                                        |
 |           2001.09.20  06.09.06 K.M.                    |
 +--------------------------------------------------------*/

#include <l4all.h>
#include <l4/l4io.h>

#define  DBGPRN  if(0)l4printf_g
#define  DBGBRK  l4printgetc

//------------ KM. Bad escape from type check --------------------
#define   port_t      int  
#define   ushort      unsigned short
#define   ulong       unsigned long


/*================================================================*/

int inb(int port)
{
  unsigned char data;
  __asm__ __volatile__("inb %%dx,%0" : "=a" (data) : "d" (port));
  DBGPRN("inb: %x -> %x \n", port, data);
  return (int)data;
}
                                                                               
ushort ins(int port)
{
  unsigned short data;
  __asm__ __volatile__("inw %%dx,%0" : "=a" (data) : "d" (port));
  DBGPRN("ins %x -> %x \n", port, data);
  return  data;
}
                                                                               
int inw(int port)
{
  unsigned int data;
  __asm__ __volatile__("inw %%dx,%0" : "=a" (data) : "d" (port));
  DBGPRN("inw: %x -> %x \n", port, data);
  return (int)data;
}
                                                                               
ulong  inl(int port)
{
  unsigned int data;
  __asm__ __volatile__("inl %%dx,%0" : "=a" (data) : "d" (port));
  DBGPRN("inw: %x -> %x \n", port, data);
  return (ulong)data;
}
                                                                               
void  insb(int port, void* addr, int count)
{
  __asm__ __volatile__("cld; rep; insb" : "+D"(addr), "+c"(count) : "d"(port));                                                                               
  DBGPRN("insb %x -> %x,%x \n", port, addr, count);
}
                                                                               
void  inss(int port, void* addr, int count)
{
  __asm__ __volatile__("cld; rep; insw" : "+D"(addr), "+c"(count) : "d"(port));                                                                               
  DBGPRN("inss: %x -> %x,%x \n", port, addr, count);
}
                                                                               
void  insl(int port, void* addr, int count)
{
  __asm__ __volatile__("insl; rep; insl" : "+D"(addr), "+c"(count) : "d"(port));
  DBGPRN("insl: %x -> %x,%x \n", port, addr, count);
}
                                                                               
void outb(int port, int value)
{
  unsigned char data = (unsigned char)value;
  DBGPRN("outb: %x <- %x \n", port, value);
  __asm__ __volatile__("outb %0,%%dx" : : "a" (data), "d" (port));
}
                                                                               
void  outs(int port, ushort value)
{
  unsigned short data = (unsigned short)value;
  DBGPRN("outs: %x <- %x \n", port, value);
  __asm__ __volatile__("outw %0,%%dx" : : "a" (data), "d" (port));
}
                                                                               
void outw(int port, int value)
{
  unsigned short data = (unsigned short)value;
  DBGPRN("outw: %x <- %x \n", port, value);
  __asm__ __volatile__("outw %0,%%dx" : : "a" (data), "d" (port));
}
                                                                               
void  outl(int port, ulong value)
{
  ulong  data = (ulong)value;
  DBGPRN("outl: %x <- %x \n", port, value);
  __asm__ __volatile__("outl %0,%%dx" : : "a" (data), "d" (port));
}
                                                                               
void  outsb(int port, void* addr, int count)
{
  DBGPRN("outsb: %x <- %x,%x \n", port, addr, count);
  __asm__ __volatile__("rep; outsb" : "+S"(addr), "+c"(count) : "d"(port));
}
                                                                               
void  outss(int port, void* addr, int count)
{
  DBGPRN("outss: %x <- %x,%x \n", port, addr, count);
  __asm__ __volatile__("rep; outsw" : "+S"(addr), "+c"(count) : "d"(port));
}
                                                                               
void  outsl(int port, void* addr, int count)
{
  DBGPRN("outsl: %x <- %x,%x \n", port, addr, count);
  __asm__ __volatile__("rep; outsl" : "+S"(addr), "+c"(count) : "d"(port));
}
                                                                               

/*=====================================================================*/
int in_byte(port_t port)
{
  unsigned char data;
  __asm__ __volatile__("inb %%dx,%0" : "=a" (data) : "d" (port));
  return (int)data;
}

int in_word(port_t port)
{
  unsigned short data;
  __asm__ __volatile__("inw %%dx,%0" : "=a" (data) : "d" (port));
  return (int)data;
}

void out_byte(port_t port, int value)
{
  unsigned char data = (unsigned char)value;
  __asm__ __volatile__("outb %0,%%dx" : : "a" (data), "d" (port));
}

void out_word(port_t port, int value)
{
  unsigned short data = (unsigned short)value;
  __asm__ __volatile__("outw %0,%%dx" : : "a" (data), "d" (port));
}

/*=====================================================================*/
void port_read(port_t port, unsigned destination, unsigned bytecount)
{
  __asm__ __volatile__(
		       "shrl $1,%%ecx  \n"
		       "cld ; rep ; insw"
		       : : "d" (port), "D" (destination), "c" (bytecount)
		       );
}

void port_read_byte(port_t port, unsigned destination,
			   unsigned bytecount)
{
  __asm__ __volatile__(
		       "cld ; rep ; insb"
		       : : "d" (port), "D" (destination), "c" (bytecount)
		       );
}

void port_write(port_t port, unsigned source, unsigned bytecount)
{
  __asm__ __volatile__ (
			"shrl $1,%%ecx  \n"
			"cld ; rep ; outsw"
			: : "d" (port),   "S" (source),  "c" (bytecount)
			);
}


void port_write_byte(port_t port, unsigned source,
			    unsigned bytecount)
{
  __asm__ __volatile__ (
			"cld ; rep ; outsb"
			: : "d" (port),   "S" (source),  "c" (bytecount)
			);
}

/*============== klib_lock, klib_unlock ==========================*/

void klib_lock()
{
  __asm__ __volatile__ ("cli");
}

void klib_unlock()
{
  __asm__ __volatile__ ("sti");
}

/*================== 	enable_irq, disable_irq	 ==================*/
void enable_irq(unsigned irq)
{
  unsigned int flags;
  unsigned int mask = ~(1 << irq);
  unsigned int old, new;
        
  __asm__ __volatile__ ("pushfl ; popl %0" : "=r" (flags));
  klib_lock();
  if (irq < 8)
    {
      old  = in_byte(0x21);
      new = old & mask;
      out_byte(0x21, new);
    }
  else
    {
      old = in_byte(0xA1);
      new = old & (mask >> 8);
      out_byte(0xA1, new);
    }
  __asm__ __volatile__ ("pushl %0 ; popfl" : : "r" (flags));
}


int disable_irq(unsigned irq)
{
  unsigned int flags;
  unsigned int mask = 1 << irq;
  unsigned int old, new, ret;

  __asm__ __volatile__ ("pushfl ; popl %0" : "=r" (flags));
  klib_lock();
  if (irq < 8)
    {
      old  = in_byte(0x21);
      if ((new = old | mask) == old)
	ret = 0;
      else
	{
	  out_byte(0x21, new);
	  ret = 1;
	}
    }
  else
    {
      old = in_byte(0xA1);
      if ((new = old | (mask >> 8)) == old)
	ret = 0;
      else
	{
	  out_byte(0xA1, new);
	  ret = 1;
	}
    }
  __asm__ __volatile__ ("pushl %0 ; popfl" : : "r" (flags));
  return ret;
}

/*====================_mem_copy =======================*/
void _mem_copy(unsigned source, unsigned destination,
		      unsigned bytecount)
{
  __asm__ __volatile__ (
			"cld ; rep ; movsb"
			: : "S" (source), "D" (destination), "c" (bytecount)
			);
}


//---------------------------------------------------
// void  interrupt(unsigned int id) {
//   int msg[16];
//   msg[1] = HARD_INT;
//   _send(id, msg);
// }



//------------------------------------------------
unsigned  get_myID( )
{
  L4_ThreadId_t  myself;
  myself = L4_Myself();
  return  myself.raw;  
}


//------------------------------------------------
//KM   This function must be refined. 
// void wait_io_complete()
// {
//     L4_Receive(IRQ14_TH_id );
// }


//-------------------------------------------------
void milli_delay(unsigned  ms)
{
    L4_Sleep(L4_TimePeriod(ms * 1000));
}






