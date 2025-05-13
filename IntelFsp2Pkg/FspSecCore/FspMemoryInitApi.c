#include "SecMain.h"
#include "SecFsp.h"
#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/FspMultiPhaseLib.h>
#include <Register/ArchitecturalMsr.h>

//
// Prototype for the common FSP API handler
//

#define FSP_API_INDEX_FSP_MEMORY_INIT      3
#define FSP_API_INDEX_FSP_SILICON_INIT     5
#define FSP_API_INDEX_FSP_MULTIPHASE_SI    6
#define FSP_API_INDEX_FSP_MULTIPHASE_MEM   8

#define FSP_API_INDEX_FSP_MULTIPHASE_MEM   8

#pragma pack(1)

typedef struct {
  UINT16    IdtrLimit;
  UINT32    IdtrBase;
  UINT16    Reserved;
  UINT32    Cr0;
  UINT32    Cr3;
  UINT32    Cr4;
  UINT32    Efer;           // lower 32-bit of EFER since only NXE bit (BIT11) need to be restored.
  UINT32    Registers[8];   // General Purpose Registers: Edi, Esi, Ebp, Esp, Ebx, Edx, Ecx and Eax
  UINT16    Flags[2];
  UINT32    FspInfoHeader;
  UINT32    ApiRet;
  UINT32    ApiParam[2];
} CONTEXT_STACK;


#pragma pack(1)
typedef struct {
  UINT64    Idtr[2];        // IDTR Limit - bit0:bi15, IDTR Base - bit16:bit79
  UINT64    Cr0;
  UINT64    Cr3;
  UINT64    Cr4;
  UINT64    Efer;
  UINT64    Registers[16];  // General Purpose Registers: RDI, RSI, RBP, RSP, RBX, RDX, RCX, RAX, and R15 to R8
  UINT32    Flags[2];
  UINT64    FspInfoHeader;
  UINT64    ApiParam[2];
  UINT64    Reserved;       // The reserved QWORD is needed for stack alignment in X64.
  UINT64    ApiRet;         // 64bit stack format is different from the 32bit one due to x64 calling convention
  BASE_LIBRARY_JUMP_BUFFER JumpBuffer;
} CONTEXT_STACK_64;
#pragma pack()

STATIC_ASSERT (OFFSET_OF (CONTEXT_STACK_64, JumpBuffer) % 8 == 0, "CONTEXT_STACK_64.JumpBuffer must be 8-byte aligned");

VOID
EFIAPI
SecStartup (
  IN UINT32          SizeOfRam,
  IN UINT32          TempRamBase,
  IN VOID            *BootFirmwareVolume,
  IN PEI_CORE_ENTRY  PeiCore,
  IN UINTN           BootLoaderStack,
  IN UINT32          ApiIdx
  );

UINT32 gFspPeiCoreEntryOffset; // to be patched by PatchFv

//
// gFspInfoHeaderRelativeOffset = &LocalGetFspInfoHeader - <FSP_INFO_HEADER>
//
__attribute__((used)) UINT32 gFspInfoHeaderRelativeOffset; // to be patched by PatchFv

__attribute__((used)) FSP_INFO_HEADER *
LocalGetFspInfoHeader (
  VOID
  )
{
  UINTN   AddressOfGetFspInfoHeader;

  AddressOfGetFspInfoHeader = (UINTN) LocalGetFspInfoHeader;
  ASSERT (AddressOfGetFspInfoHeader < MAX_UINT32);

  return (FSP_INFO_HEADER *)(UINTN)(AddressOfGetFspInfoHeader - gFspInfoHeaderRelativeOffset);
}

typedef struct {
  UINT32          SizeOfRam;
  UINT32          TempRamBase;
  VOID            *BootFirmwareVolume;
  PEI_CORE_ENTRY  PeiCore;
  UINTN           BootLoaderStack;
  UINT32          ApiIdx;
} SEC_STARTUP_CONTEXT;

VOID
EFIAPI
SecStartupWrapper (
  SEC_STARTUP_CONTEXT *Context
  )
{
  // Call SecStartup with the context parameters
  SecStartup (
    Context->SizeOfRam,
    Context->TempRamBase,
    Context->BootFirmwareVolume,
    Context->PeiCore,
    Context->BootLoaderStack,
    Context->ApiIdx
  );
}

//
// FspMemoryInitApi
// This function is called after TempRamInit and initializes the memory.
//
EFI_STATUS
EFIAPI
FspMemoryInitApi (
  IN  FSPM_UPD_COMMON_FSP24   *FspmUpdDataPtr,
  OUT VOID                    **FspHobListPtr
  )
{
  FSP_INFO_HEADER       *FspInfoHeader;
  CONTEXT_STACK_64      ContextInBlStack;
  SEC_STARTUP_CONTEXT   SecStartupContext;

  FspInfoHeader = LocalGetFspInfoHeader ();

  ContextInBlStack.FspInfoHeader = (UINT64)(UINTN)FspInfoHeader;
  AsmReadIdtr ((IA32_DESCRIPTOR *)&ContextInBlStack.Idtr);
  ContextInBlStack.Cr0 = AsmReadCr0 ();
  ContextInBlStack.Cr3 = AsmReadCr3 ();
  ContextInBlStack.Cr4 = AsmReadCr4 ();
  ContextInBlStack.Efer = AsmReadMsr64 (MSR_IA32_EFER);
  *(UINT64 *)&ContextInBlStack.Flags = AsmReadEflags ();
  ContextInBlStack.ApiParam[0] = (UINT64)(UINTN)FspmUpdDataPtr;
  ContextInBlStack.ApiParam[1] = (UINT64)(UINTN)FspHobListPtr;

  SecStartupContext.ApiIdx             = 3;
  SecStartupContext.BootFirmwareVolume = (VOID *)(UINTN)FspInfoHeader->ImageBase;
  SecStartupContext.BootLoaderStack    = (UINTN)&ContextInBlStack;
  SecStartupContext.SizeOfRam          = FspmUpdDataPtr->FspmArchUpd.StackSize;
  SecStartupContext.TempRamBase        = FspmUpdDataPtr->FspmArchUpd.StackBase;
  SecStartupContext.PeiCore            = SecStartupContext.BootFirmwareVolume + gFspPeiCoreEntryOffset;

  if (SetJump (&ContextInBlStack.JumpBuffer) == 0) {
    SwitchStack (
      (SWITCH_STACK_ENTRY_POINT) SecStartupWrapper,
      &SecStartupContext,
      NULL,
      (VOID *)(UINTN) (SecStartupContext.TempRamBase + SecStartupContext.SizeOfRam)
    );
  }
  AsmWriteIdtr ((IA32_DESCRIPTOR *)&ContextInBlStack.Idtr);
  AsmWriteCr0 (ContextInBlStack.Cr0);
  AsmWriteCr3 (ContextInBlStack.Cr3);
  AsmWriteCr4 (ContextInBlStack.Cr4);
  AsmWriteMsr64 (MSR_IA32_EFER, ContextInBlStack.Efer);
  return ContextInBlStack.Registers[7];
}

//
// FspSiliconInitApi
//
EFI_STATUS
EFIAPI
FspSiliconInitApi (
  IN  FSPS_UPD_COMMON_FSP24   *FspsUpdDataPtr
  )
{
  FSP_INFO_HEADER       *FspInfoHeader;
  CONTEXT_STACK_64      ContextInBlStack;
  SEC_STARTUP_CONTEXT   SecStartupContext;

  FspInfoHeader = LocalGetFspInfoHeader ();

  ContextInBlStack.FspInfoHeader = (UINT64)(UINTN)FspInfoHeader;
  AsmReadIdtr ((IA32_DESCRIPTOR *)&ContextInBlStack.Idtr);
  ContextInBlStack.Cr0 = AsmReadCr0 ();
  ContextInBlStack.Cr3 = AsmReadCr3 ();
  ContextInBlStack.Cr4 = AsmReadCr4 ();
  ContextInBlStack.Efer = AsmReadMsr64 (MSR_IA32_EFER);
  *(UINT64 *)&ContextInBlStack.Flags = AsmReadEflags ();
  ContextInBlStack.ApiParam[0] = (UINT64)(UINTN)FspsUpdDataPtr;
  ContextInBlStack.ApiParam[1] = 0;

  SecStartupContext.ApiIdx             = 5;
  SecStartupContext.BootFirmwareVolume = (VOID *)(UINTN)FspInfoHeader->ImageBase;
  SecStartupContext.BootLoaderStack    = (UINTN)&ContextInBlStack;
  SecStartupContext.SizeOfRam          = FspsUpdDataPtr->FspsArchUpd.StackSize;
  SecStartupContext.TempRamBase        = FspsUpdDataPtr->FspsArchUpd.StackBase;
  SecStartupContext.PeiCore            = SecStartupContext.BootFirmwareVolume + gFspPeiCoreEntryOffset;

  if (SetJump (&ContextInBlStack.JumpBuffer) == 0) {
    SwitchStack (
      (SWITCH_STACK_ENTRY_POINT) SecStartupWrapper,
      &SecStartupContext,
      NULL,
      (VOID *)(UINTN) (SecStartupContext.TempRamBase + SecStartupContext.SizeOfRam)
    );
  }
  AsmWriteIdtr ((IA32_DESCRIPTOR *)&ContextInBlStack.Idtr);
  AsmWriteCr0 (ContextInBlStack.Cr0);
  AsmWriteCr3 (ContextInBlStack.Cr3);
  AsmWriteCr4 (ContextInBlStack.Cr4);
  AsmWriteMsr64 (MSR_IA32_EFER, ContextInBlStack.Efer);
  return ContextInBlStack.Registers[7];
}

EFI_STATUS
EFIAPI
NotifyPhaseApi (
  IN NOTIFY_PHASE_PARAMS *NotifyPhaseParamPtr
)
{
  CONTEXT_STACK_64      ContextInBlStack;
  CONTEXT_STACK_64 *ContextInFspStack = (CONTEXT_STACK_64 *)GetFspGlobalDataPointer()->CoreStack;
  ContextInBlStack.ApiParam[0] = (UINT64)(UINTN)NotifyPhaseParamPtr;
  ContextInBlStack.ApiParam[1] = 0;
  GetFspGlobalDataPointer()->CoreStack = (UINTN) &ContextInBlStack;
  AsmReadIdtr ((IA32_DESCRIPTOR *)&ContextInBlStack.Idtr);
  ContextInBlStack.Cr0 = AsmReadCr0 ();
  ContextInBlStack.Cr3 = AsmReadCr3 ();
  ContextInBlStack.Cr4 = AsmReadCr4 ();
  ContextInBlStack.Efer = AsmReadMsr64 (MSR_IA32_EFER);
  *(UINT64 *)&ContextInBlStack.Flags = AsmReadEflags ();
  ContextInBlStack.ApiParam[0] = (UINT64)(UINTN)NotifyPhaseParamPtr;
  ContextInBlStack.ApiParam[1] = 0;

  if (SetJump (&ContextInBlStack.JumpBuffer) == 0) {
    LongJump (&ContextInFspStack->JumpBuffer, 1);
  }
  AsmWriteIdtr ((IA32_DESCRIPTOR *)&ContextInBlStack.Idtr);
  AsmWriteCr0 (ContextInBlStack.Cr0);
  AsmWriteCr3 (ContextInBlStack.Cr3);
  AsmWriteCr4 (ContextInBlStack.Cr4);
  AsmWriteMsr64 (MSR_IA32_EFER, ContextInBlStack.Efer);
  return ContextInBlStack.Registers[7];
}

EFI_STATUS
EFIAPI
TempRamExitApi (
  IN VOID  *TempRamExitParam
  )
{
  return EFI_SUCCESS;
}

VOID
EFIAPI
_ModuleEntryPoint (
  VOID
  )
{  
}