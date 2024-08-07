;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2016 - 2022, Intel Corporation. All rights reserved.<BR>
;*   SPDX-License-Identifier: BSD-2-Clause-Patent
;*
;*    CpuAsm.nasm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; SetCodeSelector (
;   UINT16 Selector
;   );
;------------------------------------------------------------------------------
global ASM_PFX(SetCodeSelector)
ASM_PFX(SetCodeSelector):
    push    rcx
    lea     rax, [setCodeSelectorLongJump]
    push    rax
    retfq
setCodeSelectorLongJump:
    ret

;------------------------------------------------------------------------------
; VOID
; SetDataSelectors (
;   UINT16 Selector
;   );
;------------------------------------------------------------------------------
global ASM_PFX(SetDataSelectors)
ASM_PFX(SetDataSelectors):
o16 mov     ss, cx
o16 mov     ds, cx
o16 mov     es, cx
o16 mov     fs, cx
o16 mov     gs, cx
    ret

;    IN VOID   *Buffer
;------------------------------------------------------------------------------
global ASM_PFX(RepStore5000Bytes)
ASM_PFX(RepStore5000Bytes):
    push    rdi

    mov     rax, 0xA5    ; rax = Value
    mov     rdi, rcx   ; rdi = Buffer
    mov     rcx, 5000  ; rcx = Count, rdx = Buffer
    rep     stosb

    pop     rdi
    ret
