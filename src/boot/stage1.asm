BITS 16                 ; Set the code to 16-bit mode (required for bootloaders)
ORG 0x7c00              ; Set the origin to 0x7C00, where the BIOS loads the bootloader

jmp main_gate

%include "../../headers/boot/print16.inc"

; Entry point of the bootloader
main_gate:

    mov si, welcome_string
    call Print_String16

    ; Infinite loop to prevent execution from continuing into unknown memory
hang:
    jmp hang            ; Loop forever


; data section
welcome_string: db "Welcome to stage 1", 0

; Padding to ensure the bootloader is 512 bytes (required by BIOS)
padding:
    times 510 - ($ - $$) db 0  ; Fill remaining bytes with 0s up to 510 bytes

; Boot signature (mandatory for BIOS to recognize this as a valid bootloader)
boot_signature dw 0xAA55 ; Signature value at the end of the 512-byte sector
