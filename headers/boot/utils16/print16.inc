%ifndef _COMMON_PRINT_16_INC
%define _COMMON_PRINT_16_INC

; 16 Bit Code
BITS 16

;; ******************************************
;; PrintNewline
;; This advaces the cursor to the next line
;; It is equivalent to '\n' in C/C++
;;
;; IN:
;;    Nothing
;; OUT:
;;     Advances the cursor to the starting of new line.
;; Clobbers:
;;          All general-purpose registers (saved/restored
;;          using pusha/popa).
;; ------------------------------------------
PrintNewline:
    pusha    ; Stores the state of general purpose registers

    mov ah, 0x0e    ; BIOS function to print character
    mov al, 0x0A    ; Newline character
    int 0x10        ; Call BIOS interrupt to print character

    mov al, 0x0d    ; Carriage return character
    int 0x10        ; Call BIOS interrupt to print character

    popa    ; Restores the state of general purpose registers
ret
;; ******************************************


;; ******************************************
;; Print_String16
;; Prints a null-terminated string on the screen at the
;; current cursor position.
;;
;; IN:
;;    DS:SI = Pointer to the null-terminated string to print.
;; OUT:
;;     Displays the string on the screen and advances the cursor.
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
;; IN:
;;    None (used hardcoded values for BIOS calls).
;; OUT:
;;     Clears the screen and set the cursor at (0, 0).
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
    mov dh, 0x00        ; Row (0)
    mov dl, 0x00        ; Column (0)
    call SetCursorPosition  ; Call the function which sets the cursor at particular position.

    popa        ; Restore all general-purpose registers
ret             ; Return to the caller
;; ******************************************


;; ******************************************
;; SetCursorPosition
;; Moves the cursor to the specified row and column
;;
;; IN:
;;   - DH - Row (0 - 24).
;;   - DL - Column (0 - 79).
;; Out:
;;     Moves the cursor to the specified position.
;; Clobbers:
;;         AH, BH (used for BIOS calls).
;; ------------------------------------------
SetCursorPosition:
    mov ah, 0x02        ; BIOS function: Set cursor position
    mov bh, 0x00        ; Page number (0)
    int 0x10            ; Call BIOS interrupt
    ret
;; ******************************************


;; ******************************************
;; PrintWordHex
;; Prints the value stored in DX 16-bit register
;; in hex.
;;
;; IN:
;;   - DX: (16-bit) Value to be printed in hex.
;; OUT:
;;     Prints the value in hex.
; ------------------------------------------
PrintWordHex:
    pusha    ; Stores the state of all general purpose registers

    mov cx, 4	; offset in string, counter (4 hex characters)

    .hex_loop:
        mov ax, dx	          ; Hex word passed in DX
        and al, 0Fh               ; Mask the lowe nibble (4 bits) of AL
        mov bx, hexToASCII
        xlatb                     ; AL = [DS:BX + AL]
                                  ; Convert the nibble to ASCII using the lookup table

        mov bx, cx                ; Need bx to index data
        mov [hexString + bx + 1], al  ; Store hex char in string buffer
        ror dx, 4                 ; Rotate DX right by 4 bits to bring the next nibble
                                  ; into position
        loop .hex_loop            ; Decrement CX and repeat for the next nibble

        mov si, hexString         ; Load the address of the hex string
        mov ah, 0Eh               ; BIOS function to print a character
        mov cx, 6                 ; Length of string (4 hex digits + optional prefix 0x)
        .loop:
            lodsb                 ; Load the next byte from [SI] into AL
            int 10h		  ; BIOS interrupt to print the character in AL
        loop .loop                ; Decrement CX and repeat until all characters are printed
        popa           ; Restore the state of all general purpose registers
ret
;; ******************************************


;; ******************************************
;; Member Variables
;; ------------------------------------------

hexString:    db '0x0000'
hexToASCII:   db '0123456789ABCDEF'
;; ******************************************

%endif
