
;;******************************************
;; Print String in Real mode using interrupt
;;
;; Input:
;;	SI = null-terminated string
Print_String16:
.loopy:
    lodsb               ; Load the next byte from [SI] into AL, increment SI
    cmp al, 0           ; Check if AL (current character) is 0 (null terminator)
    je .done            ; If null terminator, exit the loop
    mov ah, 0x0e        ; Set AH for teletype output (BIOS interrupt 0x10)
    int 0x10            ; Call interrupt to print character in AL
    jmp .loopy          ; Repeat the loop
.done:
    ret                 ; Return to caller
;;******************************************
