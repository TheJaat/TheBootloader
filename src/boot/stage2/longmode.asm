;; longmode.asm - the switch from 32-bit protected mode to 64-bit long mode.
;;
;; Linked AFTER stage2.asm, deliberately. Real-mode code in stage2.asm runs
;; with DS = 0, so every label it touches has to stay below linear 0xFFFF.
;; Anything appended to stage2.asm pushes its later labels up towards that
;; limit; code in its own file lands after all of them instead. Nothing here
;; runs in real mode, so its address does not matter.
;;
;; Entered by a jump from stage2.asm with EAX = physical address of the
;; PML4 that prepare_long_mode() built. Never returns.

BITS 32
section .text

global EnterLongMode
extern g_bootInfo           ; longmode.c - the TaajBootInfo for the kernel
extern g_kernelEntry64      ; elf.c      - 64-bit virtual entry point

;; Must match gdt.inc: the 64-bit code descriptor is the sixth entry.
%define CODE64_DESC         0x28
%define DATA_DESC           0x10

%define CR0_PAGING          (1 << 31)
%define CR4_PAE             (1 << 5)
%define MSR_EFER            0xC0000080
%define EFER_LME            (1 << 8)

EnterLongMode:
    cli

    ;; 1. Point CR3 at the page tables. Paging is still off, so nothing
    ;;    changes yet - the CPU only reads CR3 once PG is set.
    mov cr3, eax

    ;; 2. Physical Address Extension: the 64-bit page-table format. Long
    ;;    mode has no other.
    mov eax, cr4
    or  eax, CR4_PAE
    mov cr4, eax

    ;; 3. Ask for long mode in the EFER model-specific register. This only
    ;;    arms it ...
    mov ecx, MSR_EFER
    rdmsr
    or  eax, EFER_LME
    wrmsr

    ;; 4. ... and turning paging on activates it. From this instruction on
    ;;    every address goes through the page tables, which is why they
    ;;    identity-map the code we are executing right now.
    ;;
    ;;    The CPU is now in "compatibility mode": long mode, but still
    ;;    running 32-bit code, because CS is a 32-bit segment.
    mov eax, cr0
    or  eax, CR0_PAGING
    mov cr0, eax

    ;; 5. A far jump through a descriptor with the L bit set is what
    ;;    finally switches instruction decoding to 64-bit.
    jmp CODE64_DESC:LongModeEntry

BITS 64
LongModeEntry:
    ;; Data segments are all but ignored in long mode, but they must hold
    ;; valid selectors.
    mov ax, DATA_DESC
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ;; A temporary stack in low, identity-mapped memory, 16-byte aligned.
    ;; The kernel switches to its own before doing anything else.
    mov rsp, 0x7BF0
    xor rbp, rbp

    ;; The 64-bit entry point is a 64-bit value - read it through a
    ;; register, since this object file can only hold 32-bit relocations.
    mov esi, g_kernelEntry64
    mov rax, [rsi]

    ;; RDI = the TaajBootInfo, the kernel's first argument. Writing EDI
    ;; zero-extends into RDI, and its address is below 1 MiB.
    mov edi, g_bootInfo
    xor esi, esi

    jmp rax
