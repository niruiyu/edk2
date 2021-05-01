/** @file

  Copyright (c) 2014 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiPayloadEntry.h"

VOID  *mHobList = NULL;
/**
  Entry point to the C language phase of UEFI payload.

  @retval      It will not return if SUCCESS, and return error when passing bootloader parameter.
**/
VOID
EFIAPI
_ModuleEntryPoint (
  IN EFI_HOB_HANDOFF_INFO_TABLE *HobList
  )
{
  EFI_STATUS                    Status;
  PHYSICAL_ADDRESS              DxeCoreEntryPoint;
  EFI_HOB_HANDOFF_INFO_TABLE    *HandoffHobTable;
  EFI_PEI_HOB_POINTERS          Hob;
  PLD_EXTRA_DATA                *ExtraData;
  UINT8                         *GuidHob;
  EFI_FIRMWARE_VOLUME_HEADER    *DxeFv;

  //
  // Library constructors rely on mHobList assignment for serial port information retrieval.
  //
  mHobList = (VOID *) HobList;

  ProcessLibraryConstructorList ();

  DEBUG ((DEBUG_INFO, "sizeof(UINTN) = 0x%x\n", sizeof(UINTN)));

  // Initialize floating point operating environment to be compliant with UEFI spec.
  InitializeFloatingPointUnits ();

  // Build HOB based on information from Bootloader
  Status = BuildHobs (HobList);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BuildHobs Status = %r\n", Status));
    return;
  }

  //
  // Get DXE FV location
  //
  GuidHob = GetFirstGuidHob(&gPldExtraDataGuid);
  ASSERT (GuidHob != NULL);
  ExtraData = (PLD_EXTRA_DATA *) GET_GUID_HOB_DATA (GuidHob);
  ASSERT (ExtraData->Count == 1);
  ASSERT (AsciiStrCmp (ExtraData->Entry[0].Identifier, "uefi_fv") == 0);

  DxeFv = (EFI_FIRMWARE_VOLUME_HEADER *) (UINTN) ExtraData->Entry[0].Base;
  ASSERT (DxeFv->FvLength == ExtraData->Entry[0].Size);

  // Load the DXE Core
  Status = LoadDxeCore (DxeFv, &DxeCoreEntryPoint);
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
}
