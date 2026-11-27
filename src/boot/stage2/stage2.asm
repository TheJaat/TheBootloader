BITS 16

global stage2_entry
jmp stage2_entry

%include "defines.inc" ; contains defintion for STAGE_2_LOAD_ADDRESS

;ORG STAGE_2_LOAD_ADDRESS	; 0x0500
;; No need of ORG directive as we have specified the address in the
;; Linker script and we are generating pure binary file from LD linker.

; Memory Map
; 0x00000000 - 0x000003FF	Reserved (1KB), Real Mode IVT (Interrupt Vector Table)
; 0x00000400 - 0x000004FF	Reserved (256 bytes), BDA (BIOS Data Area)
; 0x00000500 - 0x00007AFF	Temp Buffer (~29 Kb)
; 0x00007B00 - 0x00007BFF	Stack Space (256 Bytes)
; 0x00007C00 - 0x0000CBFF	ISO Stage1 Bootloader (20 KiloBytes = 20,480 bytes)
; 0x0000CC00 - 0x0007FFFF	Second Stage Bootloader, 460 KB.
; 0x00080000 - 0x0009FFFF	128 KB, Can be used by subsystems in this bootloader
                            ; Used by the ISO9660 fs to as a buffer when read.
			                ; This memory will be known as the Subsystem memory area
			                ; It can be accesses with segment:offset
			                ; segment = 0x8000, offset = 0x00
			                ; Thus complete address => segement*16 + offset
			                ; 0x8000 * 16 + 0 = 0x80000
                            ; 0x9000 is the Memory Map Location
; 0x000A0000 - 0x000BFFFF	128 KB, Video Display Memory, reserved
; 0x000C0000 - 0x000C7FFF	32 KB, Video BIOS
; 0x000C8000 - 0x000EFFFF	160 KB BIOS Expansion
; 0x000F0000 - 0x000FFFFF	64 KB Motherboard BIOS

; Includes
%include "utils16/print16.inc"
%include "common.inc"	; For common thing between the two stages, SystemFail
%include "memory.inc"	; For memory related
%include "datastructure.inc"
%include "a20.inc"	; For enabling A20
%include "gdt.inc"	; For GDT
%include "vesa.inc"	; For VESA
%include "disk.inc"	; For disk reading
%include "keyboard.inc"	; For keyboard related stuff

stage2_entry:

    ;; The boot log's first line (K23): the firmware and stage 1 are done.
    mov si, sMarkStarted
    call SerialMark16

    mov si, WelcomeToStage2            ; Print Stage 2 Welcome message
    call PrintString16BIOS
    call PrintNewline                  ; \n

    ;; -------------------------------------------------------------------
    ;; Receive the passed drive number from stage 1
    ;; It is passed in register AL
    mov [bPhysicalDriveNum], al         ; store the received drive number
    ;; A CD's sectors (2048 bytes) or a disk's (512: the image on a USB
    ;; stick)? bios_disk.c reads either way.
    call DiskSectorSize
    mov si, sReceivedDriveNumber
    call PrintString16BIOS
    call PrintWordNumber                ; Print the received drive number
    call PrintNewline                   ; \n
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; Print the Bootloader name
    mov esi, sBootloaderName
    call PrintString16BIOS
    call PrintNewline
    ;; Set the Bootloader name in multiboot structure
    ;; to be passed to kernel.
    mov esi, sBootloaderName
    mov dword [BootHeader  + MultiBoot.BootLoaderName], esi
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; Detect Size of Lower (Conventional) Memory
    call GetLowerMemorySize
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; Detect the Size of Both the Lower and Higher Memory
    ;; with the Use of int 0x15.
    call SetupMemory
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; Enable A20 Gate (Line)
    call EnableA20Gate
    ;; -------------------------------------------------------------------


    ;; -------------------------------------------------------------------
    call InstallGdt32
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; Get Best Video Mode and its information
    mov si, sMarkVesa
    call SerialMark16
    call VesaSetup
    mov si, sMarkVesaDone
    call SerialMark16
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; We now have to get into the 32-bit land to copy the
    ;; kernel to 0x1000000, then get back to real mode,
    ;; to do other things.
    ; Go, Go, Go - with interrupts off: from the moment PE is set until
    ; Temp32Bit, the IDT is still the real-mode one, and an interrupt
    ; there is a triple fault (seen under KVM: TCG rarely hits the window)
    cli
    mov eax, cr0
    or eax, 1
    mov cr0, eax
	
    ; Jump to temporary 32-bit
    jmp CODE_DESC:Temp32Bit

;; Black Hole
jmp $

;; -------------------------------------------------------------------
;; The temporary 32-bit stay area
BITS 32
;; External C/C++ functions
    extern load_elf             ; elf.c: ELF32 or ELF64, whichever was staged
    extern g_kernelIs64         ; elf.c: set when the kernel is ELF64
    extern prepare_long_mode    ; longmode.c
    extern EnterLongMode        ; longmode.asm

    ; stage2 printing
    extern init_print_stage2



    extern init_vfs
    extern load_kernel


    ;; global variable declared in ata.c and has the kernel size
    ;; NOTE: this is the size of kernel.elf *on disk*, NOT the span of the
    ;; loaded image. It is no longer what we hand to the kernel.
    extern g_kernelSize
    ;; ramdisk globals, filled in by load_ramdisk in vfs.c
    extern load_ramdisk
    extern g_ramdiskAddress
    extern g_ramdiskSize
    ;; globals declared in elf.c, filled in by load_elf32
    extern g_kernelAddress      ; lowest  p_vaddr of any PT_LOAD
    extern g_kernelEnd          ; highest p_vaddr + p_memsz
    extern g_kernelEntry        ; e_entry from the ELF header


;; 32-bit includes
    %include "print32.inc"      ; For Printing in 32 bit assembly


Temp32Bit:
    ; Disable Interrupts
    cli
	
    ; Setup Segments and Stack
    xor eax, eax
    mov ax, DATA_DESC
    mov ds, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov es, ax
    mov esp, 0x7BFF

    ;; -------------------------------------------------------------------
    ;; Clear the screen
    call ClearScreen32
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    mov esi, sProtectedModeWelcomeSentence
    mov bl, LMAGENTA                  ; Foreground = Light Magenta
    mov bh, BLACK                     ; Background = Black
    call PrintString32
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; initialize print
    call init_print_stage2
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; Find the kernel in the ISO 9660 file system and load it. The disk
    ;; is read through the BIOS (bios_disk.c, bios_read below): a CD, or
    ;; the same image on a USB stick or a hard disk, behind whatever
    ;; controller the BIOS knows. (32-bit C functions take their
    ;; arguments on the stack, pushed right to left - cdecl.)
    call init_vfs    ; Initialize the Virtual FileSystem

    push kernel_name    ; Push the address of the kernel name string onto the stack
    call load_kernel
    add esp, 4          ; Clean up the stack, as its the caller's responsibility
    ;; Return:
    ;;        - 1: Success, Kernel was found and loaded.
    ;;        - 0: Failure, Kernel either not found or not loaded.
    cmp eax, 1               ; Success kernel was found and loaded
    jne ErrorLoadingKernel   ; Display error string of kernel loading failure
    mov esi, sMarkKernelRead
    call SerialMark32

    ;; Here, kernel was loaded successfully

    ;; -------------------------------------------------------------------
    ;; The kernel image geometry is NOT known yet - g_kernelSize is only
    ;; the size of kernel.elf on disk (headers, symtab and all), which is
    ;; unrelated to where the image actually lands in memory. Both
    ;; KernelAddr and KernelSize are filled in after load_elf32 below.

    ;; At this point, kernel was loaded successfully.
    ;; If it is the binary kernel, we can directly jump to the 
    ;; loaded location, but if of any other format like elf then
    ;; we need to parse it.
    ; jmp 0x300000            ; assembly way, for jumping to the binary kernel
    ; call jump_to_kernel     ; C-way, for jumping to the binary kernel.

    ;; -------------------------------------------------------------------
    call load_elf
    ;; Check the return value
    ;;                       - 1: Success, print success message,
    ;;                       - otherwise: failure
    cmp eax, 1               ; check Return
    ;;      - if 0 - failure
    ;;      - if 1 - success
    jne ELFParseLoadingError
    ;; loading successful
    mov esi, sMarkKernelLoaded
    call SerialMark32

    ;; Get the kernel load address from the g_kernelAddress variable
    ;; declared and assigned value in elf.c
    ;; After retrieving the kernel load address store it in the
    ;; OsBootDescriptor Structure, which is to be passed to kernel.
    ;; -------------------------------------------------------------------
    ;; Load the ramdisk. A missing RAMDISK.MDR is not fatal - load_ramdisk
    ;; returns success with a zero size and the kernel boots without one.
    call load_ramdisk
    test eax, eax
    jnz  .RamdiskOk
    mov  si, sRamdiskFailed
    call PrintString32
    .RamdiskOk:
    mov esi, sMarkRamdisk
    call SerialMark32

    mov dword eax, [g_ramdiskAddress]
    mov dword [BootDescriptor + OsBootDescriptor.RamDiskAddr], eax
    mov dword eax, [g_ramdiskSize]
    mov dword [BootDescriptor + OsBootDescriptor.RamDiskSize], eax

    ;; KernelAddr = lowest p_vaddr, KernelSize = span of the loaded image
    ;; (highest p_vaddr + p_memsz, so .bss is included).
    mov dword eax, [g_kernelAddress]
    mov dword [BootDescriptor + OsBootDescriptor.KernelAddr], eax
    mov dword ecx, [g_kernelEnd]
    sub ecx, eax
    mov dword [BootDescriptor + OsBootDescriptor.KernelSize], ecx

    ;; -------------------------------------------------------------------
    ;; Jump to protected real mode
    ;; Load 16-bit protected mode descriptor
    mov eax, DATA16_DESC
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax

    ;; Jump to protected real mode, set CS!
    jmp CODE16_DESC:PrepareToJumpToReal

;; Another Black hole
jmp $

;; -------------------------------------------------------------------
;; bios_read(lba, count) - for bios_disk.c, from 32-bit C
;;
;; Reads `count` sectors of the boot disk, from sector `lba`, into
;; BIOS_BUFFER (0x60000) with int 13h function 42h. Returns 0, or the
;; BIOS's error code (never 0 on an error: 0xFF if it gave none).
;;
;; int 13h works only in real mode, so: down to 16-bit protected mode
;; (CODE16_DESC), protection off, real mode with the BIOS's interrupt
;; table; the call; and back up the same way. The stack stays where it
;; is: it is below 64 KiB, so SS = 0 and SP = ESP's low half reach it.
;; This code must be below 64 KiB too - real mode's CS = 0 reaches no
;; further - and stage2.asm is linked first, at 0xCC00.
;; -------------------------------------------------------------------
BITS 32
global bios_read
global wDiskSectorSize
bios_read:
    push ebp
    mov ebp, esp
    pushad
    mov eax, [ebp + 8]
    mov [BiosDap.Lba], eax
    mov eax, [ebp + 12]
    mov [BiosDap.Count], ax
    mov [BiosSavedEsp], esp
    jmp CODE16_DESC:.Protected16
BITS 16
.Protected16:
    mov ax, DATA16_DESC
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax
    jmp 0:.Real
.Real:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    lidt [Idt16]
    sti
    mov ah, 0x42
    mov dl, [bPhysicalDriveNum]
    mov si, BiosDap
    int 0x13
    jnc .Read
    test ah, ah
    jnz .Failed
    mov ah, 0xFF
.Failed:
    mov [BiosResult], ah
    jmp .Up
.Read:
    mov byte [BiosResult], 0
.Up:
    cli
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_DESC:.Protected32
BITS 32
.Protected32:
    mov ax, DATA_DESC
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [BiosSavedEsp]
    popad
    movzx eax, byte [BiosResult]
    pop ebp
    ret

align 4
BiosDap:
    db 16, 0
.Count: dw 0
    dw 0x0000, 0x6000          ; 6000:0000 = 0x60000, BIOS_BUFFER
.Lba: dq 0
BiosSavedEsp: dd 0
BiosResult: db 0

BITS 16
PrepareToJumpToReal:
    ; Load 16 bit IDT
    lidt [Idt16]
    ; Disable Protected Mode
    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax

    ; Far jump to real mode unprotected
    jmp 0:Temp16Bit

;; Another Black hole
jmp $

ELFParseLoadingError:
    mov esi, sKernelELFReadingLoadingFailedSentence
    call PrintString32
jmp $

ErrorLoadingKernel:
    mov esi, sKernelLoadingFailureSentence
    mov bl, LMAGENTA           ; Foreground = Light Magenta
    mov bh, BLACK              ; Background = Black
    call PrintString32
jmp $

;; -------------------------------------------------------------------
BITS 16

Temp16Bit:
    ;; Clear registers
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi

    ;; Setup segments, leave 16 bit protected mode
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ;; Setup stack
    mov ss, ax
    mov ax, 0x7BFF
    mov sp, ax
    xor ax, ax

    ;; Enable interrupts
    sti

    ;; Switch Video Mode
    mov si, sMarkMode
    call SerialMark16
    call VesaFinish

    ;; Goto Permanent Protected Mode! Interrupts off first, as above:
    ;; VesaFinish ran with them on, and the IDT is still the real-mode one
    cli
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ;; Jump into 32 bit
    jmp CODE_DESC:ProtectedMode

;; Another Black hole
jmp $

;; -------------------------------------------------------------------
BITS 32

;; Permanent Protected Mode
;; 32 Bit includes
    %include 'print32.inc'		; For printing
    %include 'serial.inc'		; The steps on the serial port (K23)
BITS 32
ProtectedMode:
    ; Update segment registers
    xor eax, eax
    mov ax, DATA_DESC            ; Data segment selector 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esi, sMarkModeSet
    call SerialMark32

    mov esp, 0x7BFF	
		; set the stack top to the 0x7BFF
    		; The stack grows from high memory to lower memory
    		;  --------
    		;  |______| 0x7BFF
    		;  |______|    
    		;  |......|  it is growing downward as we pushes
    		;  |______|		data
    		;  |______|
    		;  |      | 0x7AFF

    ;; Disable the all irq lines
    mov al, 0xff
    out 0xa1, al
    out 0x21, al

    cli            ; Disable the interrupt as they are no more available
                   ; in real mode.

    ;; -------------------------------------------------------------------
    ;; Clear the screen
    call ClearScreen32
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; Print welcome sentence for protected mode
	mov esi, sProtectedModeWelcomeSentence
	mov bl, LMAGENTA	; Foreground = Light Magenta
	mov bh, BLACK		; Background = Black
	call PrintString32
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
	;; jmp to the kernel
	; jmp MEMLOCATION_KERNEL_LOAD_OFFSET
	;; Jump to the ELF entry point, NOT to a hard-coded 0x100000.
	;; kernel.ld puts .multiboot at 0x100000 and aligns .text to the next
	;; 4K boundary, so kernel_entry actually lives at 0x101000. Jumping to
	;; 0x100000 executes the 12-byte multiboot header as instructions, and
	;; its first one is `add dh, [eax+0x31bad]` - which silently corrupts
	;; bits 8-15 of EDX, the BootDescriptor pointer we are about to pass.
	;; A 64-bit kernel takes a different road from here: page tables,
	;; long mode, and a TaajBootInfo instead of the MultiBoot structures.
	cmp dword [g_kernelIs64], 0
	je .Kernel32

	push VesaModeInfo
	push BootHeader
	call prepare_long_mode          ; returns the PML4 address in EAX
	add esp, 8
	mov esi, sMarkJump
	call SerialMark32               ; changes no register: EAX is kept
	jmp EnterLongMode               ; does not return

.Kernel32:
	mov ecx, [g_kernelEntry]

	xor esi, esi
	xor edi, edi
	mov eax, MULTIBOOT_MAGIC
	mov ebx, BootHeader
	mov edx, BootDescriptor

	jmp ecx
    ;; -------------------------------------------------------------------

;; Final Black hole
jmp $


;times (25*1024) - ($ - $$) db 0
;section .data
;; ************************************************
;; Variables
;; ************************************************
bPhysicalDriveNum 	db	0

WelcomeToStage2 db 'Welcome to the Stage2', 0
sReceivedDriveNumber db 'Received Drive Number in Stage 2: ', 0

sBootloaderName	db	"TheTaaJBoot Version 0.0.1, Author: TheJat", 0

sProtectedModeWelcomeSentence	db	'Entered the Protective Land', 0
sKernelLoadedSentence	db	'Kernel was Loaded', 0
module_name db 'MODULE.ELF', 0  ; Define the string with a null terminator
kernel_name db 'KERNEL/KERNEL.ELF', 0
sRamdiskFailed db 'Ramdisk load failed, continuing without one', 0x0D, 0x0A, 0   ; Define the string with a null terminator
									    ; Now we are in ISO level 3 which supports upto 31 characters for the file identifier
										; If again switching back to ISO level 1, which might be the default, if not specified explicitly.
										; In that case, change the name to 11 characters long, 8 for name and 3 for extension,
										; separated by the dot (.)
sKernelLoadingFailureSentence db 'Failure in loading kernel', 0

;; The loader's steps on the serial port (K23, serial.inc).
sMarkStarted      db '[stage2] started', 0
sMarkVesa         db '[stage2] memory map, A20 and GDT done; choosing a screen mode', 0
sMarkVesaDone     db '[stage2] screen mode chosen; reading the kernel', 0
sMarkKernelRead   db '[stage2] kernel read', 0
sMarkKernelLoaded db '[stage2] kernel loaded (ELF)', 0
sMarkRamdisk      db '[stage2] ramdisk done', 0
sMarkMode         db '[stage2] setting the screen mode', 0
sMarkModeSet      db '[stage2] screen mode set', 0
sMarkJump         db '[stage2] page tables built; to the kernel', 0
sKernelELFReadingLoadingFailedSentence db 'Failure in reading and loading kernel elf file.', 0

;times (300*1023 - ($ - $$)) db 0
very_far:
    mov ah, 0x0e
    mov al, '3'
    int 0x10

jmp $
;times (460 * 1024 - ($ - $$)) db 0 ;; 460 KB
;times STAGE_2_SIZE - ($-$$) db 0		; Fill up the remaining space with zeroes
;; No need of it, as we have the assembly and C code together.
;; Since it only pad the assembly code so either find a way
;; to pad the merged flat binary file of assembly and c stage 2 code
;; (can be done with linker script in ld)
;; or left it as it is. We have 28KB stage for our stage 2.
;; Once the stage2 code breaks the 28KB barriers then it starts behaving
;; abnormally then understand the size barrier of stage2 is broken.
;; It will work perfectly until the stage 2 code is below 28 KB, It is similar
;; to the way if we padded the rest of code with 0.