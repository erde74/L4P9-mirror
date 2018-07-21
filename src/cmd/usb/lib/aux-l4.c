

   typedef struct Ref  Ref;
   struct Ref {
      long ref;
   };
 

void incref(Ref *r)
{
  __asm__ __volatile__
         ("    lock            \n"
          "    incl  0(%%eax) \n"
          :
	  : "a"(r)
	  );
}

long decref(Ref *r)
{
  long  rval;

  __asm__ __volatile__
         ("    lock            \n"
	  "    decl  0(%%eax) \n"
	  "    jz    1f        \n"
	  "    movl  $1, %%eax \n"
	  "    jmp   2f        \n"
	  "1:                  \n"
	  "    movl  $0, %%eax \n"
	  "2:                  \n"

          :  "=a" (rval)
	  :  "a" (r)
	  );
  return  rval;
}


