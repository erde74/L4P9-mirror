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

// HK 20090930 begin

#define BIOS_LADDR	0xB4000000
#define VRAM_PADDR	0x000a0000
#define VRAM_LADDR	(BIOS_LADDR + VRAM_PADDR)

static void outw(int port, int value)
{
	unsigned short data = (unsigned short) value;
	__asm__ __volatile__("outw %0,%%dx" : : "a"(data), "d"(port));
	return;
}

static void enable(VGAscr *scr)
{
	scr->vaddr = VRAM_LADDR;
	scr->apsize = 800 * 600 * 8 / 8;
	return;
}

static void disable(VGAscr *scr)
{
	return;
}

void init_palette_generic()
{
    static unsigned char rgb_tbl[16][3] = {
      {0x00, 0x00, 0x00},
      {0xff, 0x00, 0x00},
      {0x00, 0xff, 0x00},
      {0xff, 0xff, 0x00},
      {0x00, 0x00, 0xff},
      {0xff, 0x00, 0xff},
      {0x00, 0xff, 0xff},
      {0xff, 0xff, 0xff},
      {0xc6, 0xc6, 0xc6},
      {0x84, 0x00, 0x00},
      {0x00, 0x84, 0x00},
      {0x84, 0x84, 0x00},
      {0x00, 0x00, 0x84},
      {0x84, 0x00, 0x84},
      {0x00, 0x84, 0x84},
      {0x84, 0x84, 0x84}};
    int  i;
    // TBD: disable interrupt 
    outb(0x03c8, 0);
    for (i = 0; i < 16; i++) {
        outb(0x03c9, rgb_tbl[i][0] /4);
	 outb(0x03c9, rgb_tbl[i][1] /4);
	 outb(0x03c9, rgb_tbl[i][2] /4);
    }
    // TBD: 
    return;
}  

static char *genefontbackup = nil;

static void save_genefont(void)
{
	int i;
	char *p = (char *) BIOS_LADDR + 0xa0000;
 	for (i = 0; i < 32 * 1024; i++)
		genefontbackup[i] = p[i];
	return;
}

static void load_genefont(void)
{
	int i;
	char *p = (char *) BIOS_LADDR + 0xa0000;
	for (i = 0; i < 32 * 1024; i++)
		p[i] = genefontbackup[i];
	return;
}

void init_mode03_generic(void)
{
    unsigned char  c, d;
    int i, j;

    outb(0x03c6, 0xff);  // Pixcel mask register.

    // Set standard pallet
    // TBD: disable interrupt
    outb(0x03c8, 0);
    for (i = 0; i < 64; i++) {
        for (j = 0; j < 3; j++) {
	    c = 0x00;
	    d = i >> (2 - j);
	    if (d & 1)
	      c = 0xa8 / 4;
	    if (d & 8)
	      c += 0x54 / 4;
	    outb(0x03c9, c);
	}
    }
    for (i = 64 * 3; i < 256 * 3; i++)
      outb(0x03c9, c);
    // TBD:

    outw(0x03c4, 0x0100); // start seqense reset
    outb(0x03c2, 0x67);   // VGA controller
    outb(0x03c3, 0x01);   // VGA enable register

    // set sequenser 
    // TBD: disable interrupt
    for (i = 1; i < 0x05; i++) {
        static unsigned char dat[0x05] = {
	  0x03, 0x00, 0x03, 0x00, 0x02
	};
	outw(0x03c4, dat[i] << 8 | i);
    }
    outw(0x03c4, 0x0300);  // end seqenser reset
    // TBD:

    // CRT controller
    // TBD: disable interrupt
    outw(0x03d4, 0x2011);
    for (i = 0; i < 0x19; i++) {
      static unsigned char dat[0x19] = {
	0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f,
	0x00, 0x4f, 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00,
	0x9c, 0x8e, 0x8f, 0x28, 0x1f, 0x96, 0xb9, 0xa3,
	0xff
      };
      outw(0x03d4, dat[i] << 8 | i);
    }
    // TBD:

    // TBD: disable interrupt
    for (i = 0; i < 0x09; i++) {
        static unsigned char dat[0x09] = {
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0e, 0x0f, 0xff
	};
	outw(0x03ce, dat[i] << 8 | i);
    }
    // TBD:

    // TBD: disable interrupt
    inb(0x03da);
    for (i = 0; i < 0x15; i++) {
      static unsigned char dat[0x15] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x0c, 0x00, 0x0f, 0x08, 0x00
      };
      outb(0x03c0, i);
      outb(0x03c0, dat[i]);
    }
    inb(0x03da);
    outb(0x03c0, 0x20);
    // TBD:

    for (i = 0; i < 80 * 25 / 2; i++)
      ((unsigned int *) (BIOS_LADDR + 0xb8000))[i] = 0x07200720;

  return;
}

static void init_mode12(void)
// 640x480x4
{
    unsigned char  c, d;
    int i, j;

    outb(0x03c6, 0xff);  // Pixcel mask register.

    // Set standard pallet
    // TBD: disable interrupt
    outb(0x03c8, 0);
    for (i = 0; i < 64; i++) {
        for (j = 0; j < 3; j++) {
	    c = 0x00;
	    d = i >> (2 - j);
	    if (d & 1)
	      c = 0xa8 / 4;
	    if (d & 8)
	      c += 0x54 / 4;
	    outb(0x03c9, c);
	}
    }
    for (i = 64 * 3; i < 256 * 3; i++)
      outb(0x03c9, c);
    // TBD:

    outw(0x03c4, 0x0100); // start seqense reset
    outb(0x03c2, 0xe3);   // VGA controller
    outb(0x03c3, 0x01);   // VGA enable register

    // set sequenser 
    // TBD: disable interrupt
    for (i = 1; i < 0x05; i++) {
        static unsigned char dat[0x05] = {
	  0x03, 0x01, 0x0f, 0x00, 0x06
	};
	outw(0x03c4, dat[i] << 8 | i);
    }
    outw(0x03c4, 0x0300);  // end seqenser reset
    // TBD:

    // CRT controller
    // TBD: disable interrupt
    outw(0x03d4, 0x2011);
    for (i = 0; i < 0x19; i++) {
      static unsigned char dat[0x19] = {
	0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0x0b, 0x3e,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xea, 0x8c, 0xdf, 0x28, 0x00, 0xe7, 0x04, 0xe3,
	0xff
      };
      outw(0x03d4, dat[i] << 8 | i);
    }
    // TBD:

    // TBD: disable interrupt
    for (i = 0; i < 0x09; i++) {
      static unsigned char dat[0x09] = {
	0x00, 0x0f, 0x00, 0x00, 0x00, 0x03, 0x05, 0x00, 0xff
       };
	outw(0x03ce, dat[i] << 8 | i);
    }
    // TBD:

    // TBD: disable interrupt
    inb(0x03da);
    for (i = 0; i < 0x15; i++) {
      static unsigned char dat[0x15] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x01, 0x00, 0x0f, 0x00, 0x00
      };
      outb(0x03c0, i);
      outb(0x03c0, dat[i]);
    }
    inb(0x03da);
    outb(0x03c0, 0x20);
    // TBD:

  return;
}

static void init_mode13(void)
// 320x200x8
{
    unsigned char  c, d;
    int i, j;

    outb(0x03c6, 0xff);  // Pixcel mask register.

    // Set standard pallet
    // TBD: disable interrupt
    outb(0x03c8, 0);
    for (i = 0; i < 64; i++) {
        for (j = 0; j < 3; j++) {
	    c = 0x00;
	    d = i >> (2 - j);
	    if (d & 1)
	      c = 0xa8 / 4;
	    if (d & 8)
	      c += 0x54 / 4;
	    outb(0x03c9, c);
	}
    }
    for (i = 64 * 3; i < 256 * 3; i++)
      outb(0x03c9, c);
    // TBD:

    outw(0x03c4, 0x0100); // start seqense reset
    outb(0x03c2, 0xe3);   // VGA controller
    outb(0x03c3, 0x01);   // VGA enable register

    // set sequenser 
    // TBD: disable interrupt
    for (i = 1; i < 0x05; i++) {
        static unsigned char dat[0x05] = {
	  0x03, 0x01, 0x0f, 0x00, 0x0e
	};
	outw(0x03c4, dat[i] << 8 | i);
    }
    outw(0x03c4, 0x0300);  // end seqenser reset
    // TBD:

    // CRT controller
    // TBD: disable interrupt
    outw(0x03d4, 0x2011);
    for (i = 0; i < 0x19; i++) {
      static unsigned char dat[0x19] = {
	0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f,
	0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x9c, 0x8e, 0x8f, 0x28, 0x40, 0x96, 0xb9, 0xa3,
	0xff
      };
      outw(0x03d4, dat[i] << 8 | i);
    }
    // TBD:

    // TBD: disable interrupt
    for (i = 0; i < 0x09; i++) {
      static unsigned char dat[0x09] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0f, 0xff
       };
	outw(0x03ce, dat[i] << 8 | i);
    }
    // TBD:

    // TBD: disable interrupt
    inb(0x03da);
    for (i = 0; i < 0x15; i++) {
      static unsigned char dat[0x15] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x00, 0x12, 0x00, 0x04
      };
      outb(0x03c0, i);
      outb(0x03c0, dat[i]);
    }
    inb(0x03da);
    outb(0x03c0, 0x20);
    // TBD:

  return;
}

static void *virtual_vram = nil;

static void linear(VGAscr *scr, int size, int align)
{
	if (scr->gscreen->clipr.max.x <= 320) {
		scr->vaddr = scr->gscreen->data->bdata = VRAM_LADDR;
		scr->apsize = 64 * 1024;
		vgalinearaddr(scr, VRAM_LADDR, 64 * 1024);
	} else {
		if (virtual_vram == nil) {
			virtual_vram = malloc(800 * 600 * 8 / 8);
			if (virtual_vram == nil)
				panic("vgageneric.c: malloc virtual_vram error");
		}
		scr->vaddr = scr->gscreen->data->bdata = virtual_vram;
		scr->apsize = 800 * 600 * 8 / 8;
		vgalinearaddr(scr, virtual_vram, 800 * 600 * 8 / 8);
	}
	return;
}

static void flush(VGAscr *scr, Rectangle r)
{
	if (scr->gscreen->clipr.max.x > 320) {
		int x, y, ppl = scr->gscreen->clipr.max.x, l;
		unsigned char dmy;
		volatile unsigned char *p = VRAM_LADDR, *v = virtual_vram;
		outw(0x03ce, 0x0305);	// write-mode=3
		for (y = r.min.y; y < r.max.y; y++) {
			l = y * ppl + r.min.x;
			for (x = r.min.x; x < r.max.x; x++, l++) {
				outw(0x03ce, v[l] << 8);
				dmy = p[l >> 3];
				p[l >> 3] = 0x80 >> (l & 7);
			}
		}
	}
	return;
}

static void drawinit(VGAscr *scr)
{
	static int px = 0, py, chan;
	if (scr->gscreen == nil) {
		/* to textmode */
		if (px > 0) {
			init_mode13();
			load_genefont();
			init_mode03_generic();
			px = 0;
		}
	} else {
		/* to graphics mode */
		int new_px = scr->gscreen->clipr.max.x;	// HK 20091031, debug
		int new_py = scr->gscreen->clipr.max.y;
		int new_chan = scr->gscreen->chan;
		int old_px = px;
		if (old_px == 0 && new_px > 0) {
			if (genefontbackup == nil)
				genefontbackup = malloc(32 * 1024);
			init_mode13();
			save_genefont();
		}
		if (px != new_px || py != new_py || chan != new_chan) {
			px = new_px;
			py = new_py;
			chan = new_chan;
			if (new_chan == CMAP8 && new_px == 640 && new_py == 480) {
				init_mode12();
				init_palette_generic();
			}
			if (new_chan == CMAP8 && new_px == 320 && new_py == 200) {
				init_mode13();
				init_palette_generic();
			}
		}
		flush(scr, scr->gscreen->clipr);
	}
	return;
}

#if 0

static void drawinit(VGAscr *scr)
{
	if (scr->gscreen == nil)
		init_mode03_generic();	// to textmode
	else
		flush(scr, scr->gscreen->clipr);
	return;
}

#endif

static long drvctlread(VGAscr *scr, void *a, long n, vlong off)
{
	return 0;
}

static long drvctlwrite(VGAscr *scr, void *a, long n, vlong off)
{
	return 0;
}

// HK 20090831 end

VGAdev vgageneric = {
	"vga",
	enable,
	disable,
	0,
	linear,
	drawinit,	// HK 20090831
	0,
	0,
	0,
	flush,		// HK 20090930
	drvctlread,	// HK 20090831
	drvctlwrite,	// HK 20090831
};

