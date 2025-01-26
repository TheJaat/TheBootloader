%ifndef _COMMON_DISK_INC_
%define _COMMON_DISK_INC_

; ********************************
; ReadFromDisk
; IN:
; 	- DL: Drive number (0x00 = floppy A:, 0x80 = first HDD, etc.)
;	- CH: Cylinder number
;	- DH: Head number
;	- CL: Sector number (1-63)
;	- ES:BX: Memory address to load data to
;	- AL: Number of sectors to read
; ********************************
ReadFromDisk:
	; Save state
	pushad

	; Prepare for BIOS disk read
	mov 	ah, 0x02          ; BIOS function: Read sectors
	int 	0x13              ; Call BIOS disk interrupt

	; Check for errors
	jc 	.DiskReadError       ; If carry flag is set, jump to error handler

	; Restore state & return
	popad
	ret

.DiskReadError:
	; Handle disk read error (could be retry logic or error message)
	mov si, diskErrorMessage
	call PrintString16BIOS
	hlt
	jmp 	.DiskReadError


; **************************
; BIOS ReadFromDiskUsingExtendedBIOSFunction
; IN:
; 	- ES:BX: Buffer
;	- EAX: Sector start low-dword
;	- ESI: Sector start high-dword
; 	- ECX: Sector count
; 	- EDX: Sector size in bytes
;
; Registers:
; 	- Conserves all but ES:BX
; **************************
ReadFromDiskUsingExtendedBIOSFunction:
	pushad

	; Set initial 
	mov 	word [DiskPackage.Segment], es
	mov 	word [DiskPackage.Offset], bx
	mov 	dword [DiskPackage.Sector], eax
	mov	dword [DiskPackage.Sector + 4], esi

	.sLoop:
		; Setup Package
		mov 	word [DiskPackage.SectorsToRead], 1

		; Setup INT
		push 	edx
		mov 	al, 0
		mov 	ah, 0x42
		mov 	dl, byte [bPhysicalDriveNum]
		mov 	si, DiskPackage
		int 	0x13

		; It's important we check for offset overflow
		pop 	edx
		mov 	ax, word [DiskPackage.Offset]
		add 	ax, dx
		mov 	word [DiskPackage.Offset], ax
		test 	ax, ax
		jne 	.NoOverflow

	.Overflow:
		; So overflow happened
		add 	word [DiskPackage.Segment], 0x1000
		mov 	word [DiskPackage.Offset], 0x0000

	.NoOverflow:
		; Loop
		inc 	dword [DiskPackage.Sector]
		loop 	.sLoop

	.End:
	; Restore registers
	popad

	; Save position
	push 	eax
	xor 	eax, eax
	mov 	ax, word [DiskPackage.Segment]
	mov 	es, ax
	mov 	bx, word [DiskPackage.Offset]
	pop 	eax
ret
	
diskErrorMessage db 'Disk read error!', 0

; This is used for the extended read function (int 0x13)
DiskPackage:				db 0x10
					db 0
	.SectorsToRead			dw 0
	.Offset				dw 0
	.Segment 			dw 0
	.Sector 			dq 0

%endif
