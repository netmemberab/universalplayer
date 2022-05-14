	.align 2
	.def    _posdir_mix_8__7Mono8_cPUsPUcPUllUsUcUc;        .val    _posdir_mix_8__7Mono8_cPUsPUcPUllUsUcUc;        .scl    2;      .type   0155;   .endef
.globl _posdir_mix_8__7Mono8_cPUsPUcPUllUsUcUc
_posdir_mix_8__7Mono8_cPUsPUcPUllUsUcUc:
	.def    .bf;    .val    .;      .scl    101;    .line   158;    .endef
	.def    _this;  .val    8;      .scl    9;      .tag    _Mono8_c;       .size   28;     .type   030;    .endef
	.def    _Dest;  .val    12;     .scl    9;      .type   035;    .endef
	.def    _Sourc; .val    16;     .scl    9;      .type   034;    .endef
	.def    _ofs;   .val    20;     .scl    9;      .type   037;    .endef
	.def    _incr;  .val    24;     .scl    9;      .type   05;     .endef
	.def    _todo;  .val    28;     .scl    9;      .type   04;     .endef
	.def    _vol;   .val    32;     .scl    9;      .type   04;     .endef
	.def    _panpos;        .val    36;     .scl    9;      .type   04;     .endef

	; register info:
	.def    _this;  .val    2;      .scl    4;      .tag    _Mono8_c;       .size   28;     .type   030;    .endef
	.def    _Dest;  .val    1;      .scl    4;      .type   035;    .endef
	.def    _incr;  .val    7;      .scl    4;      .type   05;     .endef
	.def    _todo;  .val    3;      .scl    4;      .type   015;    .endef
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%edx		; this
	movl 12(%ebp),%ecx		; dest
	movl 24(%ebp),%edi		; incr
	movw 28(%ebp),%bx		; todo
	movzbl 32(%ebp),%eax		; vol
	sall $10,%eax
	addl 4(%edx),%eax		; voltab
	movl %eax,-4(%ebp)		; t_vol --> voltab[vol]

	movl 20(%ebp),%esi		; ofs
	movl (%esi),%edx		; first value

	; --> loop optimization, 4times unrolled
	testw %bx,%bx
	je L121			; todo==0 -> no loop, just jump to the end
	movl %ebx,%eax
	negw %ax
	andw $3,%ax
	testw %bx,%bx
	jle L127		; what does this mean for bx ???

	testw %ax,%ax
	je L123			; todo is divisible through 4
	cmpw $3,%ax
	jge L127		; ^^^^ +1
	cmpw $2,%ax
	jge L128		; ^^^^ +2

	; loop:
	movl %edx,%eax			; edx = ofs/frac
	shrl $16,%eax			; eax = ofs
	movl 16(%ebp),%esi		; esi = sample ptr
	movzbl (%eax,%esi),%eax		; eax = sample value
	movl -4(%ebp),%esi		; esi = t_vol
	movw (%esi,%eax,4),%ax		; ax  = t_vol[sample value]
	addw %ax,(%ecx)			; --> add to (*dest)
	addl $2,%ecx			; next dest
	addl %edi,%edx			; next ofs/frac
	decw %bx
L128:
	; loop:
	movl %edx,%eax
	shrl $16,%eax
	movl 16(%ebp),%esi
	movzbl (%eax,%esi),%eax
	movl -4(%ebp),%esi
	movw (%esi,%eax,4),%ax
	addw %ax,(%ecx)
	addl $2,%ecx
	addl %edi,%edx
	decw %bx

L127:	
	; loop:
	movl %edx,%eax
	shrl $16,%eax
	movl 16(%ebp),%esi
	movzbl (%eax,%esi),%eax
	movl -4(%ebp),%esi
	movw (%esi,%eax,4),%ax
	addw %ax,(%ecx)
	addl $2,%ecx
	addl %edi,%edx
	decw %bx
	je L121

L123:
	movl %edx,%eax
	shrl $16,%eax
	movl 16(%ebp),%esi
	movzbl (%eax,%esi),%eax
	movl -4(%ebp),%esi
	movw (%esi,%eax,4),%ax
	addw %ax,(%ecx)
	addl %edi,%edx

	movl %edx,%eax
	shrl $16,%eax
	movl 16(%ebp),%esi
	movzbl (%eax,%esi),%eax
	movl -4(%ebp),%esi
	movw (%esi,%eax,4),%ax
	addw %ax,2(%ecx)
	addl %edi,%edx

	movl %edx,%eax
	shrl $16,%eax
	movl 16(%ebp),%esi
	movzbl (%eax,%esi),%eax
	movl -4(%ebp),%esi
	movw (%esi,%eax,4),%ax
	addw %ax,4(%ecx)
	addl %edi,%edx

	movl %edx,%eax
	shrl $16,%eax
	movl 16(%ebp),%esi
	movzbl (%eax,%esi),%eax
	movl -4(%ebp),%esi
	movw (%esi,%eax,4),%ax
	addw %ax,6(%ecx)
	addl $8,%ecx
	addl %edi,%edx
	addw $-4,%bx
	jne L123

L121:	; after loop:
	movl 20(%ebp),%esi	
	movl %edx,(%esi)	; write *ofs
	movl %ecx,%eax		; return value dest
	leal -16(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	leave
	ret

