%ifndef _COMMON_PRINT_16_INC
%define _COMMON_PRINT_16_INC

; 16 Bit Code
BITS 16

;; ******************************************
;; Print_String16
;; Prints a null-terminated string on the screen at the
;; current cursor position.
;;
;; Inputs:
;;       DS:SI = Pointer to the null-terminated string to print.
;; Outputs:
;;       Displays the string on the screen and advances the cursor.
;; Clobbers:
;;          All general-purpose registers (saved/restored
;;          using pusha/popa).
;; ------------------------------------------
Print_String16:
    pusha               ; Save all register

.loopy:
    lodsb               ; Load byte from DS:SI into AL and increment SI
    test al, al         ; Check if AL == 0 (null terminator)
    jz .done            ; Exit loop if end of string
    mov ah, 0x0E        ; BIOS Teletype function
    int 0x10            ; Print character
    jmp .loopy          ; Continue loop
.done:

    popa                ; Restore all registers
    ret                 ; Return to caller
;; ******************************************


;; ******************************************
;; ClearScreenAndResetCursor
;; This function clears the screen and resets the cursor
;; at default position (0, 0) in text mode.
;;
;; Inputs:
;;       None (used hardcoded values for BIOS calls).
;; Outputs:
;;       Clears the screen and set the cursor at (0, 0).
;; Clobbers:
;;          All general-purpose registers (saved/restored
;;          using pusha/popa).
;; ------------------------------------------
ClearScreenAndResetCursor:
    pusha	; Save all general-purpose registers

    mov ah, 0x06        ; BIOS Function: scroll up
    mov al, 0           ; Number of lines to scroll (0 = clear entire screen)
    mov bh, 0x07        ; Attribute for blank lines (light gray on black)
    mov cx, 0x0000      ; CH = 0, CL = 0 (upper left corner)
    mov dx, 0x184F      ; End at the bottom right (row 24, column 79)
    int 0x10            ; Call BIOS interrupt
	
    ; Reset cursor position to the top-left corner
    mov ah, 0x02        ; BIOS function: Set cursor position
    mov bh, 0x00        ; Page number (0)
    mov dh, 0x00        ; Row (0)
    mov dl, 0x00        ; Column (0)
    int 0x10            ; Call BIOS interrupt

    popa        ; Restore all general-purpose registers
ret             ; Return to the caller
;; ******************************************

%endif
