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
;; PrintChar16BIOS
;; This function prints the char on the screen stored in AL register using the BIOS interrupts.
;; IN:
;;    - AL: Char
;; OUT:
;;     Print the char in the screen
;; Clobbers:
;;         - AX, BX
;; -----------------------------------------
PrintChar16BIOS:
    ; Save the state of AX and BX registers
    push ax
    push bx

    ; Setup for BIOS teletype output (INT 0x10, AH = 0x0E)
    mov ah, 0x0E               ; BIOS teletype function (TTY output)
    mov bl, byte [bTextColor]  ; Load the text color into BL
    mov bh, 0x00               ; Set the page number to 0 (usually not needed for TTY output)
    int 0x10                   ; BIOS video interrupt to print the character in AL

    ; Restore the state of BX and AX registers
    pop bx
    pop ax
ret             ; Return from the PrintChar16BIOS routine
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
;; PrintWordDecimal
;; IN:
;;    - AX: NumberToPrint
;; OUT:
;;     Prints the Value in Decimal
;; Clobbers:
;;          All general-purpose registers (saved/restored
;;          using pusha/popa).
; -------------------------------------------
PrintWordDecimal:
    ; Save all general-purpose registers
    pushad

    ; Initialize variables
    xor bx, bx        ; Clear BX to use it as a counter for the number of digits
    mov cx, 10         ; Set CX to 10, the divisor for converting number to digits

    .DigitLoop:
    xor dx, dx        ; Clear DX before division
    div cx             ; Divide AX by 10

    ; After div: AX contains quotient, DX contains remainder

    ; Convert remainder to ASCII
    add dx, 48         ; Convert digit to ASCII ('0' = 48)

    ; Store ASCII digit on the stack
    push dx

    inc bx             ; Increment digit count

    ; If quotient (EAX) is zero, we're done converting digits
    cmp ax, 0
    jnz .DigitLoop       ; If EAX is not zero, repeat the loop

    .PrintLoop:
    ; Pop ASCII digit from stack into EAX
    pop ax

    ; Print the character in EAX
    call PrintChar16BIOS

    ; Decrease digit count in EBX
    dec bx
    jnz .PrintLoop     ; If EBX is not zero, print next digit

    ; Restore all general-purpose registers
    popad
ret          ; Return from the PrintWordDecimal routine
;; ******************************************


;; ******************************************
;; Member Variables
;; b - byte sized variable
;; ------------------------------------------

bTextColor    db    0x0F

hexString:    db '0x0000'
hexToASCII:   db '0123456789ABCDEF'
;; ******************************************

%endif
