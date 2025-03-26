; 16 Bit Code, Origin at 0x0
BITS 16
ORG 0x7C00

stage1_start:

jmp main

; Memory Map:
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

%define STAGE2_AREA_SEGMENT	0x0CC0
%define STAGE2_AREA_OFFSET	0x0000;0x0500

%define SUBSYSTEM_MEM_SEGEMENT 0x8000
%define SUBSYSTEM_MEM_OFFSET 0x00
%define FILE_LOADING_AREA_MEM_SEGMENT 0xCC00
%define FILE_LOADING_AREA_MEM_OFFSET 0x0000	; It is the file loading area in memory map

;; Note: In order to access the memory above 0xFFFF, we need to use the combination of
;;	segment: offset, such that by default es is set to 0x00 and with this
;;	we can have offset of from 0x0000 to 0xFFFF. We can't access more than this
;;	with the segment set to 0x0000, which is the limit of every segment window
;;	64 KB. Thus suppose we need to access the 0x10000 which is above the 64 KB
;;	mark of the default segment: offset when segment set to 0x00.
;;	In order to access the 0x10000, we can set segment (like es) to 0x1000 and
;;	offset to 0x00 (like si). Thus es:si = 0x1000:0x0000 = 0x1000*16 + 0x0000
;;	                                                     = 0x10000

; Includes
%include "defines.inc"	; For constants and common variables
%include "utils16/print16.inc"	; For printing functions
%include "disk.inc"		; For disk read function
%include "iso9660.inc"		; For ISO 9660 file system

main:
	; Disable Interrupts, unsafe passage
	cli

	; Far jump to fix segment registers
	jmp 	0x0:FixCS

FixCS:
	; Fix segment registers to 0
	xor 	ax, ax
	mov	ds, ax
	mov	es, ax

	; Set stack
	; The sp register is used to point to the top of the stack.
	; By setting sp to 0x7C00, the bootloader ensures that the
	; stack starts at the top of the memory allocated for the
	; bootloader. This is important because the stack grows
	; downward in memory, so it's set up before any other code runs.
	mov	ss, ax
	mov	ax, 0x7C00	; It ensure that there's space for the stack to
	                ; grow downward without overlapping with other code
					; or any other data in memory.
	mov	sp, ax

	; set interrupts
	sti

	; Save the DL register value, which contains the disk number 
	mov 	byte [bPhysicalDriveNum], dl
	call ClearScreenAndResetCursor	; Clear the screen and reset the cursor

    ;; Get the ARCH flag from the -d compile time flag (symbol)
    %if ARCH == 32
        mov byte [Architecture], 32
    %elif ARCH == 64
        mov byte [Architecture], 64
    %else
        %error "Unsupported hardware"
    %endif

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; Debugging Purpose
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; Print the Register value in hex and decimal
	; mov ax, 1234
	; mov dx, ax
	; call PrintWordHex	; Print dx value in hex
	; call PrintNewline	; \n
	; call PrintWordNumber	; Print ax value in number
	; call PrintNewline	; \n

	; Print Welcome to the Screen
	mov si, WelcomeToStage1		; Load the address of the string into si register
	call PrintString16BIOS		; String printing function.
	call PrintNewline		; \n


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; Calculate and print the actual code size of stage1
	; The actual code is without padding, from start to the right before the
	; ending times statement.
	mov ax, actual_code_end - stage1_start
	mov si, sActualStage1SizeStatement
	call PrintString16BIOS
	call PrintWordNumber
	call PrintNewline

	;; Calculate and print the padded code size of stage1
	; The padded code is with padding, from start to the very end line
	; after the times statement.
	mov ax, stage1_end - stage1_start
	mov si, sPaddedStage1SizeStatement
	call PrintString16BIOS
	call PrintWordNumber
	call PrintNewline
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; calculate and print the size of PVD structure, for debugging purpose only.
	; It should be 2048 bytes
	; mov ax, PrimaryVolumeDescriptor.PVD_End - PrimaryVolumeDescriptor
	; call PrintWordNumber
	; call PrintNewline	; '\n'
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; ISO 9660 things...
	; Read the ISO 9660
	call Read_volume_descriptors
	;; TODO, there should be way to check for the success and failure of above function
	;	on return, because the below functions depends on it.

	; Search for the file whose name is stored at SampleTextFileIdentifier location
	; and load it at FILE_LOADING_AREA_MEM_SEGMENT:FILE_LOADING_AREA_MEM_OFFSET
	; and print its content.

	;; Segment of Root Directory Record (Entry) in es
	xor eax, eax
	mov ax, SUBSYSTEM_MEM_SEGEMENT	;
	mov es, ax
	;; Offset of Root Directory Record (Entry) in si
	mov si, SUBSYSTEM_MEM_OFFSET	; 0x0000

	;; Segment of file load in fs
	xor bx, bx
	mov bx, STAGE2_AREA_SEGMENT	; 0x0000
	mov fs, bx
	;; Offset of file load in di
	mov di, STAGE2_AREA_OFFSET	; 0x0500

	mov ax, Stage2FileIdentifier		; file identifier
	mov cx, Stage2FileIdentifierLength	; file identifier length
	call Find_and_Load_File_from_Root
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	; Load stage from the disk
	; mov dl, [bPhysicalDriveNum]	; Drive number
	; mov ch, 0		; Cylinder number
	; mov dh, 0		; Head number
	; mov cl, 2		; Sector starting (Indexed 1, as first sector is at index 1)
	; mov ax, 0x0000
	; mov es, ax
	; mov bx, STAGE_2_LOAD_ADDRESS		; Memory address (0x500)
	; mov al, STAGE_2_SECTORS_COUNT	; 58 Number of sectors to read
	; call ReadFromDisk	; Call the routine to read from disk

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; Pass Data from stage 1 to stage 2,
	;; through register
	; mov ax, 77
	; Call PrintWordNumber
	; call PrintNewline	; \n
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; Pass Data from stage 1 to stage 2,
	;; through known fixed memory location
	;; For our example, we will pass the data by storing
	;; at memory "0x7E00" which is just after the bootcode.
	; mov ax, 1628
	; mov word [0x7E00], ax		; Store the passing value at the location,
					; by specifying size explicitly.
	;; OR
	;; mov [0x7E00], ax		; Store the passing value at the location
					; Without specifying size explicitly, the assembler
					; interprets it as to store the data of size of AX
					; which is word size.
	;; These both methods works, as AX is of word size,
	;; so assembler only stores word size data at the location.
	; call PrintWordNumber		; Print the Passing data
	; call PrintNewline		; \n
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; Pass Data from stage 1 to stage 2 Using Stack
	;; We will push the data in our stage 1
	;; and pop the data from in our stage 2
	; mov ax, 107	; Set AX to the value want to pass by pushing
	; call PrintWordNumber	; Print the passing value
	; call PrintNewline	; \n
	
	; push ax		; Push AX, which is 107 on the stack
	
	; mov ax, 108	        ; Change AX to 108
	; call PrintWordNumber	; Print Changed value
	; Call PrintNewline	; \n
	
	; push ax		; Push AX, which is 108 on the stack
	
	;; Here we have passed the value 107 and 108 in the stack,
	;; such that stack is:
	;;	|	    | Top (Low Memory Area)
	;;	|  108	|
	;;	|-------|
	;;	|  107	| Bottom (High Memory Area)
	;;	---------
	;; At receiving end they will be received in reverse order.
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; Pass drive number to stage 2
	;; we will pass it in register AL, fast and easy
	;; method for small data
	xor ax, ax		            ; For printing clear complete AX
	mov al, [bPhysicalDriveNum]	; Put drive number in al to be passed.
	mov si, sPassedDriveNumber
	call PrintString16BIOS
	call PrintWordNumber	; Print the passing drive number
	call PrintNewline	; \n
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	
	;; Jump to the loaded binary stage2
	; jump to the stage 2 land
	jmp STAGE2_AREA_SEGMENT:STAGE2_AREA_OFFSET	; 0x0000:0x0500

	; Infinite loop
	jmp $


; **************************
; Variables
; **************************
bPhysicalDriveNum	db	0	; Define variable to store disk number	

WelcomeToStage1	db 'Welcome to the Stage1', 0	; Define welcome message
sPassedDriveNumber db	'Passed Drive Number from Stage1 : ', 0
sActualStage1SizeStatement db 'Actual size of the stage1 code (without padding in bytes): ', 0
sPaddedStage1SizeStatement db 'Padded size of the stage1 code (with padding in bytes): ', 0

;; Sample Text File
SampleTextFileIdentifier: db 'AB.TXT', 0
SampleTextFileIdentifierLength equ $ - SampleTextFileIdentifier - 1	; -1 is for the null terminator 

;; Stage 2 Bin File
Stage2FileIdentifier: db 'STAGE2.BIN', 0
Stage2FileIdentifierLength equ $ - Stage2FileIdentifier - 1	; -1 is for the null terminator 

; Fill out bootloader
;times 510-($-$$) db 0		; Fill up the remaining space with zeroes

; Boot Signature
;db 0x55, 0xAA		; Boot signature indicating valid bootloader

; Now we are in ISO 9660, so its sector size is 2048 bytes (2KB)
;times 2048 - ($ - $$) db 0

Architecture: db 0


PrimaryVolumeDescriptor:
    .PVD_Type               db 0                  ; 1 byte: Volume Descriptor Type
    .PVD_StandardIdentifier db 5 dup(0)           ; 5 bytes: Standard Identifier (CD001)
    .PVD_Version            db 0                  ; 1 byte: Volume Descriptor Version
    .PVD_Unused1            db 1 dup(0)           ; 1 byte: Unused Field
    .PVD_SystemIdentifier   db 32 dup(0)          ; 32 bytes: System Identifier
    .PVD_VolumeIdentifier   db 32 dup(0)          ; 32 bytes: Volume Identifier
    .PVD_Unused2            db 8 dup(0)           ; 8 bytes: Unused Field
    .PVD_VolumeSpaceSize    dd 0                  ; 4 bytes: Volume Space Size (little-endian)
    .PVD_VolumeSpaceSizeBE  dd 0                  ; 4 bytes: Volume Space Size (big-endian)
    .PVD_Unused3            db 32 dup(0)          ; 32 bytes: Unused Field
    .PVD_VolumeSetSize      dw 0                  ; 2 bytes: Volume Set Size (little-endian)
    .PVD_VolumeSetSizeBE    dw 0                  ; 2 bytes: Volume Set Size (big-endian)
    .PVD_VolumeSequenceNumber dw 0                ; 2 bytes: Volume Sequence Number (little-endian)
    .PVD_VolumeSequenceNumberBE dw 0              ; 2 bytes: Volume Sequence Number (big-endian)
    .PVD_LogicalBlockSize   dw 0                  ; 2 bytes: Logical Block Size (little-endian)
    .PVD_LogicalBlockSizeBE dw 0                  ; 2 bytes: Logical Block Size (big-endian)
    .PVD_PathTableSize      dd 0                  ; 4 bytes: Path Table Size (little-endian)
    .PVD_PathTableSizeBE    dd 0                  ; 4 bytes: Path Table Size (big-endian)
    .PVD_LocTypeLPathTable  dd 0                  ; 4 bytes: Location of Type L Path Table (little-endian)
    .PVD_LocOptionalTypeLPathTable dd 0           ; 4 bytes: Location of Optional Type L Path Table (little-endian)
    .PVD_LocTypeMPathTable  dd 0                  ; 4 bytes: Location of Type M Path Table (big-endian)
    .PVD_LocOptionalTypeMPathTable dd 0           ; 4 bytes: Location of Optional Type M Path Table (big-endian)
    .PVD_DirectoryEntry     db 34 dup(0)          ; 34 bytes: Directory Entry for Root Directory
    .PVD_VolumeSetIdentifier db 128 dup(0)        ; 128 bytes: Volume Set Identifier
    .PVD_PublisherIdentifier db 128 dup(0)        ; 128 bytes: Publisher Identifier
    .PVD_DataPreparerIdentifier db 128 dup(0)     ; 128 bytes: Data Preparer Identifier
    .PVD_ApplicationIdentifier db 128 dup(0)      ; 128 bytes: Application Identifier
    .PVD_CopyrightFileIdentifier db 37 dup(0)     ; 37 bytes: Copyright File Identifier
    .PVD_AbstractFileIdentifier db 37 dup(0)      ; 37 bytes: Abstract File Identifier
    .PVD_BibliographicFileIdentifier db 37 dup(0) ; 37 bytes: Bibliographic File Identifier
    .PVD_VolumeCreationDate db 17 dup(0)          ; 17 bytes: Volume Creation Date and Time
    .PVD_VolumeModificationDate db 17 dup(0)      ; 17 bytes: Volume Modification Date and Time
    .PVD_VolumeExpirationDate db 17 dup(0)        ; 17 bytes: Volume Expiration Date and Time
    .PVD_VolumeEffectiveDate db 17 dup(0)         ; 17 bytes: Volume Effective Date and Time
    .PVD_FileStructureVersion db 0                ; 1 byte: File Structure Version
    .PVD_Reserved1          db 1 dup(0)           ; 1 byte: Reserved Field
    .PVD_ApplicationUse     db 512 dup(0)         ; 512 bytes: Application Use
    .PVD_Reserved2          db 653 dup(0)         ; 653 bytes: Reserved for future standardization
    ; End of structure
    .PVD_End                db 0                  ; End marker (not part of the ISO 9660 standard, added for structure alignment)

actual_code_end:	; After this there is padding only

times 20480 - ($ - $$) db 0	; 20 KB padding
stage1_end:	; Ending with padding

