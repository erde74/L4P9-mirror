/*********************************************************************
 *                
 * Copyright (C) 2003-2004,  Karlsruhe University
 *                
 * File path:     l4io.h
 * Description:   
 *                
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *                
 * $Id: l4io.h,v 1.3.4.2 2004/11/23 14:09:33 skoglund Exp $
 *                
 ********************************************************************/
#ifndef __L4IO_H__
#define __L4IO_H__

#include <l4/types.h>

#ifdef __cplusplus
extern "C" {
#endif

  int l4vsnprintf(char *str, L4_Size_t size, const char *fmt, char*  ap);
  int l4vprintf(const char *fmt, char* ap);
  int l4snprintf(char *str, L4_Size_t size, const char *fmt, ...);

  int l4printf(const char *fmt, ...);
  int l4printf_r(const char *fmt, ...);
  int l4printf_g(const char *fmt, ...);
  int l4printf_b(const char *fmt, ...);
  int l4printf_s(const char *fmt, ...);
  int l4printf_c(int color, const char *fmt, ...);

  void set_l4print_direct(int val); 
  void l4putc(int c);
  void l4_set_print_color(int c);
  void l4_set_print_color_default();

  char l4getc();
  char l4printgetc(const char *fmt, ...);
  void set_6845(int reg, unsigned val);  

#ifdef __cplusplus
}
#endif

#endif /* !__L4IO_H__ */
