/** @file

  Copyright (c) 2014 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiPayloadEntry.h"

extern VOID  *mHobList;
/**
  Entry point to the C language phase of UEFI payload.

  @retval      It will not return if SUCCESS, and return error when passing bootloader parameter.
**/
EFI_STATUS
EFIAPI
PayloadEntry (
  IN UINTN                     BootloaderParameter
  )
{
  EFI_STATUS                    Status;
  PHYSICAL_ADDRESS              DxeCoreEntryPoint;
  EFI_HOB_HANDOFF_INFO_TABLE    *HandoffHobTable;
  EFI_PEI_HOB_POINTERS          Hob;
  UINTN                         ImageBase;
  PLD_IMAGE_BASE_HOB            *PldImageBaseHob;
  UINT8                         *GuidHob;

  // Call constructor for all libraries
  mHobList = (VOID *) BootloaderParameter;

  ProcessLibraryConstructorList ();

  DEBUG ((DEBUG_INFO, "sizeof(UINTN) = 0x%x\n", sizeof(UINTN)));

  // Initialize floating point operating environment to be compliant with UEFI spec.
  InitializeFloatingPointUnits ();

  // Build HOB based on information from Bootloader
  Status = BuildHobs (BootloaderParameter);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BuildHobs Status = %r\n", Status));
    return Status;
  }

  // Get Payload Image Base
  GuidHob = GetFirstGuidHob(&gPldImageBaseGuid);
  ASSERT (GuidHob != NULL);
  PldImageBaseHob = (PLD_IMAGE_BASE_HOB *) GET_GUID_HOB_DATA (GuidHob);
  ASSERT (PldImageBaseHob != NULL);
  ImageBase = (UINTN) PldImageBaseHob->Base;
  ASSERT (PldImageBaseHob->Base == (UINT64) ImageBase);

  // Load the DXE Core
  Status = LoadDxeCore (ImageBase, &DxeCoreEntryPoint);
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_INFO, "DxeCoreEntryPoint = 0x%lx\n", DxeCoreEntryPoint));

  //
  // Mask off all legacy 8259 interrupt sources
  //
  IoWrite8 (LEGACY_8259_MASK_REGISTER_MASTER, 0xFF);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_SLAVE,  0xFF);

  HandoffHobTable = (EFI_HOB_HANDOFF_INFO_TABLE *) GetFirstHob(EFI_HOB_TYPE_HANDOFF);
  Hob.HandoffInformationTable = HandoffHobTable;
  HandOffToDxeCore (DxeCoreEntryPoint, Hob);

  // Should not get here
  CpuDeadLoop ();
  return EFI_SUCCESS;
}
