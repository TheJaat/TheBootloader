%ifndef _VESA_INC_
%define _VESA_INC_

BITS 16

; Structure Definitions
struc VbeControllerStruct
	.Signature			resb	4
	.Version 			resw	1
	.OemNamePtr			resd 	1
	.Capabilities		resd 	1
	.ModeListPtr		resd 	1 	; List of WORDS terminated by 0xFFFF
	.Num64KBlocksMem	resw 	1
	.ReservedSpace 		resb 	492
endstruc

struc VbeModeStruct
	.Attributes 		resw 	1
	.WindowAttribsA 	resb 	1
	.WindowAttribsB		resb 	1
	.WindowGranKb 		resw 	1
	.WindowSizeKb 		resw 	1
	.WindowASegment 	resw 	1
	.WindowBSegment 	resw 	1
	.FuncPtrPosition 	resd 	1
	.BytesPerScanline 	resw 	1
	.ResolutionX		resw 	1
	.ResolutionY 		resw 	1
	.CharCellWidth 		resb 	1
	.CharCellHeight 	resb 	1
	.NumPlanes 			resb 	1
	.Bpp 				resb 	1
	.NumBanks 			resb 	1
	.MemoryModel		resb 	1
	.BankSizeKb 		resb 	1
	.MaxImagePages 		resb 	1
	.Reserved 			resb 	1
	.RedMaskSize 		resb 	1
	.RedFieldPos 		resb 	1
	.GreenMaskSize 		resb 	1
	.GreenFieldPos 		resb 	1
	.BlueMaskSize 		resb 	1
	.BlueFieldPos 		resb 	1
	.AlphaMaskSize		resb 	1
	.AlphaFieldPos 		resb 	1
	.DirectColorAttribs resb 	1
	.FrameBuffer 		resd 	1
	.Reserved1 			resd 	1
	.Reserved2 			resw 	1
	.ReservedSpace 		resb 	213
endstruc

; **************************
; VesaGetVideoModes
; IN:
; 	- ES:DI: Buffer
; OUT:
; 	- EAX: 0 for success
; **************************
VesaGetVideoModes:
	; Save state
	pushad

	; Setup for INT
	xor 	eax, eax
	mov 	ax, 0x4F00
	int 	0x10

	; Check if success
	test 	ah, ah
	jne 	.NoVesa

	cmp 	al, 0x4F
	jne 	.NoVesa

	; Yay! Go back
	popad
	;; Save in the bootheader to be passed to kernel
	mov 	dword [BootHeader + MultiBoot.VBEControllerInfo], edi
	mov 	eax, 0
	ret

	; Damniiiit
	.NoVesa:
		popad
		mov 	eax, 1
		ret

; **************************
; VesaFindBestSuitableMode
; IN:
; 	- ES:SI: List of Modes
; **************************
VesaFindBestSuitableMode:
	; Save state
	pushad

	.cLoop:
		; Null buffer
		push 	esi
		xor 	eax, eax
		mov 	edi, VesaModeInfo
		mov 	ecx, 63
		rep 	stosd
		pop 	esi

		; Load mode
		mov 	cx, word [es:si]

		; End of list? 
		cmp 	cx, 0xFFFF
		je 		.Done

		; Setup INT 
		mov 	ax, 0x4F01
		mov 	edi, VesaModeInfo
		int 	0x10

		; Did it work?
		test 	ah, ah
		jne 	.NextEntry

		cmp 	al, 0x4F
		jne 	.NextEntry

		; Obviously, worked, now lets parse
		mov 	ax, word [VesaModeInfo + VbeModeStruct.Attributes]

		; We want Color, Graphics, LFB
		and 	ax, 0x009B
		cmp 	ax, 0x009B
		jne 	.NextEntry

		; We want 32 bit modes!! ALPHA FTW
		mov 	al, byte [VesaModeInfo + VbeModeStruct.Bpp]
		cmp 	al, 0x20
		jne 	.NextEntry

		; Ok... The modes we are looking for are:
		; 1920x1080
		; 1600x900
		; 1280x720
		; 1024x768
		mov 	ax, word [VesaModeInfo + VbeModeStruct.ResolutionX]
		mov 	bx, word [VesaModeInfo + VbeModeStruct.ResolutionY]

		; Check 1024x768
		cmp 	ax, 0x0400
		jne .NextEntry
		;jne 	.Check1280
		cmp 	bx, 0x0300
		jne .NextEntry
		;jne 	.Check1280
		jmp 	.ValidateEntry

		; Ok, so this is 1024x768
		.Check1280:
			cmp 	ax, 0x0500
			jne 	.Check1600
			cmp 	bx, 0x02D0
			jne 	.Check1600
			jmp 	.ValidateEntry

		.Check1600:
			cmp 	ax, 0x0640
			jne 	.Check1920
			cmp 	bx, 0x0384
			jne 	.Check1920
			jmp 	.ValidateEntry

		.Check1920:
			cmp 	ax, 0x0780
			jne 	.NextEntry
			cmp 	bx, 0x0438
			jne 	.NextEntry

		.ValidateEntry:
			; Check if we have better stored
			cmp 	ax, word [wCurrentResX]
			jbe 	.NextEntry
			cmp 	bx, word [wCurrentResY]
			jbe 	.NextEntry

			; Save
			mov 	word [wCurrentResX], ax
			mov 	word [wCurrentResY], bx
			mov 	word [wOptimizedVesaMode], cx

		.NextEntry:
			; Increase esi
			add 	esi, 2
			jmp 	.cLoop

	.Done:
		; Return
		popad
		ret

; **************************
; VesaFinish
; **************************
VesaFinish:
	; Save state
	pushad

	; Make sure it is not 0
	cmp 	word [wOptimizedVesaMode], 0
	je 		.Done

	; Setup INT 
	mov 	ax, 0x4F02
	mov 	bx, word [wOptimizedVesaMode]
	or 		bx, 0x4000
	int 	0x10

	.Done:
		; Restore & Return
		popad
		ret

; **************************
; VesaSetup
; IN:
; 	- ES:DI: Buffer
; **************************
VesaSetup:
	; Save state
	pushad

	;; Print Entry Statement
	mov 	esi, szVesaSetup
	call 	PrintString16BIOS
	;call	PrintNewline		; \n

	; Setup buffer
	xor 	eax, eax
	xor 	edi, edi
	mov 	es, ax
	mov 	edi, VesaControllerInfo

	; Get controller info
	call 	VesaGetVideoModes
	test 	eax, eax
	jne 	.Fail

	; Now parse list
	xor 	eax, eax
	mov 	es, ax
	mov 	esi, dword [VesaControllerInfo + VbeControllerStruct.ModeListPtr]
	call 	VesaFindBestSuitableMode

	; Yay?
	cmp 	word [wOptimizedVesaMode], 0
	je 		.Fail

	; YAY!
	mov 	esi, szVesaModeSpace
	call 	PrintString16BIOS
	xor 	eax, eax
	mov 	ax, word [wCurrentResX]
	call 	PrintWordNumber
	mov 	esi, szVesaModeSeperator
	call 	PrintString16BIOS
	mov 	ax, word [wCurrentResY]
	call 	PrintWordNumber
	mov 	esi, szVesaModeEnd
	call 	PrintString16BIOS

	; Reload structure
	xor 	ecx, ecx
	mov 	ax, 0x4F01
	mov 	cx, word [wOptimizedVesaMode]
	mov 	edi, VesaModeInfo
	mov 	dword [BootHeader + MultiBoot.VBEModeInfo], edi
	mov 	dword [BootHeader + MultiBoot.VBEMode], ecx
	int 	0x10
	
	call PrintNewline	; \n

	; Restore & Return
	popad 
	ret

	.Fail:
		; Ew
		mov 	esi, sVESAFailure
		call 	PrintString16BIOS 

		; Restore & Return
		popad 
		ret


; ****************************
; Variables
; ****************************

; Strings - 0x0D (LineFeed), 0x0A (Carriage Return)
sVESAFailure					db	'VESA Failure', 0
szVesaSetup 					db 		"Retrieving Optimal Video Modes: ", 0x00
szVesaModeSpace					db 		" [", 0x00
szVesaModeSeperator				db 		"x", 0x00
szVesaModeEnd					db 		"]", 0x00

; Etc
wOptimizedVesaMode 				dw 		0
wCurrentResX					dw 		0
wCurrentResY 					dw 		0

; ****************************
; VBE Structures
; ****************************
VesaControllerInfo:
istruc VbeControllerStruct
	at VbeControllerStruct.Signature,			db		0, 0, 0, 0
	at VbeControllerStruct.Version, 			dw		0
	at VbeControllerStruct.OemNamePtr,			dd 		0
	at VbeControllerStruct.Capabilities,		dd 		0
	at VbeControllerStruct.ModeListPtr,			dd 		0 	; List of WORDS terminated by 0xFFFF
	at VbeControllerStruct.Num64KBlocksMem,		dw 		0
	at VbeControllerStruct.ReservedSpace,		times 	492 db 0
iend


VesaModeInfo:
istruc VbeModeStruct
	at VbeModeStruct.Attributes, 		dw 	0
	at VbeModeStruct.WindowAttribsA, 	db 	0
	at VbeModeStruct.WindowAttribsB,	db 	0
	at VbeModeStruct.WindowGranKb, 		dw 	0
	at VbeModeStruct.WindowSizeKb, 		dw 	0
	at VbeModeStruct.WindowASegment, 	dw 	0
	at VbeModeStruct.WindowBSegment, 	dw 	0
	at VbeModeStruct.FuncPtrPosition, 	dd 	0
	at VbeModeStruct.BytesPerScanline, 	dw 	0
	at VbeModeStruct.ResolutionX,		dw 	0
	at VbeModeStruct.ResolutionY, 		dw 	0
	at VbeModeStruct.CharCellWidth, 	db 	0
	at VbeModeStruct.CharCellHeight, 	db 	0
	at VbeModeStruct.NumPlanes, 		db 	0
	at VbeModeStruct.Bpp, 			    db 	0
	at VbeModeStruct.NumBanks, 		    db 	0
	at VbeModeStruct.MemoryModel,		db 	0
	at VbeModeStruct.BankSizeKb, 		db 	0
	at VbeModeStruct.MaxImagePages, 	db 	0
	at VbeModeStruct.Reserved, 		    db 	0
	at VbeModeStruct.RedMaskSize, 		db 	0
	at VbeModeStruct.RedFieldPos, 		db 	0
	at VbeModeStruct.GreenMaskSize, 	db 	0
	at VbeModeStruct.GreenFieldPos, 	db 	0
	at VbeModeStruct.BlueMaskSize, 		db 	0
	at VbeModeStruct.BlueFieldPos, 		db 	0
	at VbeModeStruct.AlphaMaskSize,		db 	0
	at VbeModeStruct.AlphaFieldPos, 	db 	0
	at VbeModeStruct.DirectColorAttribs,db 	0
	at VbeModeStruct.FrameBuffer, 		dd 	0
	at VbeModeStruct.Reserved1, 		dd 	0
	at VbeModeStruct.Reserved2, 		dw 	0
	at VbeModeStruct.ReservedSpace, 	times 	213 db 0
iend

modes_list:
	times 256 dw 0  ; Space to store up to 256 mode numbers
;VesaModeCount:
;	dw 0		; Keep track of the number of modes

struc VesaModeEntry
	.ModeID			resw 1
	.HorizontalResolution	resw 1
	.VerticalResolution	resw 1
endstruc

%define MAX_VESA_MODES 256
%define VesaModeEntry_size 6

VesaModeBuffer resb VesaModeEntry_size * 256
VesaModeCount resw 1


%endif
