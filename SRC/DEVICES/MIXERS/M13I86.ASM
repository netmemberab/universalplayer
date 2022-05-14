;****************************************************************************
;   Mixer13 - fast ix86 assembler code
; --------------------------------------------------------------------------
;  (c) copyright 1996 by A.Baresel
;
; compile it with TASM /ml /l and convert it with o2c (obj2coff)
; then link it.
;****************************************************************************/
; neg dir - not tested

PENTIUM = 1
PROFILE = 1

p386
ideal
model flat,C

DATASEG	; --------------------------------------------------------------------
	public  volumetab

	public	mixclocks
	public	mixloops

	public	posttab

volumetab	dw	65*256 dup (?)
posttab		db	2*4096 dup (?)
mixstackptr     dd	?
mixparameters	dd	?

mixclocks	dd	0
mixloops	dd	0
cyclecount	dd	0

align 4
lvol		db    000003fh
rvol		db    000003fh
__sav_ebp	dd    ?

__stepfrac	dd    ?
__stepint	dd    ?

CODESEG ; --------------------------------------------------------------------
	public	_I86_mix13_m8
	public	_I86_mix13_s8
	public	_I86_amp13_8
	public	_I86_clrbuf13
	extrn	showregs_Cpp:proc

MACRO	showregs
	mov	[__sav_ebp],ebp
	mov	ebp,[mixparameters]
	pushad

	push	ss
	push	gs
	push	fs
	push	es
	push	ds
	push	cs
	push	esp
	push	[__sav_ebp]
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	call	showregs_Cpp
	sub	esp,8*4+6*2
	popad
	mov	ebp,[__sav_ebp]
ENDM

MACRO   PREPARE_CYCLE_READ
IF PENTIUM AND PROFILE
	cli
	db  	0fh,31h
	mov 	[cyclecount],eax
	;db  	0d6h
ENDIF
ENDM

MACRO   READ_CYCLES
IF PENTIUM AND PROFILE
	db 	0fh,31h
	sti
	sub 	eax,[cyclecount]
	sub 	eax,16d
	add 	[mixclocks],eax
ENDIF
ENDM


; 13BIT-mix: MONO-8BIT-mixing -------------------------------------------------
proc	_I86_mix13_m8
	arg	Destination:DWORD,Source:DWORD,ofs:DWORD,frac:DWORD,\
		direction:DWORD,step_int:DWORD,step_frac:DWORD,\
		todo:DWORD,vol:DWORD,panpos:DWORD

        pushad
	mov	[mixparameters],ebp

        ; 'local volume'
        mov     ebx,[vol]
	shl	ebx,8

        mov     edi,[Destination]

        mov     esi,[Source]
        mov     eax,[ofs]
        add     esi,[eax]

        mov     eax,[frac]
        mov     edx,[eax]
        mov     ecx,[todo]

        mov	eax,[step_int]
	mov	[__stepint],eax
        mov	eax,[step_frac]
	mov	[__stepfrac],eax

	cmp	[direction],1
	je	M8_negdir

        mov     ebp,[__stepfrac]

; positiv direction:
;---------------------------------------------------------------------------
;inner loop unrolled 4 times...
;---------------------------------------------------------------------------

	mov	eax,ecx
	shr	ecx,2
	and	eax,3
	jz	M8P_premixloop
	test	eax,2
	je	M8P_extra1
	test	eax,1
	je	M8P_extra2

M8P_extra3:
	add     edx,ebp
	mov     bl,[esi]
	adc     esi,[__stepint]
	mov     ax,[volumetab+ebx+ebx]
	add     [edi],ax
	add	edi,2
M8P_extra2:
	add     edx,ebp
	mov     bl,[esi]
	adc     esi,[__stepint]
	mov     ax,[volumetab+ebx+ebx]
	add     [edi],ax
	add	edi,2
M8P_extra1:
	add     edx,ebp
	mov     bl,[esi]
	adc     esi,[__stepint]
	mov     ax,[volumetab+ebx+ebx]
	add     [edi],ax
	add	edi,2

M8P_premixloop:
	cmp	ecx,0
	je	M8_endmix

	mov 	eax,ecx
	shl	eax,2
	add 	[mixloops],eax
PREPARE_CYCLE_READ

	mov	eax,ecx
	shl	eax,16
	mov	ecx,[__stepint]

MACRO   innerl  nr
	add     edx,ebp
	mov     bl,[esi]
	mov     ax,[volumetab+ebx+ebx]
	adc     esi,ecx
	add     [edi+nr],ax
ENDM

align 4
M8P_mixloop:
	innerl  0
	innerl  2
	innerl  4
	innerl  6
	add	edi,8
	sub     eax,010000h
	jnc     M8P_mixloop

	READ_CYCLES

	jmp	M8_endmix


; negativ direction:
;---------------------------------------------------------------------------
;inner loop unrolled 4 times...
;---------------------------------------------------------------------------
M8_negdir:

        mov     ebp,[__stepfrac]

	mov	eax,ecx
	shr	ecx,2
	and	eax,3
	jz	M8N_premixloop
	test	eax,2
	je	M8N_extra1
	test	eax,1
	je	M8N_extra2

M8N_extra3:
	sub     edx,ebp
	mov     bl,[esi]
	sbb     esi,[__stepint]
	mov     ax,[volumetab+ebx+ebx]
	add     [edi],ax
	add	edi,2
M8N_extra2:
	sub     edx,ebp
	mov     bl,[esi]
	sbb     esi,[__stepint]
	mov     ax,[volumetab+ebx+ebx]
	add     [edi],ax
	add	edi,2
M8N_extra1:
	sub     edx,ebp
	mov     bl,[esi]
	sbb     esi,[__stepint]
	mov     ax,[volumetab+ebx+ebx]
	add     [edi],ax
	add	edi,2

M8N_premixloop:
	cmp	ecx,0
	je	M8_endmix

	mov 	eax,ecx
	shl	eax,2
	add 	[mixloops],eax
	PREPARE_CYCLE_READ

	mov	eax,ecx
	shl	eax,16
	mov	ecx,[__stepint]

MACRO   inner_m8n  offset
	sub     edx,ebp
	mov     bl,[esi]
	sbb     esi,ecx
	mov     ax,[volumetab+ebx+ebx]
	add     [edi+offset],ax
ENDM

M8N_mixloop:
	inner_m8n   0
	inner_m8n   2
	inner_m8n   4
	inner_m8n   6
	add	edi,8
	sub     eax,010000h
	jnc     M8N_mixloop

	READ_CYCLES

M8_endmix:
	mov	ebp,[mixparameters]

        ; write back ofs,frac
        sub     esi,[Source]
        mov     eax,[ofs]
        mov     [eax],esi
        mov     eax,[frac]
        mov     [eax],edx

        popad
	ret
endp	_I86_mix13_m8


; 13BIT-mix: STEREO-8BIT-mixing -----------------------------------------------
proc	_I86_mix13_s8
	arg	Destination:DWORD,Source:DWORD,ofs:DWORD,frac:DWORD,\
		direction:DWORD,step_int:DWORD,step_frac:DWORD,\
		todo:DWORD,vol:DWORD,panpos:DWORD

        pushad
	mov	[mixparameters],ebp

        ; 'local volume'
        mov     ebx,[vol]
	mov	eax,[panpos]
	imul	ebx
	mov	[lvol],ah
	mov	eax,255
	sub	eax,[panpos]
	imul	ebx
	mov	[rvol],ah

        mov     edi,[Destination]

        mov     esi,[Source]
        mov     eax,[ofs]
        add     esi,[eax]

        mov     eax,[frac]
        mov     edx,[eax]
        mov     ecx,[todo]

        mov	eax,[step_int]
	mov	[__stepint],eax
        mov	eax,[step_frac]
	mov	[__stepfrac],eax

	cmp	[direction],1
	je	S8_negdir

; positiv direction:
        mov     ebp,[__stepfrac]
	shl	ecx,16
	mov	cl,[lvol]
	mov	ch,[rvol]

	cmp	[__stepint],0
	je	pre_S8P_zeroint_mixloop

	IF PROFILE
	mov	eax,ecx
	shr	eax,16
	add 	[mixloops],eax
	ENDIF
	PREPARE_CYCLE_READ
align 4
S8P_mixloop:
	mov	bh,ch
	add     edx,ebp
	mov     bl,[esi]
	mov     ax,[volumetab+ebx+ebx]
	mov	bh,cl			
	adc     esi,[__stepint]		
	shl	eax,16
	mov     ax,[volumetab+ebx+ebx]	
	add     [edi],eax
	add	edi,4			
	sub	ecx,010000h
	jnc     S8P_mixloop		
	READ_CYCLES
	jmp	S8_endmix

pre_S8P_zeroint_mixloop:
	IF PROFILE
	mov	eax,ecx
	shr	eax,16
	add 	[mixloops],eax
	ENDIF
	PREPARE_CYCLE_READ
align 4
S8P_zeroint_mixloop:
	mov	bh,ch
	add     edx,ebp
	mov     bl,[esi]
	mov     ax,[volumetab+ebx+ebx]
	mov	bh,cl			
	adc     esi,0
	shl	eax,16
	mov     ax,[volumetab+ebx+ebx]	
	add     [edi],eax
	add	edi,4			
	sub	ecx,010000h
	jnc     S8P_zeroint_mixloop	; pair ok
	READ_CYCLES
	jmp	S8_endmix


; negativ direction:

S8_negdir:
        mov     ebp,[__stepfrac]
	shl	ecx,16
	mov	cl,[lvol]
	mov	ch,[rvol]

S8N_mixloop:
	mov	bh,cl
	sub     edx,ebp		; pair ok
	mov     bl,[esi]
	sbb     esi,[__stepint]		; pair ok
	mov     ax,[volumetab+ebx+ebx]
	mov	bh,ch		; pair ok
	add     [edi],ax
	mov     ax,[volumetab+2*ebx]	; pair ok ?
	add     [edi+2],ax
	add	edi,4		; pair ok ?
	sub	ecx,010000h
	jnc     S8N_mixloop	; pair ok

S8_endmix:
	mov	ebp,[mixparameters]

        ; write back ofs,frac
        sub     esi,[Source]
        mov     eax,[ofs]
        mov     [eax],esi
        mov     eax,[frac]
        mov     [eax],edx

        popad

	ret
endp	_I86_mix13_s8


; 13BIT-mix: amplify to 8BIT --------------------------------------------------
proc	_I86_amp13_8
	arg     Destination:DWORD,Source:DWORD,count:DWORD
	pushad
	xor	ebx,ebx
	mov	edi,[Source]
	mov	esi,[Destination]
	mov	ecx,[count]

amploop:
	mov	bx,[edi]
	add	edi,2		; pair ok 

	mov	al,[posttab+2*ebx]
	inc	esi		; pair ok

	dec	ecx
	mov	[esi-1],al
	jnz	amploop

	popad
	ret
endp	_I86_amp13_8

; clear temporary buffer for 13bit mixing -------------------------------------
proc	_I86_clrbuf13
	arg	Buffer:DWORD,Value:DWORD,entries:DWORD
        pushad

	mov	[mixparameters],ebp

	mov	ax,ds
	mov	es,ax
	mov	edi,[Buffer]
	mov	ecx,[entries]
	mov	eax,[Value]
	mov	bx,ax
	shl	eax,16
	mov	ax,bx
	shr	ecx,1
	rep	stosd
	jnc	@@noadditional
	stosw
@@noadditional:
	popad
	ret
endp	_I86_clrbuf13

end

; PENTIUM OPTIMIZING:
;
; cli
; db 0fh,31h
; mov [cyclcount],eax
; db 0d6h
; -----
; db 0d6h
; db 0fh,31h
; sti
; sub eax,[cyclecount]
; sub eax,16d
; eax = no cycles
;