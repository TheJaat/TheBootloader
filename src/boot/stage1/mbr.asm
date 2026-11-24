; mbr.asm - the master boot record of a TheTaaJ image written to a USB
; stick (or any disk): what a PC BIOS runs from a disk's first sector.
;
; The image is a "hybrid": an ISO 9660 CD image whose first 32 KiB -
; which ISO 9660 leaves unused, the "system area" - start with this
; sector. Booted as a CD, the BIOS ignores it and follows El Torito to
; stage1. Booted as a disk, the BIOS loads this sector to 0x7C00 and
; runs it, and it does what El Torito would have done: load stage1's
; first 2 KiB to 0x7C00 and jump there, with the drive number in DL.
;
; Where stage1 is: xorriso writes it at 0x1B0 when it makes the image
; (-isohybrid-mbr, ISOLINUX's convention): the boot image's first
; sector, in 512-byte sectors from the start of the disk, 64 bits.
; Then come the disk signature (0x1B8), the partition table (0x1BE) and
; 0x55 0xAA (0x1FE), also xorriso's.
;
; A BIOS loads a disk's first sector to 0x7C00 - where stage1 must go
; too. So this sector first moves itself to 0x0600, out of the way.

BITS 16
ORG 0x0600

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 256
    rep movsw
    jmp 0:moved

moved:
    sti
    mov [drive], dl
    ; LBA reads (int 13h extensions): every BIOS since the late 1990s,
    ; and the only way to reach past the first 8 GB.
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc no_extensions
    cmp bx, 0xAA55
    jne no_extensions
    mov eax, [stage1_lba]
    mov [dap.lba], eax
    mov eax, [stage1_lba + 4]
    mov [dap.lba + 4], eax
    mov si, dap
    mov ah, 0x42
    mov dl, [drive]
    int 0x13
    jc read_failed
    mov dl, [drive]
    jmp 0:0x7C00

no_extensions:
    mov si, s_no_extensions
    jmp fail
read_failed:
    mov si, s_read_failed
fail:
    lodsb
    test al, al
    jz .halt
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp fail
.halt:
    cli
    hlt
    jmp .halt

drive: db 0
; The disk address packet for int 13h, ah=42h: 4 sectors (2 KiB, as El
; Torito loads) to 0000:7C00.
align 4
dap:
    db 16, 0
    dw 4
    dw 0x7C00, 0x0000
.lba: dq 0

s_no_extensions: db "TheTaaJ: this BIOS cannot read disks by LBA", 13, 10, 0
s_read_failed: db "TheTaaJ: cannot read the loader from the disk", 13, 10, 0

times 0x1B0 - ($ - $$) db 0
stage1_lba: dq 0
; The disk signature, the partition table and the boot signature are
; xorriso's to write.
times 510 - ($ - $$) db 0
dw 0xAA55
