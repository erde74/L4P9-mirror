/*	int _tas(int x, int y)
 *	{  
 *	   ....
 *	}
 *	
 *	|   	 |
 *	+--------+ <-------- EBP
 *	| oldFP  |
 *	+========+ 
 *	|  PC    |
 *	+--------+
 *	|   x    |  8(EBP)
 *	+--------+
 *	|        |
 */	
	
	.text

/*  
 *	int _tas(void *adrs) ATOMIC
 *	{
 *	  int  tmp; 
 *	  tmp = *addr;
 *	  *addr = 0xdeaddead;
 *	  return  tmp;
 *	}
 */
.global  _tas
	.type   _tas,@function
		
_tas:            
		pushl   %ebp
		movl    %esp, %ebp
		pushl   %ebx	
		movl    $0xdeaddead, %eax
	        movl    8(%ebp), %ebx
	        xchgl   %eax, (%ebx)        /* lock->key */
		popl    %ebx
		leave
	        ret

