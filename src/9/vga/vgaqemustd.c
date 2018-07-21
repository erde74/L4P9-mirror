//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include "u.h"
#include "../port/lib.h"
#include "../pc/mem.h"
#include "../pc/dat.h"
#include "../pc/fns.h"
#include "../pc/io.h"
#include "../port/error.h"
#include "ureg.h"

#define	Image	IMAGE
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "../pc/screen.h"

#define WORD(p) ((p)[0] | ((p)[1]<<8))
#define LONG(p) ((p)[0] | ((p)[1]<<8) | ((p)[2]<<16) | ((p)[3]<<24))
#define PWORD(p, v) (p)[0] = (v); (p)[1] = (v)>>8
#define PLONG(p, v) (p)[0] = (v); (p)[1] = (v)>>8; (p)[2] = (v)>>16; (p)[3] = (v)>>24

// HK 20090831 begin

// sigma0:[VRAM_PADDR ... ] is mapped to hvm:[VRAM_LADDR ... ].
#define VRAM_PADDR  0xE0000000
#define VRAM_LADDR  0xB0000000

signed char lp49_mappages(unsigned int size, unsigned int laddr, unsigned int paddr);	// HK 20091031, @mappages.c

#if 0
#define ROUND(x, y)	( ((x) + (y) - 1) & ~(-(y)) )
#define PGROUND(x)	ROUND(x, 4096)
#endif

#if 0
static void outb(int port, int value)
{
	unsigned char data = (unsigned char) value;
	__asm__ __volatile__("outb %0,%%dx" : : "a"(data), "d"(port));
	return;
}
#endif

static void outw(int port, int value)
{
	unsigned short data = (unsigned short) value;
	__asm__ __volatile__("outw %0,%%dx" : : "a"(data), "d"(port));
	return;
}

#if 0
static int inb(int port)
{
	unsigned char data;
	__asm__ __volatile__("inb %%dx,%0" : "=a"(data) : "d"(port));
	return (int) data;
}
#endif

static int inw(int port)
{
	unsigned short data;
	__asm__ __volatile__("inw %%dx,%0" : "=a"(data) : "d"(port));
	return (int) data;
}

static void set_qemuvga0reg(int reg, int data)
{
	outw(0x01ce, reg);
	outw(0x01cf, data);
	return;
}

static int get_qemuvga0reg(int reg)
{
	outw(0x01ce, reg);
	return inw(0x01cf);
}

static int is_stdvga()
{
	set_qemuvga0reg(0x0000, 0xb0c2);
	return get_qemuvga0reg(0x0000) == 0xb0c2;
}

static void linear(VGAscr *scr, int size, int align)
{
	if (!is_stdvga())
		error("qemu-stdvga device not found");
	vgalinearaddr(scr, VRAM_LADDR, 16 * 1024 * 1024);
	scr->gscreen->data->bdata = VRAM_LADDR;
	return;
}

static char vmap_flag = 0;

static void enable(VGAscr *scr)
{
	if (vmap_flag == 0) {
		if (lp49_mappages(16 * 1024 * 1024, VRAM_LADDR, VRAM_PADDR))
			L4_KDB_Enter("Failed to get VRAM pages from sigma0");
		vmap_flag = 1;
	}
	scr->vaddr = VRAM_LADDR;
	scr->apsize = 16 * 1024 * 1024;
	return;
}

static void disable(VGAscr *scr)
{
	if (vmap_flag != 0) {
		//if (lp_unmappges(16 * 1024 * 1024, VRAM_LADDR)) ...
		// vmap_flag = 0;
	}
	return;
}

static void init_qemuvga0(int x, int y, int c, int flag)
{
	set_qemuvga0reg(0x0004, 0x0000);
	set_qemuvga0reg(0x0001, x);
	set_qemuvga0reg(0x0002, y);
	set_qemuvga0reg(0x0003, c);
	set_qemuvga0reg(0x0005, 0x0000);
	set_qemuvga0reg(0x0004, flag);
	return;
}

void init_palette_generic(void);	// in vgageneric.c
void init_mode03_generic(void);		// in vgageneric.c

static char *qemufontbackup = nil;

static void save_qemufont(void)
{
	int i;
	char *p = (char *) VRAM_LADDR;
 	for (i = 0; i < 32 * 1024; i++)
		qemufontbackup[i] = p[i];
	return;
}

static void load_qemufont(void)
{
	int i;
	char *p = (char *) VRAM_LADDR;
	for (i = 0; i < 32 * 1024; i++)
		p[i] = qemufontbackup[i];
	return;
}

void init_qemumode03(void)
{
	if (is_stdvga())
		set_qemuvga0reg(0x0004, 0x0000);  // initialize VBE reg.
	init_mode03_generic();
	return;
}

static void drawinit(VGAscr *scr)
{
	static int px = 0, py, chan;
	if (scr->gscreen == nil) {
		/* to textmode */
		if (px > 0) {
			load_qemufont();
			init_qemumode03();
			px = 0;
		}
	} else {
		/* to graphics mode */
		int new_px = scr->gscreen->clipr.max.x;	// HK 20091031, debug
		int new_py = scr->gscreen->clipr.max.y;
		int new_chan = scr->gscreen->chan;
		int old_px = px;
		if (px != new_px || py != new_py || chan != new_chan) {
			px = new_px;
			py = new_py;
			chan = new_chan;
			if (new_chan == CMAP8) {
				init_qemuvga0(new_px, new_py,  8, 0xc1);
				init_palette_generic();
			}
			if (new_chan == RGB15)
				init_qemuvga0(new_px, new_py, 15, 0xc1);
			if (new_chan == RGB16)
				init_qemuvga0(new_px, new_py, 16, 0xc1);
			if (new_chan == XRGB32)
				init_qemuvga0(new_px, new_py, 32, 0xc1);
		}
		if (old_px == 0) {
			if (qemufontbackup == nil)
				qemufontbackup = malloc(32 * 1024);
			save_qemufont();
		}
	}
	return;
}

static long drvctlread(VGAscr *scr, void *a, long n, vlong off)
{
	return 0;
}

static long drvctlwrite(VGAscr *scr, void *a, long n, vlong off)
{
	return 0;
}

// HK 20090831 end

VGAdev vgaqemustddev = {
	"qemustd",
	enable,
	disable,
	0,
	linear,
	drawinit,	// HK 20090831
	0,
	0,
	0,
	0,
	drvctlread,	// HK 20090831
	drvctlwrite,	// HK 20090831
};

