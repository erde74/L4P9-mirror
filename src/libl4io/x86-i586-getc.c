/*
 *  Imported from: L4  kernel/kdb/platform/pc99/io.cc
 *  Busy waiting  -- shoule not be used except for debugging.
 */

#include <l4all.h>
#include <l4/l4io.h>
//#include <l4/l4varargs.h>

#include  <lp49/lp49.h>


#define NULL (void*)0
#define INLINE static inline
typedef unsigned char  byte_t;

#if 1 //-----------------------------------------
static unsigned char _inb(int port)
{
  unsigned char data;
  __asm__ __volatile__("inb %%dx,%0" : "=a" (data) : "d" (port));
  return data;
}

#else //-----------------------------------------
#if 1 //KM---------
INLINE unsigned char _inb(unsigned short port)  //KM
#else // ORIG
INLINE unsigned char _inb(int  port)
#endif //----------
{
    unsigned char tmp;
    __asm__ __volatile__("inb %1, %0\n"
                         : "=al"(tmp)
                         : "dN"(port));
    return tmp;
};
#endif //-------------------------------------

#define KBD_STATUS_REG		0x64	
#define KBD_CNTL_REG		0x64	
#define KBD_DATA_REG		0x60	

#define KBD_STAT_OBF 		0x01	/* Keyboard output buffer full */

#define kbd_read_input()  _inb(KBD_DATA_REG)
#define kbd_read_status() _inb(KBD_STATUS_REG)


static unsigned char keyb_layout[2][128] =
{
  "\000\0331234567890-=\177\t"                    /* 0x00 - 0x0f */
  "qwertyuiop[]\r\000as"                          /* 0x10 - 0x1f */
  "dfghjkl;'`\000\\zxcv"                          /* 0x20 - 0x2f */
  "bnm,./\000*\000 \000\201\202\203\204\205"      /* 0x30 - 0x3f */
  "\206\207\210\211\212\000\000789-456+1"         /* 0x40 - 0x4f */
  "230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
  "\r\000/"                                       /* 0x60 - 0x6f */
  ,
#if 1 //080106   \177 -> | -----------------------
  "\000\033!@#$%^&*()_+|\t"                    /* 0x00 - 0x0f */
#else //Original -------------------------
  "\000\033!@#$%^&*()_+\177\t"                    /* 0x00 - 0x0f */
#endif //---------------------------------
  "QWERTYUIOP{}\r\000AS"                          /* 0x10 - 0x1f */
  "DFGHJKL:\"`\000\\ZXCV"                         /* 0x20 - 0x2f */
  "BNM<>?\000*\000 \000\201\202\203\204\205"      /* 0x30 - 0x3f */
  "\206\207\210\211\212\000\000789-456+1"         /* 0x40 - 0x4f */
  "230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
  "\r\000/"                                       /* 0x60 - 0x6f */
};



char l4getc()
{
  static byte_t last_key = 0;
  static byte_t shift = 0;
  char c;

  while(1) {
    unsigned char status; 

    status = kbd_read_status();

    while (status & KBD_STAT_OBF) {
      byte_t scancode;
      scancode = kbd_read_input();
      /* check for SHIFT-keys */
      if (((scancode & 0x7F) == 42) || ((scancode & 0x7F) == 54))
        {
          shift = !(scancode & 0x80);
          continue;
        }
      /* ignore all other RELEASED-codes */
      if (scancode & 0x80)
        last_key = 0;
      else if (last_key != scancode)
        {
          //printf("kbd: %d, %d, %c\n", scancode, last_key, keyb_layout[shift][scancode]);
          last_key = scancode;
          c = keyb_layout[shift][scancode];
          if (c > 0) return c;
        }
    }


// #if "src/9/pc/kdb.c is not used" cf.  src/9/pc/kbd.c:kbdenable() 
//    L4_Sleep(L4_TimePeriod(20UL));
// #endif 
  }
}

