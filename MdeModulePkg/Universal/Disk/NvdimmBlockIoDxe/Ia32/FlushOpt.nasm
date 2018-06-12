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
;  Flushes a cache line from all the instruction and data caches within the
;  coherency domain of the CPU using "clflushopt" instruction.
;
;  Flushed the cache line specified by LinearAddress, and returns LinearAddress.
;  This function is only available on IA-32 and x64.
;
;  @param  LinearAddress The address of the cache line to flush. If the CPU is
;                        in a physical addressing mode, then LinearAddress is a
;                        physical address. If the CPU is in a virtual
;                        addressing mode, then LinearAddress is a virtual
;                        address.
;
;  @return LinearAddress.
;**/
; VOID *
; EFIAPI
; AsmFlushCacheLineOpt (
;   IN      VOID                      *LinearAddress
;   )
global ASM_PFX(AsmFlushCacheLineOpt)
ASM_PFX(AsmFlushCacheLineOpt):
    mov        eax, [esp + 4]
    clflushopt [eax]
    ret

