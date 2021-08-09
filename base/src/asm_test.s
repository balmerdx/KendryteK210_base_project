    .section .text,"ax",@progbits
    .option norvc #disable compressed instructions
    .globl	asm_get_int
    .type	asm_get_int, @function
asm_get_int:
    .cfi_startproc
    li a0, 2345
    ret
    .cfi_endproc
    
    .globl	asm_get_str
    .type	asm_get_str, @function
asm_get_str:
    .cfi_startproc
    lla a0, .asm_get_str.1
    ret
.asm_get_str.1:
    .string	"my_str"
    .cfi_endproc

	.section	.text.asm_loop,"ax",@progbits
	.align	1
    .globl	asm_loop
    .type	asm_loop, @function
asm_loop:
    .cfi_startproc
    addi	a0, a0, 1
.loop:
    addi	a0, a0, -1
    bnez	a0, .loop
    ret
    .cfi_endproc
    .size	asm_loop, .-asm_loop

	.section	.text.asm_loop,"ax",@progbits
	.align	1
    .globl	asm_loop
    .type	asm_loop, @function
    
	.section	.text.asm_loop_float,"ax",@progbits
	.align	1
    .globl	asm_loop_float
    .type	asm_loop_float, @function
asm_loop_float:
    .cfi_startproc
    addi	a1, a0, 1
    li      a0, 0
.loop1:
    #fcvt.w.s a0, fa0,rtz
    #fmadd.s fa3, fa0, fa1, fa2
    fsqrt.s fa0, fa0
    addi	a1, a1, -1
    bnez	a1, .loop1
    #fmv.s fa0, fa3
    ret
    .cfi_endproc
    .size	asm_loop_float, .-asm_loop_float

    .end
