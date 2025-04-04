%ifndef _COMMON_DEFINES_INC_
%define _COMMON_DEFINES_INC_

%define		STAGE_2_LOAD_ADDRESS	0x0500
%define		STAGE_2_SIZE			30207	            ; 0x7AFF - 0x500 = 0x75FF = 30,207
%define		STAGE_2_SECTORS_COUNT	STAGE_2_SIZE/512	; 30,207 / 512(size of 1 sector) = 58

%define 	MEMLOCATION_MEMORY_MAP			0x9000

;; Kernel Loading Address
%define 	MEMLOCATION_KERNEL_LOAD_SEGMENT			0x0000
%define 	MEMLOCATION_KERNEL_LOAD_OFFSET			0xB000

%endif
