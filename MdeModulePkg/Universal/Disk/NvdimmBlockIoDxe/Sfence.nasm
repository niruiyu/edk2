;------------------------------------------------------------------------------
;
; Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------
    SECTION .text


;/**
;  Call "sfense" instruction to serialize load and store operations.
;**/
; VOID
; EFIAPI
; AsmStoreFence (
;   VOID
;   );
global ASM_PFX(AsmStoreFence)
ASM_PFX(AsmStoreFence):
    sfence
    ret
