BITS 16

global stage2_entry
jmp stage2_entry

%include "defines.inc" ; contains defintion for STAGE_2_LOAD_ADDRESS

;ORG STAGE_2_LOAD_ADDRESS	; 0x0500
;; No need of ORG directive as we have specified the address in the
;; Linker script and we are generating pure binary file from LD linker.

; Memory Map: OLD
; 0x00000000 - 0x000004FF		Reserved
; 0x00000500 - 0x00007AFF		Second Stage Bootloader (~29 Kb)
; 0x00007B00 - 0x00007BFF		Stack Space (256 Bytes)
; 0x00007C00 - 0x00007DFF		Bootloader (512 Bytes)
; 0x00007E00 - 0x00008FFF		Used by subsystems in this bootloader
; 0x00009000 - 0x00009FFF		Memory Map
; 0x0000A000 - 0x0000AFFF		Vesa Mode Map / Controller Information
; 0x0000B000 - 0x0007FFFF		File Loading Bay

; Memory Map: NEW
; 0x00000000 - 0x000003FF	Reserved (1KB), Real Mode IVT (Interrupt Vector Table)
; 0x00000400 - 0x000004FF	Reserved (256 bytes), BDA (BIOS Data Area)
; 0x00000500 - 0x00007AFF	Second Stage Bootloader (~29 Kb)
; 0x00007B00 - 0x00007BFF	Stack Space (256 Bytes)
; 0x00007C00 - 0x0000CBFF	ISO Stage1 Bootloader (20 KiloBytes = 20,480 bytes)
; 0x0000CC00 - 0x0007FFFF	460 KB, File Loading.
; 0x00080000 - 0x0009FFFF	128 KB, Can be used by subsystems in this bootloader
			; This memory will be known as the Subsystem memory area
			; It can be accesses with segment:offset
			; segment = 0x8000, offset = 0x00
			; Thus complete address => segement*16 + offset
			; 0x8000 * 16 + 0 = 0x80000
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

    mov si, WelcomeToStage2            ; Print Stage 2 Welcome message
    call PrintString16BIOS
    call PrintNewline                  ; \n

    ;; -------------------------------------------------------------------
    ;; Receive the passed drive number from stage 1
    ;; It is passed in register AL
    mov [bPhysicalDriveNum], al         ; store the received drive number
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
    call VesaSetup
    ;; -------------------------------------------------------------------

    ;; -------------------------------------------------------------------
    ;; We now have to get into the 32-bit land to copy the
    ;; kernel to 0x1000000, then get back to real mode,
    ;; to do other things.
    ; Go, Go, Go
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
    extern load_elf32
    extern ata_read_sector

    ; stage2 printing
    extern init_print_stage2

    extern detect_ata_devices

    extern check_and_set_atapi_device

    extern init_vfs
    extern load_kernel

    extern find_and_load_kernel_from_9660_using_atapi
    extern jump_to_kernel

    ;; global variable declared in ata.c and has the kernel size
    extern g_kernelSize
    ;; global variable declared in elf.c and has the elf kernel load address
    extern g_kernelAddress

    ;; Defined in boot_menu.c file
    extern DrawBootMenu

    extern create_menu

;; 32-bit includes
    %include "ata.inc"          ; For ATA interface
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
    ; Identify the ATA devices
    call identify_ata_devices
;jmp $
    call detect_ata_devices
    ;; -------------------------------------------------------------------
    ;; Find the kernel from the ISO 9660 fs and load it to the destined location.
    ;; 32-bit system make use of cdecl calling convention,
    ;; while x64 make use of System V AMD64 Calling Convention;
    ;; 
    ;; cdecl calling convention =
    ;; In the cdecl calling convention, which is common for x86 (32-bit) systems,
    ;; function arguments are passed on the stack.
    ;; Arguments are pushed from right to left, and the caller is responsible
    ;; for cleaning up the stack after the function call.
    ;;
    ;; System V AMD64 calling convention =
    ;; In the System V AMD64 calling convention, which is common for x86-64
    ;; (64-bit) systems, the first six integer or pointer arguments are
    ;; passed in registers rdi, rsi, rdx, rcx, r8, and r9.
    ;; Additional arguments are passed on the stack.
;jmp temp

    call check_and_set_atapi_device    ; Check for the ATAPI device
    cmp eax, 1                         ; Check if we found the valid ATAPI device
            ; if - 1: Success
            ; if - 0: Failure
    jne NoATAPIDevice

    call init_vfs    ; Initialize the Virtual FileSystem
    call load_kernel
    jmp next
jmp $

temp:
    push kernel_name    ; Push the address of the kernel name string onto the stack
    call find_and_load_kernel_from_9660_using_atapi
    add esp, 4          ; Clean up the stack, as its the caller'r responsibility
jmp $
    ;; Return:
    ;;        - 1: Success, Kernel was found and loaded.
    ;;        - 0: Failure, Kernel either not found or not loaded.
    cmp eax, 1               ; Success kernel was found and loaded
    jne ErrorLoadingKernel   ; Display error string of kernel loading failure

next:

    ;; Get the kernel size from the g_kernelSize variable declared and
    ;; assigned value in ata.c
    ;; After retrieving the kernel size store it in the OsBootDescriptor
    ;; Structure, which is to be passed to kernel.
    mov dword eax, [g_kernelSize]
    mov dword [BootDescriptor + OsBootDescriptor.KernelSize], eax

    ;; At this point, kernel was loaded successfully.
    ;; If it is the binary kernel, we can directly jump to the 
    ;; loaded location, but if of any other format like elf then
    ;; we need to parse it.
    ; jmp 0x300000            ; assembly way, for jumping to the binary kernel
    ; call jump_to_kernel     ; C-way, for jumping to the binary kernel.

    ;; -------------------------------------------------------------------
    call load_elf32
    ;; Check the return value
    ;;                       - 1: Success, print success message,
    ;;                       - otherwise: failure
    cmp eax, 1               ; check Return
    ;;      - if 0 - failure
    ;;      - if 1 - success
    jne ELFParseLoadingError
    ;; loading successful
jmp $
    ;; Get the kernel load address from the g_kernelAddress variable
    ;; declared and assigned value in elf.c
    ;; After retrieving the kernel load address store it in the
    ;; OsBootDescriptor Structure, which is to be passed to kernel.
    mov dword  eax, [g_kernelAddress]
    mov dword [BootDescriptor + OsBootDescriptor.KernelAddr], eax

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

NoATAPIDevice:
    mov esi, sNoATAPIDeviceFound
    mov bl, LMAGENTA           ; Foreground = Light Magenta
    mov bh, BLACK              ; Background = Black
    call PrintString32
    cli
    hlt
jmp $


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
    ;; call VesaFinish

    ;; Goto Permanent Protected Mode!
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
ProtectedMode:
    ; Update segment registers
    xor eax, eax
    mov ax, DATA_DESC            ; Data segment selector 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

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

jmp $


;times (25*1024) - ($ - $$) db 0
;section .data
; **********
; Variables
; **********
bPhysicalDriveNum 	db	0

WelcomeToStage2 db 'Welcome to the Stage2', 0
sReceivedDriveNumber db 'Received Drive Number in Stage 2: ', 0

sBootloaderName	db	"TheTaaJBoot Version 0.0.1, Author: TheJat", 0

sProtectedModeWelcomeSentence	db	'Entered the Protective Land', 0
sKernelLoadedSentence	db	'Kernel was Loaded', 0
module_name db 'MODULE.ELF', 0  ; Define the string with a null terminator
kernel_name db 'KERNEL/KERNEL.ELF', 0  ; Define the string with a null terminator
										; Now we are in ISO level 3 which supports upto 31 characters for the file identifier
										; If again switching back to ISO level 1, which might be the default, if not specified explicitly.
										; In that case, change the name to 11 characters long, 8 for name and 3 for extension,
										; separated by the dot (.)
sKernelLoadingFailureSentence db 'Failure in loading kernel', 0
sNoATAPIDeviceFound db 'No ATAPI device is found', 0
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

