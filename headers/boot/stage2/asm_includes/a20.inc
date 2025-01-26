%ifndef _STAGE_2_A20_INC_
%define _STAGE_2_A20_INC_

BITS 16

; ********************************
; CheckA20Gate
; OUT: 
;   - AX: 0 If A20 is disabled, 1 If enabled
; ******************************** 
CheckA20Gate:
	pushf               ; Save flags register
	push    ds          ; Save DS register
	push    es          ; Save ES register
	push    di          ; Save DI register
	push    si          ; Save SI register
 
	cli                 ; Clear interrupts to prevent interference
 
	xor     ax, ax      ; Clear AX (set AX to 0)
	mov     es, ax      ; Set ES segment to 0
 
	not     ax          ; Invert AX (set AX to 0xFFFF)
	mov     ds, ax      ; Set DS segment to 0xFFFF
 
	mov     di, 0x0500  ; Set DI to 0x0500 (ES:DI points to 0x000500)
	mov     si, 0x0510  ; Set SI to 0x0510 (DS:SI points to 0x10FF0)
 
	mov     al, byte [es:di] ; Load byte from ES:DI (0x000500) into AL
	push    ax          ; Save the byte read from 0x000500

	mov     al, byte [ds:si] ; Load byte from DS:SI (0x10FF0) into AL
	push    ax          ; Save the byte read from 0x10FF0
 
	mov     byte [es:di], 0x00 ; Write 0x00 to 0x000500
	mov     byte [ds:si], 0xFF ; Write 0xFF to 0x10FF0
 
	cmp     byte [es:di], 0xFF ; Compare byte at 0x000500 with 0xFF
 
	pop     ax          ; Restore original byte to AL from stack
	mov     byte [ds:si], al  ; Write the original byte back to 0x10FF0
 
	pop     ax          ; Restore original byte to AL from stack
	mov     byte [es:di], al  ; Write the original byte back to 0x000500
 
	mov     ax, 0       ; Set AX to 0 (assume A20 is disabled)
	je      CheckA20Gate__Exit ; If the comparison was equal (A20 disabled), jump to exit
 
	mov     ax, 1       ; Otherwise, set AX to 1 (A20 enabled)
 
CheckA20Gate__Exit:
	pop     si          ; Restore SI register
	pop     di          ; Restore DI register
	pop     es          ; Restore ES register
	pop     ds          ; Restore DS register
	popf                ; Restore flags register
ret                 ; Return from subroutine


; ********************************
; A20MethodBios
; ******************************** 
A20MethodBios:
	mov		si, sA20MethodBIOSSentence
	call		PrintString16BIOS
	call		PrintNewline

	; BIOS function to enable A20 line
	mov     ax, 0x2401    ; Load AX with BIOS function 2401h (Enable A20)
	int     0x15          ; Call BIOS interrupt 15h to enable A20
ret                   ; Return from subroutine



; ********************************
; A20MethodKeyboardController
; ******************************** 
A20MethodKeyboardController:
        cli                     ; Clear interrupts to prevent interference
        
        mov		si, sA20MethodKeyboardControllerSentence
	call		PrintString16BIOS
	call		PrintNewline

        call    A20Wait         ; Wait for the keyboard controller to be ready
        mov     al,0xAD         ; Command to disable the keyboard
        out     0x64,al         ; Send command to keyboard controller

        call    A20Wait         ; Wait for the keyboard controller to be ready
        mov     al,0xD0         ; Command to read the output port
        out     0x64,al         ; Send command to keyboard controller

        call    A20Wait2        ; Wait until there is data to read
        in      al,0x60         ; Read data from the keyboard controller's output port
        push    eax             ; Save the current state of the output port

        call    A20Wait         ; Wait for the keyboard controller to be ready
        mov     al,0xD1         ; Command to write to the output port
        out     0x64,al         ; Send command to keyboard controller

        call    A20Wait         ; Wait for the keyboard controller to be ready
        pop     eax             ; Restore the saved state of the output port
        or      al,2            ; Set the A20 enable bit (bit 1) in the output port value
        out     0x60,al         ; Write the modified value back to the output port

        call    A20Wait         ; Wait for the keyboard controller to be ready
        mov     al,0xAE         ; Command to enable the keyboard
        out     0x64,al         ; Send command to keyboard controller

        call    A20Wait         ; Wait for the keyboard controller to be ready
        sti                     ; Enable interrupts
        ret                     ; Return from subroutine

; A20Wait - Wait until the keyboard controller is ready to accept a command
A20Wait:
        in      al,0x64         ; Read status register from keyboard controller
        test    al,2            ; Test input buffer status bit (bit 1)
        jnz     A20Wait         ; If bit 1 is set, wait (loop) until it is cleared
        ret                     ; Return from subroutine

; A20Wait2 - Wait until there is data available to read from the keyboard controller
A20Wait2:
        in      al,0x64         ; Read status register from keyboard controller
        test    al,1            ; Test output buffer status bit (bit 0)
        jz      A20Wait2        ; If bit 0 is clear, wait (loop) until it is set
ret                     ; Return from subroutine


; ********************************
; EnableA20Gate
; ******************************** 
EnableA20Gate:
	; Save State
	pusha	; Push all general-purpose registers onto the stack

	; Print the enabling a20 line string
	mov	si, sA20GateSentence   ; Load address of string into SI
	call	PrintString16BIOS      ; Call Print subroutine to print the sentence
	call	PrintNewline		; \n

	; Step 1, check the status of A20 Gate
	call	CheckA20Gate        ; Call CheckA20Gate subroutine to check if A20 is enabled
	cmp	ax, 1           ; Compare AX with 1 (AX = 1 if A20 is enabled)
	je		.Done   ; If equal (A20 is enabled), jump to .Done

	; First we try to enable through BIOS
	call	A20MethodBios   ; Call A20MethodBios subroutine to enable A20 via BIOS

	; Did it work?
	call	CheckA20Gate        ; Call CheckA20Gate subroutine to check if A20 is enabled
	cmp		ax, 1           ; Compare AX with 1 (AX = 1 if A20 is enabled)
	je			.Done           ; If equal (A20 is enabled), jump to .Done

	; Failed, not supported then, try keyboard controller
	call	A20MethodKeyboardController ; Call A20MethodKeyboardController subroutine

	; Did it work?
	call	CheckA20Gate        ; Call CheckA20Gate subroutine to check if A20 is enabled
	cmp		ax, 1           ; Compare AX with 1 (AX = 1 if A20 is enabled)
	je			.Done           ; If equal (A20 is enabled), jump to .Done

.Error:
	; Failed :(
	mov     si, sA20GateFailSentence ; Load address of string into SI
	call    PrintString16BIOS         ; Call Print subroutine to print the failure message

	; Halt
	call    SystemFail   ; Call SystemFail subroutine to handle failure
				; Which waits for keypress and restarts the system.
	cli                   ; Clear interrupts
	hlt                   ; Halt the processor

.Done:
	; Print
	mov     si, sA20GateSuccessSentence ; Load address of szSuccess string into ESI
	call    PrintString16BIOS          ; Call Print subroutine to print the success message
	call	PrintNewline

        ; Restore & Return
	popa          ; Restore all general-purpose registers from the stack
ret                   ; Return from subroutine


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Variables

sA20GateSentence db 'The A20 Area.', 0
sA20GateFailSentence db 'Enabling A20 Failed.', 0
sA20GateSuccessSentence db 'Enabling A20 Succeed.', 0
sA20MethodBIOSSentence db 'Trying to enable A20 using BIOS...', 0
sA20MethodKeyboardControllerSentence db 'Trying to enable A20 using Keyboard Controller...', 0

%endif
