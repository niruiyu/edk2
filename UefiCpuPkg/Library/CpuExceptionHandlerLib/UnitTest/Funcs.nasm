;; @file
;  Provide read ESP function
;
; Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; AsmReadEsp (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadEsp)
ASM_PFX(AsmReadEsp):
    mov     eax, esp
    ret


;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmWriteEsp (
;   UINT32  Esp
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteEsp)
ASM_PFX(AsmWriteEsp):
    mov     eax, [esp + 4]
    mov     esp, eax
    ret

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmWriteSs (
;   UINT16  Ss
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteSs)
ASM_PFX(AsmWriteSs):
    mov     ax, [esp + 4]
    mov     ss, ax
    ret