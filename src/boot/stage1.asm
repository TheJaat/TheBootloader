BITS 16                 ; Set the code to 16-bit mode (required for bootloaders)
ORG 0x7c00              ; Set the origin to 0x7C00, where the BIOS loads the bootloader

; Entry point of the bootloader
main_gate:
    ; Print the character 'J' to the screen
    mov ah, 0x0e        ; BIOS teletype function (interrupt 0x10, AH=0x0E)
    mov al, 'J'         ; Load the ASCII value of 'J' into AL
    int 0x10            ; Call the BIOS interrupt to print the character

    ; Infinite loop to prevent execution from continuing into unknown memory
hang:
    jmp hang            ; Loop forever

; Padding to ensure the bootloader is 512 bytes (required by BIOS)
padding:
    times 510 - ($ - $$) db 0  ; Fill remaining bytes with 0s up to 510 bytes

; Boot signature (mandatory for BIOS to recognize this as a valid bootloader)
boot_signature dw 0xAA55 ; Signature value at the end of the 512-byte sector

