BITS 16                 ; Set the code to 16-bit mode (required for bootloaders)
ORG 0x7c00              ; Set the origin to 0x7C00, where the BIOS loads the bootloader

main_gate:

jmp main

;; Include files
%include "utils16/print16.inc"

; Entry point of the bootloader
main:
    ; Disable Interrupts
    cli

    ; Far jump to fix segment registers
    jmp 0x0:FixCS

FixCS:
    ; Fix segment registers to 0
    xor ax, ax
    mov ds, ax
    mov es, ax

    ;; ******************************************
    ;; Set Stack
    ;; The stack segment (SS) and stack pointer (SP) are initialized to 
    ;; ensure the stack operates correctly. The stack grows downward in memory.
    ;; 
    ;; SS is set to the same value as the code segment (CS) because, in a 
    ;; bootloader, code and stack often reside in the same segment. 
    ;; SP is set to 0x7C00, which is just below the start of the bootloader's 
    ;; code (0x7C00 is the entry point for the boot sector). This setup ensures 
    ;; there is space for the stack to grow downward without interfering with 
    ;; other memory regions.
    ;; AX is 0x0000 (segment base address for SS)
    mov ss, ax            ; Set the Stack Segment (SS)
    mov sp, 0x7C00        ; Set the Stack Pointer (SP) to 0x7C00
    ;; ******************************************

    ; Set interrupts
    sti

    ; Save the DL register value, which contains the disk number
    mov byte [bPhysicalDriveNum], dl

    call ClearScreenAndResetCursor    ; Clear the screen and reset the
                                      ; cursor at the top left corner

    ;; Print the stage1 welcome message
    mov si, welcome_stage1_str    ; Load the address of the string into si register
    call Print_String16           ; Print the string pointed by si
    call PrintNewline     ; \n


    ;; ******************************************
    ;; Debugging Purpose
    ;; ------------------------------------------
    ;; Print the Register value in hex and decimal
    mov ax, 1234
    mov dx, ax
    call PrintWordHex    ; Print dx value in hex
    call PrintNewline	; \n
    call PrintWordDecimal    ; Print ax value in number
    call PrintNewline	; \n
    ;; ******************************************

    ;; ******************************************
    ;; Calculate and print the actual code size without padding
    ;; of stage1 right from the start to the before the ending statement.
    ;; ------------------------------------------
    mov ax, actual_code_end - main_gate
    mov si, sActualStage1SizeStatement
    call Print_String16
    call PrintWordDecimal
    call PrintNewline
    ;; ******************************************

    ;; ******************************************
    ;; Calculate and print the padded code size of stage1
    ;; The padded code is with padding, from start to the very
    ;; ending after the times statement.
    mov ax, stage1_end - main_gate
    mov si, sPaddedStage1SizeStatement
    call Print_String16
    call PrintWordDecimal
    call PrintNewline
    ;; ******************************************

    ; Infinite loop to prevent execution from continuing into unknown memory
hang:
    jmp hang            ; Loop forever




;; ******************************************
;; data section
;; ******************************************

bPhysicalDriveNum db 0	; Variable to store disk number

welcome_stage1_str: db "Welcome to stage 1", 0
sActualStage1SizeStatement: db 'Actual size of the stage1 code (without padding in bytes): ', 0
sPaddedStage1SizeStatement: db "Padded size of the stage1 code (with padding in bytes): ", 0

;; Actual code end flag
actual_code_end:    ; After this there is padding only

; Padding to ensure the bootloader is 512 bytes (required by BIOS)
padding:
    times 510 - ($ - $$) db 0  ; Fill remaining bytes with 0s up to 510 bytes

; Boot signature (mandatory for BIOS to recognize this as a valid bootloader)
boot_signature dw 0xAA55 ; Signature value at the end of the 512-byte sector

stage1_end:    ; Padded code ended
