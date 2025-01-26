%ifndef __KEYBOARD_INC__
%define __KEYBOARD_INC__

; 16 BITS Code
BITS 16

; ********************************
; GetKeyInputWithBIOS
; IN:
; 	Nothing
; --------------------------------
GetKeyInputWithBIOS:
	pusha
	mov si, sPressAnyKeyBIOSStatement
	call PrintString16BIOS
	
	;; Get key input from the keyboard
	mov ah, 0x00	; Function to read key
	int 0x16		; Wait for key stroke and read
	
	;; when key is pressed
	;; This interrupt would return:
	;; AH = key scan code
	;; AL = ASCII character or zero if special function key.
	
	;; Display the pressed key
	mov ah, 0x0E
	; al already got the pressed key ascii character
	int 0x10
	
	call PrintNewline	; \n
	
	popa
ret
; ********************************


; ********************************
; GetKeyInputWithoutBIOS
; IN:
; 	- Nothing
; --------------------------------
GetKeyInputWithoutBIOS:
	pusha		; save the state
	
	;; Print info string to press any key
	mov si, sPressAnyKeyPortStatement
	call PrintString16BIOS

poll_keyboard:
	in al, 0x64   ; Read status port
	test al, 1    ; Test if the output buffer is full
	jz poll_keyboard ; If not full, continue polling

	in al, 0x60   ; Read scan code from data port
	mov ah, 0     ; Clear AH for the BIOS interrupt call (optional)

	; At this point, AL contains the scan code of the key pressed
	; You can process the scan code here or convert it to ASCII if needed

	; We will just print the scancode in hex.
	mov dx, ax
	call PrintWordHex

	call PrintNewline		; \n
	popa		; restore the saved state
ret
; ********************************


; ********************************
; GetPressedKeyBIOS
; IN:
; 	Nothing
; OUT:
;	AL: ASCII character
;   BL: KEY Scan code
; --------------------------------
GetPressedKeyBIOS:
	
	;; Get key input from the keyboard
	mov ah, 0x00	; Function to read key
	int 0x16		; Wait for key stroke and read
	
	;; when key is pressed
	;; This interrupt would return:
	;; AH = key scan code
	;; AL = ASCII character or zero if special function key.

	; move the scan code into the bl
	mov bl, ah

	;; Display the pressed key
	mov ah, 0x0E
	; al already got the pressed key ascii character
	int 0x10
	
	call PrintNewline	; \n
ret
; ********************************


; ********************************
; check_for_boot_keys
; It waits for the user to press the boot key. It has the loop that
; only breaks when the pressed key is the boot key.
;
; IN:
; 	Nothing
; OUT:
;	AL: ASCII character
; --------------------------------
check_for_boot_keys:
	mov si, sPressSpaceKeyToShowBootMenuStatement
	call PrintString16BIOS
.retry:
	call GetPressedKeyBIOS		; Get the pressed key using the interrupt
								; It returns bl = scancode of the pressed key
	cmp bl, [boot_menu_key_scan_code]		; check if the pressed key is boot key (space key)
	je .gotBootKey

	;; pressed key is not the boot key
	; retry
	mov si, sRetryBootKeyPressStatement
	call PrintString16BIOS
	jmp .retry

.gotBootKey:
	;; pressed key is the boot key
	mov si, sBootKeyPressed
	call PrintString16BIOS
ret

;; variables
boot_menu_key_scan_code: db 0x39	;; SPACE key
sPressAnyKeyBIOSStatement: db 'Press Any Key to Continue: ', 0
sPressAnyKeyPortStatement: db 'Press Any Key to Continue: ', 0

sPressSpaceKeyToShowBootMenuStatement: db 'Press Space Key to Show BootMenu : ', 0
sBootKeyPressed: db 'Boot key is pressed.', 0
sRetryBootKeyPressStatement: db 'Please press the boot key.', 0

%endif
