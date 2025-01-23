BITS 16                 ; Set the code to 16-bit mode (required for bootloaders)
ORG 0x0500              ; Set the origin to 0x7C00, where the BIOS loads the bootloader

stage2_entry:
jmp stage2

;; Include files
%include "utils16/print16.inc"

stage2:

    ;; Print stage2 welcome message
    mov si, welcome_stage2_str
    call Print_String16
jmp $


;; Data
welcome_stage2_str: db "Welcome to stage 2", 0

times 1024 - ($ - $$) db 0
