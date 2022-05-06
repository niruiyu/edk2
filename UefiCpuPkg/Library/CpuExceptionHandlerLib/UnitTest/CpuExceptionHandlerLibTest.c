/** @file
  CPU PEI Module installs CPU Multiple Processor PPI.

  Copyright (c) 2015 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Ppi/VectorHandoffInfo.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/CpuExceptionHandlerLib.h>

EFI_VECTOR_HANDOFF_INFO  mHookTest[] = {
  {
    3,           // INT3
    EFI_VECTOR_HANDOFF_HOOK_AFTER,
    { 0 }
  },
  {
    15,          // GP fault
    EFI_VECTOR_HANDOFF_HOOK_BEFORE,
    { 0 }
  },
  {
    0,
    EFI_VECTOR_HANDOFF_LAST_ENTRY,
    { 0 }
  }
};

#pragma warning (disable: 4717 4101 4740)
#pragma optimize ("", off)
VOID
StackOverflow (
  UINTN   Level
  )
{
  UINT8  Local[SIZE_1KB];
  DEBUG ((DEBUG_ERROR, "%a: Level = %d\n", __FUNCTION__, Level));
  StackOverflow (Level + 1);
}
#pragma optimize ("", on)

/**
  The Entry point of the PEIM.

  This function will wakeup APs and collect CPU AP count and install the
  Mp Service Ppi.

  @param  FileHandle    Handle of the file being invoked.
  @param  PeiServices   Describes the list of possible PEI Services.

  @retval EFI_SUCCESS   MpServicePpi is installed successfully.

**/
EFI_STATUS
CpuExceptionHandlerLibTestPeimInit (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                       Status;

  StackOverflow (0);

  Status = InitializeCpuExceptionHandlers (mHookTest);
  ASSERT_EFI_ERROR (Status);
  CpuBreakpoint ();

  AsmReadMsr64 (0xFFFFFFFF);
  CpuDeadLoop ();

  return Status;
}
