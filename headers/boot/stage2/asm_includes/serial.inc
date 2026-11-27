;; serial.inc - stage 2's markers on the first serial port (K23).
;;
;; Stage 2 prints on the screen, which no test reads and no log keeps. A
;; few lines on COM1 as well - stage 2 started, the kernel read, loaded,
;; the screen mode set, the jump - put the loader's steps in the boot log,
;; where tools/boot-timeline.py stamps each with its time (K23), and where
;; a boot that dies before the kernel shows how far it got.
;;
;; Each waits for the port's "transmitter empty" bit, then sends a byte.
;; A PC with no port reads 0xFF there - the bit set - so it never waits
;; for ever. The kernel sets the port up properly later; until then QEMU
;; and real UARTs alike send at whatever rate the firmware left.
;;
;; SerialMark16: SI = a 0-terminated string (DS:SI), in real mode.
;; SerialMark32: ESI = the same, in protected mode.
;; Both send "\r\n" after it, and change no register.

COM1_DATA   equ 0x3F8
COM1_STATUS equ 0x3FD

BITS 16
SerialMark16:
    pusha
.Next:
    lodsb
    test al, al
    jz .Line
    call SerialPut16
    jmp .Next
.Line:
    mov al, 13
    call SerialPut16
    mov al, 10
    call SerialPut16
    popa
    ret

SerialPut16:
    push dx
    push ax
    mov dx, COM1_STATUS
.Wait:
    in al, dx
    test al, 0x20
    jz .Wait
    pop ax
    mov dx, COM1_DATA
    out dx, al
    pop dx
    ret

BITS 32
SerialMark32:
    pushad
.Next:
    lodsb
    test al, al
    jz .Line
    call SerialPut32
    jmp .Next
.Line:
    mov al, 13
    call SerialPut32
    mov al, 10
    call SerialPut32
    popad
    ret

SerialPut32:
    push edx
    push eax
    mov dx, COM1_STATUS
.Wait:
    in al, dx
    test al, 0x20
    jz .Wait
    pop eax
    mov dx, COM1_DATA
    out dx, al
    pop edx
    ret
