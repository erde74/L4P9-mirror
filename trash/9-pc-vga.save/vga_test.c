/*======================================================*
 *             -- QEMU_VGA_TEST --			*
 *======================================================*/

#include  <l4all.h>
#include  <l4/l4io.h>
#include  <lp49/lp49.h>

#define _DBGFLG 1
#define DBGPRN  if(_DBGFLG)print


void Mandelbrot (uchar *vram);

// sigma0:[VRAM_PADDR ... ] is mapped to hvm:[VRAM_LADDR ... ].
#define VRAM_PADDR  0xE0000000
#define VRAM_LADDR  0xB0000000

static L4_ThreadId_t  sigma0;


static int get_pages_from_sigma0(L4_Word_t sigma0_adrs, L4_Word_t map_to, int log2pages)
{
    L4_Fpage_t  request_fp, result_fp, recv_fp;
    L4_MsgTag_t tag;
    L4_Msg_t msg;

    sigma0.raw = 0x000C0001;
    request_fp = L4_FpageLog2(sigma0_adrs, 12+log2pages);
    recv_fp = L4_FpageLog2(map_to, 12+log2pages);

    L4_Accept(L4_MapGrantItems (recv_fp));
    L4_MsgClear(&msg);
    L4_MsgAppendWord(&msg, request_fp.raw); //MR1
    L4_MsgAppendWord(&msg, 0);              //MR2 attribute
    L4_Set_MsgLabel(&msg, (L4_Word_t) -6UL << 4); //MR0-label
    L4_MsgLoad(&msg);

    tag = L4_Call(sigma0);
    if (L4_IpcFailed (tag)) {
        DBGPRN("< get_page_from_sigma0: err tag=%x\n", tag.raw);
	return 0;
    }
    L4_MsgStore(tag, &msg);

    result_fp.raw = L4_MsgWord(&msg, 1); // L4_StoreMR(2, &result_fp.raw);

    if( L4_IsNilFpage(result_fp) )
        return 0;
    else return 1;
}


static void vram_map()
{
    int  rc;
    rc = get_pages_from_sigma0(VRAM_PADDR, VRAM_LADDR, 8);
    if (rc == 0) L4_KDB_Enter("Failed to get VRAM pages from sigma0");
}


static void outb(int port, int value)
{
    unsigned char data = (unsigned char)value;
    __asm__ __volatile__("outb %0,%%dx" : : "a" (data), "d" (port));
}

static void outw(int port, int value)
{
  unsigned short data = (unsigned short)value;
  __asm__ __volatile__("outw %0,%%dx" : : "a" (data), "d" (port));
}

static int inb(int port)
{
  unsigned char data;
  __asm__ __volatile__("inb %%dx,%0" : "=a" (data) : "d" (port));
  return (int)data;
}

static int inw(int port)
{
  unsigned int data;
  __asm__ __volatile__("inw %%dx,%0" : "=a" (data) : "d" (port));
  return (int)data;
}



static void set_qemuvga0reg(int reg, int data)
{
    outw(0x01ce, reg);
    outw(0x01cf, data);
}


typedef struct VGAinfo {
    char   cyls, leds, vmode, reserve;
    short  scrnx, scrny;
    char   *vram;
} VGAinfo;
static VGAinfo  vgainfo;

static void init_qemuvga0(VGAinfo *vinfo,
		   int x, int y, int c, int flag)
{
    set_qemuvga0reg(0x0004, 0x0000);
    set_qemuvga0reg(0x0001, x);
    set_qemuvga0reg(0x0002, y);
    set_qemuvga0reg(0x0003, c);
    set_qemuvga0reg(0x0005, 0x0000);
    set_qemuvga0reg(0x0004, flag);

    vinfo->scrnx = x;
    vinfo->scrny = y;
    vinfo->vmode = c;
    if ((flag & 0x40) == 0) {
        vinfo->vram = (char*) 0x000a0000;
    }
    else {
        vinfo->vram = (char*) VRAM_LADDR;
    }
}

static void init_palette()
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
        outb(0x03c9,  rgb_tbl[i][0]/4);
	outb(0x03c9,  rgb_tbl[i][1]/4);
	outb(0x03c9,  rgb_tbl[i][2]/4);
    }
    // TBD: 
}  
   

//---081014 KM --------------------------------------
static int qemufontbackup[32 * 1024 / 4];

int is_stdvga()
{
    outw(0x01ce, 0x0000);
    outw(0x01cf, 0xb0c2);
    outw(0x01ce, 0x0000);
    return  (inw(0x01cf) == 0xb0c2);
}

static void save_qemufont(void)
{
    int i, *p = (int *) VRAM_LADDR;
    for (i = 0; i < 32 * 1024 / 4; i++)
        qemufontbackup[i] = p[i];
}

static void load_qemufont(void)
{
    int i, *p = (int *) VRAM_LADDR;
    for (i = 0; i < 32 * 1024 / 4; i++)
        p[i] = qemufontbackup[i];
}


void init_qemumode03(void)
{
    unsigned char  c, d;
    int i, j;

    set_qemuvga0reg(0x0004, 0x0000);  // initialize VBE reg.
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
    outw(0x03d4, 0x2011);  // 00-07のライトプロテクト解除
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

    /* VGAグラフィックコントローラの設定 */
    // TBD: disable interrupt
    for (i = 0; i < 0x09; i++) {
        static unsigned char dat[0x09] = {
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0e, 0x0f, 0xff
	};
	outw(0x03ce, dat[i] << 8 | i);
    }
    // TBD:

    /* アトリビュートコントローラの設定 */
    /* アトリビュートコントローラの設定 */
    // TBD: disable interrupt
    inb(0x03da);  /* 空読み */
    for (i = 0; i < 0x15; i++) {
      static unsigned char dat[0x15] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x0c, 0x00, 0x0f, 0x08, 0x00
      };
      outb(0x03c0, i);
      outb(0x03c0, dat[i]);
    }
    inb(0x03da);  /* 空読み */
    outb(0x03c0, 0x20);
    // TBD:

    /* テキストVRAMクリア */
    for (i = 0; i < 80 * 25 / 2; i++)
      ((unsigned int *) 0xb8000)[i] = 0x07200720;

  return;
}
//----------------------------------------------------


static void boxfill(char *vram, int xsize, unsigned char c,
	     int x0, int y0, int dx1, int dy1)
{
    int  x, y;
    for (y = y0; y <= y0 + dy1; y++)
        for (x = x0; x <=x0 + dx1; x++) 
	    vram[y * xsize + x] = c;
}

static void  drwline(char *vram, int x0, int y0, int x1, int y1, int color)
{
    int  i, x, y, len, dx, dy;
    dx = x1 - x0;
    dy = y1 - y0;
    x = x0 << 10;
    y = y0 << 10;

    if (dx < 0)  dx = -dx;
    if (dy < 0)  dy = -dy;

    if (dx >= dy){
        len = dx + 1;
	if (x0 > x1) dx = -1024;
	else	     dx = 1024;

	if (y0 <= y1) dy = ((y1 - y0 + 1) << 10)/len;
	else 	      dy = ((y1 - y0 -1 ) << 10)/len;
    }else{
        len = dy + 1;
	if (y0 > y1) dy = -1024;
	else 	     dy = 1024;

	if (x0 <= x1) dx = ((x1 - x0 + 1) << 10)/len;
	else          dx = ((x1 - x0 -1) << 10)/len;
    }

    for (i = 0; i<len; i++){
      vram[(y >> 10) * 800 + (x >> 10)] = color;
      x += dx;
      y += dy;
    }
}


void vga_test( )
{
    char *p;
    VGAinfo *vinfo = &vgainfo;
    int   i;

    vram_map();

    init_palette();
    init_qemuvga0(vinfo, 800, 600, 8, 0xc1);
    save_qemufont(); //??

    p = (char*) vinfo->vram;

    for (i = 0; i<1024; i++)
        drwline(p, 700-i, i, 600-i, i-50, (i>>2)%16);

    for (i = 0; i<128; i++) {
        L4_Sleep(L4_TimePeriod(5000));  
	boxfill(p, 800, i % 16, (20+i*25), (20+i*5), 10+i, 10+i);
    }

    Mandelbrot(p);
    L4_Sleep(L4_TimePeriod(1000000));

    load_qemufont();  //?
    init_qemumode03();  //?

    //    L4_KDB_Enter("<VGA_TEST");
}


/*======================================================*
 *             -- Mandelbrot --				*
 *          (C) maruyama@nii.ac.jp                      *
 *======================================================*/
#define  width  800    
#define  height 600    

static  double  scale = 2.0;
static  double  kReal0 =  -1.4;
static  double  kImage0 = 1.0;
static  int     iteration = 200;

struct {
    double   kReal0;
    double   kImage0;
    double   scale;
} idata[] =
{ { -18.0, 10.0, 20.0},
  { -14.0, 8.0,  16.0},
  { -12.0, 7.0,  14.0},
  { -10.0, 6.0,  12.0},
  { -8.0,  5.0,  10.0},
  { -6.0,  4.0,  8.0},
  { -4.0,  3.0,  6.0},
  { -3.0,  3.0,  5.0},
  { -2.0,  2.0,  4.0},
  { -1.4,  1.0,  2.0}
};

void Mandelbrot (uchar *vram) 
{
    int   cycle = 0;
    double  kReal, kImage, zReal, zImage;
    double  zReal2, zImage2, zRealNew, zImageNew;
    int i, x, y;
    uchar  color;

    // repeat:  z := z*z + k; (where z=x+y*i and k=a+b*i)
    for(i=0; i<10; i++) {
        kReal0 = idata[i].kReal0;
        kImage0 = idata[i].kImage0;
        scale = idata[i].scale;

        for (y = 0; y < height; y++) {
	    kImage = kImage0 - (double)y/(double)height * scale; 

	    for (x = 0;  x < width; x++) {
	        kReal = kReal0 + (double)x/(double)width * scale;
		zReal = zImage = 0;

		for (cycle = 0;  cycle < iteration; cycle++ ) {
		    zReal2  = zReal * zReal;
		    zImage2 = zImage * zImage;
		    if ((zReal2 + zImage2) >= 4.0)
		      break;
		    zRealNew = zReal2 - zImage2 + kReal;
		    zImageNew = 2 * zReal * zImage + kImage;
		    zReal = zRealNew;
		    zImage = zImageNew;
		}
		if (cycle == iteration)  color = 0;    
		else    color = (uchar)(cycle % 16);

		vram[y*width + x] = color;
	    }
	}	
    }
}
