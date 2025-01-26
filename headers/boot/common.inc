%ifndef _COMMON_INC_
%define _COMMON_INC_

; ********************************
; SystemFail
; Only call if fatal error
; ********************************
SystemFail:
	int     0x16                    ; await keypress
	int     0x19                    ; warm boot computer

	; No return from this
	cli
	hlt

%endif
