%ifndef _GDT_INC_
%define _GDT_INC_

BITS 16

; Definitions
%define 		NULL_DESC 	0
%define 		CODE_DESC 	0x8
%define 		DATA_DESC 	0x10
%define			CODE16_DESC	0x18
%define 		DATA16_DESC 	0x20



; ********************************
; InstallGdt
; ********************************
InstallGdt32:
	; Save state
	pushad		; pushes all general purpose 32-bit registers onto the stack
			; This saves their current values so they can be restored later.

	mov	si, sGDT32InstallingSentence
	call	PrintString16BIOS
	call	PrintNewline		; \n

	; Clear interrupts
	cli 		; Disable interrupts to prevent any interruptions.

	; Load Gdt
	lgdt 	[GDT32]

	; Restore & Return
	sti		; Restore interrupts.
	
	mov	si, sGDT32InstalledSentence
	call	PrintString16BIOS
	call	PrintNewline		; \n
	
	popad		; Pops the previously saved values of general purpose 32-bit
				; registers.
	ret

;*******************************************
; Global Descriptor Table
;*******************************************
StartOfGdt:		; Beginning of the GDT section
	dd 0            ; null descriptor
	dd 0 

; gdt code:	        ; code descriptor
	dw 0FFFFh       ; limit low
	dw 0		; base low
	db 0            ; base middle
	db 10011010b    ; access
	db 11001111b    ; granularity
	db 0            ; base high

; gdt data:	        ; data descriptor
	dw 0FFFFh       ; limit low (Same as code)10:56 AM 7/8/2007
	dw 0            ; base low
	db 0            ; base middle
	db 10010010b    ; access
	db 11001111b    ; granularity
	db 0            ; base high

; gdt code 16bit:	; code descriptor
	dw 0FFFFh       ; limit low
	dw 0            ; base low
	db 0            ; base middle
	db 10011010b    ; access
	db 00001111b    ; granularity
	db 0            ; base high

; gdt data 16bit:       ; data descriptor
	dw 0FFFFh       ; limit low (Same as code)10:56 AM 7/8/2007
	dw 0            ; base low
	db 0            ; base middle
	db 10010010b    ; access
	db 00001111b    ; granularity
	db 0            ; base high
EndOfGdt:

; The actual Gdt Header
GDT32:
	dw EndOfGdt - StartOfGdt - 1	; Size
	dd StartOfGdt			; Starting address

; Interrupt Descriptor Table
Idt16:
	dw 0x3ff		; 256 entries, 4b each = 1K
	dd 0			; Real Mode IVT @ 0x0000

sGDT32InstallingSentence db 'Installing the GDT 32...', 0
sGDT32InstalledSentence db 'Installed the GDT 32...', 0

%endif
