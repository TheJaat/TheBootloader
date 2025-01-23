%ifndef _STAGE1_STRING_INC_
%define _STAGE1_STRING_INC_

; Stage 1 Information Strings
Stage1WelcomeMessage: db "Welcome to Stage 1", 0
Stage1ActualSizeMessage: db "Actual size of Stage 1 (without padding): ", 0
Stage1PaddedSizeMessage: db "Padded size of Stage 1 (with padding): ", 0

; Error Messages
DiskReadErrorMessage: db "Disk read error!", 0

%endif ; _STAGE1_STRING_INC_

