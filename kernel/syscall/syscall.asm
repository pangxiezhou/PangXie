
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
; 导出符号
global	get_ticks
global	write
global	open
global	close

bits 32
[section .text]

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
