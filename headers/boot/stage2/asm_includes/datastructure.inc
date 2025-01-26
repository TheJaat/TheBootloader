%ifndef _STAGE_2_DATA_STRUCTURE_INC_
%define _STAGE_2_DATA_STRUCTURE_INC_

BITS 16

;; Declaration of structure
struc DataStructure
	.MemMapAddr	resd	1
	.MemMapLength	resd	1
	.MemoryLo	resd	1
	.MemoryHi	resd	1
	
	
endstruc

;; Initialization of structure
dataStructureObj:
istruc DataStructure
	at DataStructure.MemMapAddr,	dd 0
	at DataStructure.MemMapLength,	dd 0
	at DataStructure.MemoryLo,	dd 0
	at DataStructure.MemoryHi,	dd 0
iend

; It would be passed to the kernel
%define 		MULTIBOOT_MAGIC			0x2BADB002

; Structure Definitions
struc MultiBoot
	.Flags				resd	1
	.MemoryLo			resd	1
	.MemoryHi			resd	1
	.BootDevice			resd	1
	.CmdLine			resd	1
	.ModuleCount			resd	1
	.ModuleAddr			resd	1
	.Symbols0			resd	1
	.Symbols1			resd	1
	.Symbols2			resd	1
	.Symbols3			resd	1
	.MemMapLength			resd	1
	.MemMapAddr			resd	1
	.DrivesLength			resd	1
	.DrivesAddr			resd	1
	.ROMConfigTable			resd	1
	.BootLoaderName			resd	1
	.ApmTable			resd	1
	.VBEControllerInfo		resd	1
	.VBEModeInfo			resd	1
	.VBEMode			resd	1
	.VBEInterfaceSeg		resd	1
	.VBEInterfaceOff		resd	1
	.VBEInterfaceLen		resd	1
endstruc


struc OsBootDescriptor
	.KernelAddr 		resd 	1
	.KernelSize		resd 	1
	.RamDiskAddr		resd 	1
	.RamDiskSize 		resd 	1
	.ExportAddr		resd 	1
	.ExportSize		resd 	1
	.SymbolsAddr		resd	1
	.SymbolsSize		resd	1
endstruc



; ________________________________
;
; MultiBoot Header
; ________________________________
align 32
BootHeader:
istruc MultiBoot
	at MultiBoot.Flags,			dd 0
	at MultiBoot.MemoryLo,			dd 0
	at MultiBoot.MemoryHi,			dd 0
	at MultiBoot.BootDevice,		dd 0
	at MultiBoot.CmdLine,			dd 0
	at MultiBoot.ModuleCount,		dd 0
	at MultiBoot.ModuleAddr,		dd 0
	at MultiBoot.Symbols0,			dd 0
	at MultiBoot.Symbols1,			dd 0
	at MultiBoot.Symbols2,			dd 0
	at MultiBoot.Symbols3, 			dd 0
	at MultiBoot.MemMapLength,		dd 0
	at MultiBoot.MemMapAddr,		dd 0
	at MultiBoot.DrivesLength,		dd 0
	at MultiBoot.DrivesAddr,		dd 0
	at MultiBoot.ROMConfigTable,		dd 0
	at MultiBoot.BootLoaderName,		dd 0
	at MultiBoot.ApmTable,			dd 0
	at MultiBoot.VBEControllerInfo,		dd 0
	at MultiBoot.VBEModeInfo,		dd 0
	at MultiBoot.VBEMode,			dd 0
	at MultiBoot.VBEInterfaceSeg,		dd 0
	at MultiBoot.VBEInterfaceOff,		dd 0
	at MultiBoot.VBEInterfaceLen,		dd 0
iend

; ________________________________
;
; Boot Header for the Kernel
; ________________________________
align 32
BootDescriptor:
istruc OsBootDescriptor
	at OsBootDescriptor.KernelAddr, 		dd 	0
	at OsBootDescriptor.KernelSize,		dd 	0
	at OsBootDescriptor.RamDiskAddr,		dd 	0
	at OsBootDescriptor.RamDiskSize, 		dd 	0
	at OsBootDescriptor.ExportAddr,		dd 	0
	at OsBootDescriptor.ExportSize,		dd 	0
	at OsBootDescriptor.SymbolsAddr,		dd	0
	at OsBootDescriptor.SymbolsSize,		dd	0
iend

%endif
