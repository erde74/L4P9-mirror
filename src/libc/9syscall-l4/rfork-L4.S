#      rfork
#     (C) HKawai
#
#    int  rfork(int flag); 
#    int syscall_iii(int syscallnr, int arg0, int arg1, int arg3) ; 
#
#	
	
	.file       "rfork-L4.S"
        .text
        .global     rfork
        .type       rfork,@function
	
rfork:
        pushfl				#  EFLAGS
        pushal				#  8 registers
	
        movl        %esp,%ebp		#

        pushl       %ebp		#  arg[2]:  SP
        pushl       $childstart		#  arg[1]:  IP
        pushl       40(%ebp)		#  arg[0]:  flag
        pushl       $19			#  RFORK = 19

	call        syscall_iii		#  syscall_iii(19, flag, IP, SP)
					#  --> sysrfork(flag, IP, SP)
	
        movl        %ebp,%esp
        cmpl        $0xbeef,%eax	#  to avoid code ellimination
        je          childstart

	movl        %eax,28(%ebp)      # eax at pushal
        popal
        popfl
        ret

		
        .global     childstart
childstart:
        popal
        popfl
        xorl        %eax,%eax
        ret


#
#		|	|
#		| RFORK	|
#		| flag	|  arg[0]: flag		
#		| cstart|  arg[1]: IP
#		| <EBP>	|  arg[2]: SP
#		| EDI	| <-- EBP			
#		| ESI	|
#		| EBP	|
#		| Temp	|
#		| EBX	|
#		| EDX	|
#		| ECX	|
#		| EAX	|  28
#		| EFLGS	|
#		| rtnPC	|
#		| flag	|  40
#		|	|

