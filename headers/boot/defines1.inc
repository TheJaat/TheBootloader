%ifndef _COMMON_DEFINES_INC_
%define _COMMON_DEFINES_INC_


;;********************************************
;; Stage 1 and Stage 2 Constants
;;********************************************
%define          STAGE_2_LOAD_ADDRESS         0x0500
%define          STAGE_2_SIZE                 30207    ; 0x7AFF - 0x500 = 0x75FF = 30,207
%define	         STAGE_2_SECTORS_COUNT	STAGE_2_SIZE/512        ; 30,207 / 512(size of 1 sector) = 58

%define          MEMLOCATION_MEMORY_MAP	                0x9000


%endif ; _COMMON_DEFINES_INC_

