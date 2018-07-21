/*   
 *   x86-putc.c 
 *   (C) KM@NII
 */

#include <l4all.h>


#define  DISP_ROWS 24

#define  RBUF_ROWS   128              // 128 rows (RBUF_SIZE/160)
#define  RBUF_SIZE   (160*RBUF_ROWS)  // 80-chars * 2 * rows


typedef  struct { 
    unsigned dd[40];
} row_t;

static void zero_row(row_t *rr)
{
    int  i;
    for (i=0; i<40; i++) rr->dd[i] = 0; // 160 bytes
}


static union {
    char      aa[160*RBUF_ROWS];    // indexed by char
    unsigned  bb[40*RBUF_ROWS];     // indexed by unsigned
    row_t     cc[RBUF_ROWS];        // indexed by row
}  RINGBUF;

static int  inx = 0; 
static int  scroll = 0;


typedef union {
    char      aa[160*DISP_ROWS];
    unsigned  bb[40*DISP_ROWS];
    row_t     cc[DISP_ROWS];
} Display_t;

Display_t * DISPLAY = (Display_t*)0xB8000;

static unsigned __cursor = 0;


char __COLOR = 14;

static void put2vram(unsigned  rawx)
{
    int  i;
    for (i = 0; i < DISP_ROWS; i++) {
        DISPLAY->cc[i] = RINGBUF.cc[(rawx + i) % (RBUF_ROWS) ];
    }
}

void l4putc(char c)
{
    int  i;
    int  rawx;

    switch(c) {

    case 0xF:  // scroll-down
        if(scroll >= (RBUF_ROWS-1))
            return ;
        scroll++;
	rawx = (inx/160 + RBUF_ROWS - DISP_ROWS - scroll) % (RBUF_ROWS);
	put2vram(rawx);
	return;

    case 0xE:  // scroll-up
        if (scroll > 0){
	    scroll--;
	    rawx = (inx/160 + RBUF_ROWS - DISP_ROWS - scroll) % (RBUF_ROWS);
	    put2vram(rawx);
	}
	return;

    case '\r':
        if (scroll > 0) {
	    scroll = 0;
	    put2vram((inx/160 + RBUF_ROWS - DISP_ROWS) % RBUF_ROWS);
	}
	__cursor = (__cursor /160) * 160;  
	inx = (inx/160) * 160;            //KM--- test -----
	zero_row(&RINGBUF.cc[inx % RBUF_ROWS]);	
        break;

    case '\n':
        if (scroll > 0) {
	     scroll = 0;
	     put2vram((inx/160 + RBUF_ROWS - DISP_ROWS) % RBUF_ROWS);
	}
        __cursor += (160 - (__cursor % 160));

	{
	    int rr  = 160 - (inx % 160);
	    for (i= 0; i < rr; i++)
	        RINGBUF.aa[inx + i] = 0; // clear the line remained
	}
        inx += (160 - (inx % 160));
	zero_row(&RINGBUF.cc[inx % RBUF_ROWS]);
        break;

    case '\t':
        if (scroll > 0) {
	     scroll = 0;
	     put2vram((inx/160 + RBUF_ROWS - DISP_ROWS) % RBUF_ROWS);
	}

	for ( i = 0; i < (8 - (__cursor % 8)); i++ ){
	    DISPLAY->aa[__cursor++] = RINGBUF.aa[inx++] = ' ';
	    DISPLAY->aa[__cursor++] = RINGBUF.aa[inx++] = __COLOR;
	    if (inx >= RBUF_SIZE)
	       inx = inx % RBUF_SIZE;
	}
        break;

    case 127:
    case '\b':                          //% 08/06/26
        if ((inx % 160) != 0) {
	    __cursor -= 2;
	    inx -= 2;
	    DISPLAY->aa[__cursor+0] = RINGBUF.aa[inx+0] = ' ';
	    DISPLAY->aa[__cursor+1] = RINGBUF.aa[inx+1] = 14;
	}
	break; 


    default:
        if (scroll > 0) {
	     scroll = 0;
	     put2vram((inx/160 + RBUF_ROWS - DISP_ROWS) % RBUF_ROWS);
	}
        DISPLAY->aa[__cursor++] = RINGBUF.aa[inx++] = c;
        DISPLAY->aa[__cursor++] = RINGBUF.aa[inx++] = __COLOR;
	if ((inx%160) == 0)
	    zero_row(&RINGBUF.cc[inx % RBUF_ROWS]);
	if (inx >= RBUF_SIZE)
	    inx = inx % RBUF_SIZE;
    }

    if ((__cursor / 160) == DISP_ROWS) {
#if 1 
	for (i = 0; i < DISP_ROWS -1; i++)
	    DISPLAY->cc[i] = DISPLAY->cc[i+1];

	zero_row(&DISPLAY->cc[DISP_ROWS-1]);
#else
        unsigned * clearline; 
	for (i = 40; i < 40*DISP_ROWS; i++)
	    ((unsigned*)DISPLAY->aa)[i - 40] = ((unsigned*)DISPLAY->aa)[i];
	clearline = (unsigned*)&RINGBUF.aa[inx % RBUF_SIZE];
	for (i = 0; i < 40; i++){
	    ((unsigned*)DISPLAY->aa)[40*(DISP_ROWS-1) + i] = 0;
	    (clearline)[40*(DISP_ROWS-1) + i] = 0;
	}
#endif
	__cursor -= 160;
    }
}


void l4_set_print_color(char c)
{
  __COLOR = c;
}

void l4_set_print_color_default()
{
  __COLOR = 14;
}

void l4_reset_print_cursor( )
{
  __cursor = 0;
}
