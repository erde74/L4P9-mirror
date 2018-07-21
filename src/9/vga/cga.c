//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../port/error.h"

signed char lp49_mappages(unsigned int size, unsigned int laddr, unsigned int paddr);	// HK 20091031, @mappages.c
extern char vga_bios_map;	// HK 20091031, @devvga.c
#define BIOS_LADDR	0xB4000000	// HK 20091031, @devvga.c

enum {
	Black,
	Blue,
	Green,
	Cyan,
	Red,
	Magenta,
	Brown,
	Grey,

	Bright = 0x08,
	Blinking = 0x80,

	Yellow = Bright|Brown,
	White = Bright|Grey,
};
	
enum {
	Width		= 80*2,
	Height		= 25,

	Attr		= (Black<<4)|Grey,	/* high nibble background
						 * low foreground
						 */
};

#define CGASCREENBASE	( (uchar*) (BIOS_LADDR + 0xb8000) )

static int cgapos;
static Lock cgascreenlock;

static uchar
cgaregr(int index)
{
	outb(0x3D4, index);
	return inb(0x3D4+1) & 0xFF;
}

static void
cgaregw(int index, int data)
{
	outb(0x3D4, index);
	outb(0x3D4+1, data);
}

static void
movecursor(void)
{
	cgaregw(0x0E, (cgapos/2>>8) & 0xFF);
	cgaregw(0x0F, cgapos/2 & 0xFF);
	CGASCREENBASE[cgapos+1] = Attr;
}

static void
cgascreenputc(int c)
{
	int i;
	uchar *p;

	if(c == '\n'){
		cgapos = cgapos/Width;
		cgapos = (cgapos+1)*Width;
	}
	else if (c == '\r') {	// HK 20091031
		cgapos = (cgapos / Width) * Width;
	}
	else if(c == '\t'){
		i = 8 - ((cgapos/2)&7);
		while(i-->0)
			cgascreenputc(' ');
	}
	else if(c == '\b'){
		if(cgapos >= 2)
			cgapos -= 2;
		cgascreenputc(' ');
		cgapos -= 2;
	}
	else{
		CGASCREENBASE[cgapos++] = c;
		CGASCREENBASE[cgapos++] = Attr;
	}
	if(cgapos >= Width*Height){
		memmove(CGASCREENBASE, &CGASCREENBASE[Width], Width*(Height-1));
		p = &CGASCREENBASE[Width*(Height-1)];
		for(i=0; i<Width/2; i++){
			*p++ = ' ';
			*p++ = Attr;
		}
		cgapos = Width*(Height-1);
	}
	movecursor();
}

static void
cgascreenputs(char* s, int n)
{
	if(!islo()){
		/*
		 * Don't deadlock trying to
		 * print in an interrupt.
		 */
		if(!canlock(&cgascreenlock))
			return;
	}
	else
		lock(&cgascreenlock);

	while(n-- > 0)
		cgascreenputc(*s++);

	unlock(&cgascreenlock);
}

void
screeninit(void)
{
	// HK 20091031 begin
	if (vga_bios_map == 0) {
		if (lp49_mappages(0x60000, BIOS_LADDR + 0xa0000, 0xa0000))
			panic("cga.c: vga bios mapping error");
		vga_bios_map = 1;
	}
	// HK 20091031 end

	cgapos = cgaregr(0x0E)<<8;
	cgapos |= cgaregr(0x0F);
	cgapos *= 2;

	screenputs = cgascreenputs;
}

