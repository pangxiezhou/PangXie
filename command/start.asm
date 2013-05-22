;; start.asm

extern	main

bits 32

[section .text]

global _start

_start:
	push	eax
	push	ecx
	;for simple to make main to unend loop
	call	main
	;; need not clean up the stack here

	hlt	; should never arrive here
