
;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmStoreFence (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmStoreFence)
ASM_PFX(AsmStoreFence):
    sfence
    ret
