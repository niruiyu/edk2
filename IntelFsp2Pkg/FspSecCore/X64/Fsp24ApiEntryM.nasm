;; @file
;  Provide FSP API entry points.
;
; Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;;
    DEFAULT REL
    SECTION .text

;
; Following functions will be provided in PlatformSecLib
;
extern ASM_PFX(AsmGetFspBaseAddress)
extern ASM_PFX(AsmGetFspInfoHeader)
extern ASM_PFX(FspMultiPhaseMemInitApiHandler)

;----------------------------------------------------------------------------
; FspMultiPhaseMemoryInitApi API
;
; This FSP API provides multi-phase Memory initialization, which brings greater
; modularity beyond the existing FspMemoryInit() API.
; Increased modularity is achieved by adding an extra API to FSP-M.
; This allows the bootloader to add board specific initialization steps throughout
; the MemoryInit flow as needed.
;

global ASM_PFX(FspPeiCoreEntryOff)
ASM_PFX(FspPeiCoreEntryOff):
   ;
   ; This value will be patched by the build script
   ;
   DD    0x12345678
