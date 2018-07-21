.global  _increment
	.type   _increment,@function	
_increment:	/* void _increment(int*)  */
		pushl   %ebp
	        movl    %esp, %ebp

	        movl    8(%ebp), %eax
	        lock    
		incl    0(%eax)
		leave
	        ret

.global  _decrement
	 .type   _decrement,@function
	
_decrement:	/* long _decrement(int*) */
		pushl   %ebp
	        movl    %esp, %ebp
		pushl   %ebx			
	        movl    8(%ebp), %ebx
	        xorl    %eax, %eax
	        lock
		decl   0(%ebx)
	        jl     _decrementlt
	        jg     _decrementgt
		popl   %ebx			
		leave
	        ret
_decrementgt:
	        incl   %eax
		popl   %ebx			
		leave
	        ret
_decrementlt:
	        decl    %eax
		popl   %ebx			
		leave
	        ret


