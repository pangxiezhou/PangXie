
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;                               syscall.asm
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;                                                     Forrest Yu, 2005
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

%include "sconst.inc"

INT_VECTOR_SYS_CALL equ 0x90
_NR_get_ticks       equ 0
_NR_write	    equ 1
_NR_open		equ 2
_NR_close		equ 3
_NR_read		equ 4
_NR_writef		equ 5
_NR_delete	equ 6
_NR_fork		equ 7
_NR_goin		equ 8
; 导出符号
global	get_ticks
global	write
global	open
global	close
global	read
global	writef
global	deletef
global	fork
global	goin


;debug
extern debug
bits 32
[section .text]
;int fork();

goin:
	mov eax, _NR_goin
	 mov ebx, [esp +4]
	int		INT_VECTOR_SYS_CALL
	ret

fork:
	mov eax, _NR_fork
	int		INT_VECTOR_SYS_CALL
	;push eax
	;call debug
	;pop eax
	;mov eax, 10
	ret

deletef:
	mov eax, _NR_delete
	mov ebx, [esp +4]
	int		INT_VECTOR_SYS_CALL
	ret

;read(int fd, void* buf, int size)
read:
	mov eax, _NR_read
	mov ebx, [esp +4]
	mov ecx, [esp + 8]
	mov edx, [esp + 12]
	int		INT_VECTOR_SYS_CALL
	ret

writef:
	mov eax, _NR_writef
	mov ebx, [esp +4]
	mov ecx, [esp + 8]
	mov edx, [esp +12]
	int		INT_VECTOR_SYS_CALL
	ret

;open(const char* pathname, int flags)
open:
	mov eax, _NR_open
	mov ebx, [esp +4]
	mov ecx, [esp + 8]
	int		INT_VECTOR_SYS_CALL
	ret

;close(int fd)
close:
	mov	eax, _NR_close
	mov ebx, [esp + 4]
	int		INT_VECTOR_SYS_CALL
	ret
; ====================================================================
;                              get_ticks
; ====================================================================
get_ticks:
	mov	eax, _NR_get_ticks
	int		INT_VECTOR_SYS_CALL
	ret

; ====================================================================================
;                          void write(char* buf, int len);
; ====================================================================================
write:
        mov     eax, _NR_write
        mov     ebx, [esp + 4]
        mov     ecx, [esp + 8]
        int     INT_VECTOR_SYS_CALL
        ret
