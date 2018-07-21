/*	int foo(int x, int y)
 *	{  int  a, b;
 *	   ....
 *	}
 *	
 *	|   	 |
 *	+--------+
 *	|   b    |  -8(EBP) 
 *	+--------+
 *	|   a    |  -4(EBP)
 *	+--------+ <-------- EBP
 *	| oldFP  |
 *	+========+ 
 *	|  PC    |
 *	+--------+
 *	|   x    |  8(EBP)
 *	+--------+
 *	|   y	 |  12(EBP)
 *	+--------+
 *	|        |
 */	
	
	.text


/*  
 *	int tas(void *adrs)  
 *	{
 *	  int  tmp; 
 *	  tmp = *addr;
 *	  *addr = 0xdeaddead;
 *	  return  tmp;
 *	}
 */
.global  tas
	.type   tas,@function
		
tas:            
		pushl   %ebp
		movl    %esp, %ebp
		pushl   %ebx	
		movl    $0xdeaddead, %eax
	        movl    8(%ebp), %ebx
	        xchgl   %eax, (%ebx)        /* lock->key */
		popl    %ebx
		leave
	        ret

.global  _xinc
	.type   _xinc,@function	
_xinc:	        /* void inc(int*)  */
		pushl   %ebp
	        movl    %esp, %ebp

	        movl    8(%ebp), %eax
	        lock    
		incl    0(%eax)
		leave
	        ret

.global  _xdec
	 .type   _xdec,@function
	
_xdec:		/* long _xdec(int*) */
		pushl   %ebp
	        movl    %esp, %ebp
		pushl   %ebx			
	        movl    8(%ebp), %ebx
	        xorl    %eax, %eax
	        lock
		decl   0(%ebx)
	        jl     _xdeclt
	        jg     _xdecgt
		popl   %ebx			
		leave
	        ret
_xdecgt:
	        incl   %eax
		popl   %ebx			
		leave
	        ret
_xdeclt:
	        decl    %eax
		popl   %ebx			
		leave
	        ret

.global  xchgw
	.type   xchgw,@function
	
xchgw:		/*  int   xchgw(ushort*, int) ; */
	        pushl   %ebp
	        movl    %esp, %ebp
		pushl   %ebx			
	        movl    12(%ebp), %eax
	        movl    8(%ebp), %ebx
	        xchgw   %ax, (%ebx)
		popl   %ebx			
		leave
	        ret


/*	
 *  int  cmpswap486(long* addr, long old, long new)  
 *  {	
 *	if (*addr == old){
 *	    *addr = new ;
 *           return 1;
 *	}
 *	else
 *	    retrun 0;
 *   }
 */

.global  cmpswap486
	.type   compswap,@function	
cmpswap486:	
	        pushl   %ebp
	        movl    %esp, %ebp
		pushl   %ebx		
		pushl   %ecx				
	        movl    8(%ebp), %ebx  // addr
	        movl    12(%ebp), %eax  // old
	        movl    16(%ebp), %ecx  // new
	        lock
	        cmpxchgl  %ecx, (%ebx) 
	        jnz     didnt
	        movl    $1, %eax
		popl    %ecx
		popl    %ebx			
		leave
	        ret
didnt:
	        xorl    %eax, %eax
		popl    %ecx
		popl    %ebx			
		leave
	        ret

	

#if 1 //--------------------------------------
/*  int splhi(void); */
.global    splhi
	.type   splhi,@function	

splhi:
		pushl  %ebp
		movl   %esp, %ebp
	shi:	
	        pushfl
	        popl    %eax
	        testl   $0x200, %eax
	        jz      alreadyhi
	        // MOVL    $(MACHADDR+0x04), CX            /* save PC in m->splpc */
	        // MOVL    (SP), BX
	        // MOVL    BX, (CX)
	alreadyhi:	
	        cli
		leave
	        ret

/* int spllo(void); */
.global    spllo
	.type   spllo,@function	
spllo:
	        pushl  %ebp
	        movl   %esp, %ebp
	
	slo:	
	        pushfl
	        popl    %eax
	        testl   $0x200, %eax
	        jnz      alreadylo
	        // MOVL    $(MACHADDR+0x04), CX            /* clear m->splpc */
	        // MOVL    $0, (CX)
	alreadylo:	
	        sti
		leave
	        ret
	
/* void splx(int)  */
.global    splx
	.type   splx,@function	
	
splx:
	        pushl   %ebp
		movl    %esp, %ebp
	
	        movl    8(%ebp), %eax
	        testl   $0x200, %eax
	        jnz     slo
	        jmp     shi

	
/* int  islo(void);*/
.global    islo
	.type   islo,@function	
islo:
	        pushl   %ebp
	        movl    %esp, %ebp
	
	        pushfl
	        popl    %eax
	        andl    $0x200, %eax       /* interrupt enable flag */
		leave
	        ret

#endif //-------------------------------------

