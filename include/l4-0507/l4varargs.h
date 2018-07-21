/******************************************************************************
 * l4varargs.h
 ******************************************************************************/
#ifndef __L4VARARGS_H__
#define __L4VARARGS_H__

#if 1
typedef char*   va_list;

#define va_start(list, start) list =\
        (sizeof(start) < 4?\
                (char*)((int*)&(start)+1):\
                (char*)(&(start)+1))

#define va_end(list)\
        (list = (void*)0)

#define va_arg(list, mode)\
        ((sizeof(mode) == 1)?\
                ((mode*)(list += 4))[-4]:\
        (sizeof(mode) == 2)?\
                ((mode*)(list += 4))[-2]:\
                ((mode*)(list += sizeof(mode)))[-1])
#endif

#endif /* __L4VARARGS_H__ */

