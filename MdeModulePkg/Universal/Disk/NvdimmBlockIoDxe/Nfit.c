/** @file

  Provide NFIT parsing functions.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "NvdimmBlockIoDxe.h"

EFI_GUID  gNvdimmControlRegionGuid          = EFI_ACPI_6_0_NFIT_GUID_NVDIMM_CONTROL_REGION;
EFI_GUID  gNvdimmPersistentMemoryRegionGuid = EFI_ACPI_6_0_NFIT_GUID_BYTE_ADDRESSABLE_PERSISTENT_MEMORY_REGION;
EFI_GUID  gNvdimmBlockDataWindowRegionGuid  = EFI_ACPI_6_0_NFIT_GUID_NVDIMM_BLOCK_DATA_WINDOW_REGION;

BOOLEAN
ValidateSpa (
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *Spa
)
{
  RETURN_STATUS        RStatus;
  UINT64               Result;
  if (Spa->Length != sizeof (*Spa)) {
    return FALSE;
  }
  if (Spa->SPARangeStructureIndex == 0) {
    return FALSE;
  }
  if (Spa->SystemPhysicalAddressRangeBase == MAX_UINT64) {
    return FALSE;
  }
  RStatus = SafeUint64Add (Spa->SystemPhysicalAddressRangeBase, Spa->SystemPhysicalAddressRangeLength, &Result);
  if (RETURN_ERROR (RStatus)) {
    return FALSE;
  }
  return TRUE;
}

BOOLEAN
ValidateMap (
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map
)
{
  RETURN_STATUS                     Status;
  UINT64                            Result;
  if (Map->Length != sizeof (*Map)) {
    return FALSE;
  }
  if (Map->NVDIMMControlRegionStructureIndex == 0) {
    return FALSE;
  }
  if ((Map->InterleaveStructureIndex != 0) && (Map->InterleaveWays <= 1)) {
    return FALSE;
  }
  Status = SafeUint64Add (Map->RegionOffset, Map->MemoryDeviceRegionSize, &Result);
  if (RETURN_ERROR (Status)) {
    return FALSE;
  }
  return TRUE;
}

BOOLEAN
ValidateInterleave (
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE *Interleave
)
{
  if (Interleave->Length < sizeof (*Interleave)) {
    return FALSE;
  }
  if (Interleave->InterleaveStructureIndex == 0) {
    return FALSE;
  }
  if (Interleave->NumberOfLines == 0) {
    return FALSE;
  }
  if (Interleave->Length != sizeof (*Interleave) + Interleave->NumberOfLines * sizeof (UINT32)) {
    return FALSE;
  }
  return TRUE;
}

BOOLEAN
ValidateSmbios (
  EFI_ACPI_6_0_NFIT_SMBIOS_MANAGEMENT_INFORMATION_STRUCTURE *Smbios
)
{
  if (Smbios->Length < sizeof (*Smbios)) {
    return FALSE;
  }
  return TRUE;
}

BOOLEAN
ValidateControl (
  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE *Control
)
{
  if (Control->Length != sizeof (*Control)) {
    return FALSE;
  }
  if (Control->NVDIMMControlRegionStructureIndex == 0) {
    return FALSE;
  }
  return TRUE;
}

BOOLEAN
ValidateDataWindow (
  EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE *DataWindow
)
{
  if (DataWindow->Length != sizeof (*DataWindow)) {
    return FALSE;
  }
  if (DataWindow->NVDIMMControlRegionStructureIndex == 0) {
    return FALSE;
  }
  return TRUE;
}

BOOLEAN
ValidateHintAddress (
  EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE *HintAddress
)
{
  if (HintAddress->Length < sizeof (*HintAddress)) {
    return FALSE;
  }
  if (HintAddress->Length != sizeof (*HintAddress) + HintAddress->NumberOfFlushHintAddresses * sizeof (UINT64)) {
    return FALSE;
  }
  return TRUE;
}


typedef
BOOLEAN
(*VALIDATE_NFIT_STRUCTURE) (
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *NfitStruc
  );

VALIDATE_NFIT_STRUCTURE mValidateNfitStruc[] = {
  ValidateSpa,
  ValidateMap,
  ValidateInterleave,
  ValidateSmbios,
  ValidateControl,
  ValidateDataWindow,
  ValidateHintAddress
};

/**
  This function find ACPI table with the specified signature in RSDT or XSDT.

  @param Sdt              ACPI RSDT or XSDT.
  @param Signature        ACPI table signature.
  @param TablePointerSize Size of table pointer: 4 or 8.

  @return ACPI table or NULL if not found.
**/
VOID *
ScanTableInSDT (
  IN EFI_ACPI_DESCRIPTION_HEADER    *Sdt,
  IN UINT32                         Signature,
  IN UINTN                          TablePointerSize
  )
{
  UINTN                          Index;
  UINTN                          EntryCount;
  UINTN                          EntryBase;
  EFI_ACPI_DESCRIPTION_HEADER    *Table;

  EntryCount = (Sdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / TablePointerSize;

  EntryBase = (UINTN)(Sdt + 1);
  for (Index = 0; Index < EntryCount; Index++) {
    //
    // When TablePointerSize is 4 while sizeof (VOID *) is 8, make sure the upper 4 bytes are zero.
    //
    Table = 0;
    CopyMem (&Table, (VOID *)(EntryBase + Index * TablePointerSize), TablePointerSize);

    if (Table == NULL) {
      continue;
    }

    if (Table->Signature == Signature) {
      return Table;
    }
  }

  return NULL;
}

/**
  Locate the NFIT ACPI structure in the ACPI table.

  @return  The NFIT ACPI structure.
**/
EFI_ACPI_6_0_NVDIMM_FIRMWARE_INTERFACE_TABLE *
LocateNfit (
  VOID
  )
{
  EFI_STATUS                                    Status;
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp;
  EFI_ACPI_6_0_NVDIMM_FIRMWARE_INTERFACE_TABLE  *Nfit;
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, (VOID **)&Rsdp);
  if (EFI_ERROR (Status)) {
    Status = EfiGetSystemConfigurationTable (&gEfiAcpi10TableGuid, (VOID **)&Rsdp);
  }

  if (EFI_ERROR (Status) || (Rsdp == NULL)) {
    return NULL;
  }
  Nfit = NULL;

  //
  // Find FADT in XSDT
  //
  if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION && Rsdp->XsdtAddress != 0) {
    Nfit = ScanTableInSDT (
      (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress,
      EFI_ACPI_6_0_NVDIMM_FIRMWARE_INTERFACE_TABLE_STRUCTURE_SIGNATURE,
      sizeof (UINTN)
    );
  }

  //
  // Find NFIT in RSDT
  //
  if (Nfit == NULL && Rsdp->RsdtAddress != 0) {
    Nfit = ScanTableInSDT (
      (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->RsdtAddress,
      EFI_ACPI_6_0_NVDIMM_FIRMWARE_INTERFACE_TABLE_STRUCTURE_SIGNATURE,
      sizeof (UINT32)
    );
  }

return Nfit;
}

/**
  Locate the NFIT structure by the structure index.

  @param NfitStrucs           The NFIT structures array.
  @param NfitStrucCount       Number of the NFIT structures.
  @param StructureIndexOffset The structure index offset.
  @param SructureIndex        Structure index to look for.

  @return NULL or the NFIT structure with the specified structure index.
**/
EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *
LocateNfitStrucByIndex (
  IN EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs,
  IN UINTN                              NfitStrucCount,
  IN UINTN                              StructureIndexOffset,
  IN UINT16                             SructureIndex
  )
{
  UINTN                              Index;
  for (Index = 0; Index < NfitStrucCount; Index++) {
    if (*(UINT16 *)((UINT8 *)NfitStrucs[Index] + StructureIndexOffset) == SructureIndex) {
      return NfitStrucs[Index];
    }
  }

  return NULL;
}

/**
  Locate the NFIT structure by the device handle.

  @param NfitStrucs           The NFIT structures array.
  @param NfitStrucCount       Number of the NFIT structures.
  @param DeviceHandleOffset   The device handle offset.
  @param DeviceHandle         The device handle.

  @return NULL or the NFIT structure with the specified device handle.
**/
EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *
LocateNfitStrucByDeviceHandle (
  IN EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs,
  IN UINTN                              NfitStrucCount,
  IN UINTN                              DeviceHandleOffset,
  IN EFI_ACPI_6_0_NFIT_DEVICE_HANDLE    *DeviceHandle
  )
{
  UINTN                              Index;
  for (Index = 0; Index < NfitStrucCount; Index++) {
    if (CompareMem ((UINT8 *)NfitStrucs[Index] + DeviceHandleOffset, DeviceHandle,
      sizeof (EFI_ACPI_6_0_NFIT_DEVICE_HANDLE)) == 0) {
      return NfitStrucs[Index];
    }
  }

  return NULL;
}

/**
  Convert the region offset to system physical address.

   @param RegionOffset  The region offset.
   @param Spa           The system physical address range structure.
   @param Map           The map structure.
   @param Interleave    The optional interleave structure.
   @param Address       Return the system physical address.

   @retval RETURN_BUFFER_TOO_SMALL The conversion fails.
   @retval RETURN_SUCCESS          The conversion succeeds.
**/
RETURN_STATUS
DeviceRegionOffsetToSpa (
  IN  UINT64                                                                RegionOffset,
  IN  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *Spa,
  IN  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map,
  IN  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave, OPTIONAL
  OUT UINT8                                                                 **Address
)
{
  RETURN_STATUS Status;
  UINT64 RotationSize;
  UINT64 RotationNum;
  UINT64 Uint64;
  UINT64 Remainder;
  UINT64 LineNum;
  UINT32 Offset;
  UINT64 StartAddress;

  ASSERT (Spa != NULL);
  ASSERT (Map != NULL);
  ASSERT (Address != NULL);

  StartAddress = Spa->SystemPhysicalAddressRangeBase + Map->RegionOffset;

  //
  //  Offset  NVDIMM#0         NVDIMM#1
  //  0x0000 ++++++++++       ++++++++++
  //  0x4000 ++++++++++       ++++++++++
  //  0x8000 ++++++++++       ++++++++++

  //  0xC000 ++++++++++       ++++++++++
  // 0x10000 ++++++++++       ++++++++++
  // 0x14000 ++++++++++       ++++++++++

  // 0x18000 ++++++++++       ++++++++++
  // 0x1C000 +x++++++++       ++++++++++
  // 0x20000 ++++++++++       ++++++++++
  //
  // NumberOfLines = 3
  // LineSize = 0x1000
  // LineOffset[3] = { 0, 8, 0x10}
  // Interleave = 2
  // RegionSize = RatationSize * 3
  //
  // RegionOffset (position of x) = 0x1C400
  //
  if (Interleave != NULL) {
    ASSERT ((Interleave->LineSize != 0) && (Interleave->NumberOfLines != 0));
    //
    // RatationSize = LineSize * NumberOfLines = 0x1000 * 3
    //
    RotationSize = MultU64x32 (Interleave->LineSize, Interleave->NumberOfLines);
    //
    // RotationNum = RegionOffset / RotationSize = 0x1C400 / (0x1000 * 3) = 9 MOD 0x1400
    // Remainder   = 0x1400
    //
    RotationNum = DivU64x64Remainder (RegionOffset, RotationSize, &Remainder);
    //
    // LineNum = Remainder / LineSize = 0x1400 / 0x1000 = 1 MOD 0x400
    // Offset  = 0x400
    //
    LineNum = DivU64x32Remainder (Remainder, Interleave->LineSize, &Offset);
    if (LineNum > MAX_UINTN) {
      return RETURN_BUFFER_TOO_SMALL;
    }

    //
    // *Address = StartAddress
    //   + RotationNum * RotationSize * Map->InterleaveWays
    //   + Interleave->LineOffset[LineNum] * Interleave->LineSize
    //   + RegionOffset % Interleave->LineSize;
    // NOTE: RotationNum * RotationSize won't exceed MAX_UINT64 because:
    //   RotationNum = (RegionOffset - Remainder) / RotationSize
    //
    //          = StartAddress
    //   + 9 * 0x3000 * 2 + 8 * 0x1000 + 0x400
    //          = StartAddres + 0x3E400

    Status = SafeUint64Mult (MultU64x64 (RotationNum, RotationSize), Map->InterleaveWays, &Uint64);
    if (RETURN_ERROR (Status)) {
      return Status;
    }
    Status = SafeUint64Add (StartAddress, Uint64, &StartAddress);
    if (RETURN_ERROR (Status)) {
      return Status;
    }
    Uint64 = MultU64x32 (Interleave->LineOffset[(UINTN)LineNum], Interleave->LineSize);
    Status = SafeUint64Add (StartAddress, Uint64, &StartAddress);
    if (RETURN_ERROR (Status)) {
      return Status;
    }
    Status = SafeUint64Add (StartAddress, Offset, &StartAddress);
  } else {
    Status = SafeUint64Add (StartAddress, RegionOffset, &StartAddress);
  }
  if (RETURN_ERROR (Status)) {
    return Status;
  }
  if (StartAddress > MAX_UINTN) {
    return RETURN_BUFFER_TOO_SMALL;
  }
  *Address = (UINT8 *)(UINTN)StartAddress;
  return RETURN_SUCCESS;
}

/**
  Free the NFIT structures.
**/
VOID
FreeNfitStructs (
  VOID
)
{
  UINTN Index;
  for (Index = 0; Index < ARRAY_SIZE (mPmem.NfitStrucs); Index++) {
    if (mPmem.NfitStrucs[Index] != NULL) {
      FreePool (mPmem.NfitStrucs[Index]);
    }
  }
  ZeroMem (mPmem.NfitStrucs, sizeof (mPmem.NfitStrucs));
  ZeroMem (mPmem.NfitStrucCount, sizeof (mPmem.NfitStrucCount));
}

/**
  Parse the NFIT ACPI structures to create all the NVDIMM instances.

  @retval EFI_SUCCESS           The NFIT ACPI structures are valid and all NVDIMM instances are created.
  @retval EFI_NOT_FOUND         There is no NFIT ACPI structure.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource to parse the NFIT ACPI structures.
  @retval EFI_INVALID_PARAMETER The NFIT ACPI structures are invalid.
**/
EFI_STATUS
ParseNfit (
  VOID
)
{
  RETURN_STATUS                                                         RStatus;
  UINTN                                                                 Result;
  EFI_STATUS                                                            Status;
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER                                    *NfitStruc;
  UINTN                                                                 Index;
  UINTN                                                                 NfitStrucCount[ARRAY_SIZE (mPmem.NfitStrucCount)];
  EFI_ACPI_6_0_NVDIMM_FIRMWARE_INTERFACE_TABLE                          *Nfit;
  NVDIMM                                                                *Nvdimm;
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *Spa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map;
  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *Control;
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave;
  UINTN                                                                 SpaIndex;
  UINTN                                                                 MapIndex;

  Status = EFI_SUCCESS;
  Nfit = LocateNfit ();
  if (Nfit == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to find NFIT.\n"));
    return EFI_NOT_FOUND;
  }

  //
  // Validate the NFIT length.
  //
  RStatus = SafeUintnAdd ((UINTN)Nfit, Nfit->Header.Length, &Result);
  if (RETURN_ERROR (RStatus)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Count all NFIT structures using local variables.
  //
  ZeroMem (NfitStrucCount, sizeof (NfitStrucCount));
  for (NfitStruc = (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)(Nfit + 1)
    ; NfitStruc < (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)((UINT8 *)Nfit + Nfit->Header.Length)
    ; NfitStruc = (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)((UINT8 *)NfitStruc + NfitStruc->Length)
    ) {
    if ((NfitStruc->Type >= ARRAY_SIZE (NfitStrucCount)) || (NfitStruc->Length >= Nfit->Header.Length)) {
      return EFI_INVALID_PARAMETER;
    }
    if (!mValidateNfitStruc[NfitStruc->Type] (NfitStruc)) {
      return EFI_INVALID_PARAMETER;
    }
    NfitStrucCount[NfitStruc->Type]++;
  }

  //
  // Allocate spaces for NFIT structures
  //
  for (Index = 0; Index < ARRAY_SIZE (mPmem.NfitStrucs); Index++) {
    mPmem.NfitStrucs[Index] = AllocatePool (sizeof (VOID *) * NfitStrucCount[Index]);
    if (mPmem.NfitStrucs[Index] == NULL) {
      FreeNfitStructs ();
      return EFI_OUT_OF_RESOURCES;
    }
  }

  //
  // Collect all the NFIT structures
  //
  for (NfitStruc = (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)(Nfit + 1)
    ; NfitStruc < (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)((UINT8 *)Nfit + Nfit->Header.Length)
    ; NfitStruc = (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)((UINT8 *)NfitStruc + NfitStruc->Length)
    ) {
    if (NfitStruc->Type < ARRAY_SIZE (mPmem.NfitStrucs)) {
      mPmem.NfitStrucs[NfitStruc->Type][mPmem.NfitStrucCount[NfitStruc->Type]] = NfitStruc;
      mPmem.NfitStrucCount[NfitStruc->Type]++;
    }
  }

  //
  // Find the corresponding Map structures for a (PM/Control) SPA structure.
  // Find the corresponding Control Region structure for (Control) SPA structures.
  // For PM:
  //   1 SPA : m Map : m Control Region (?)     : m FlushHint
  // For BLK:
  //   1 SPA : n Map : n Control Region         : n FlushHint
  //   1 SPA : n Map : n BW Region              : n FlushHint
  //
  // For a NVDIMM which contains 1 PM:
  //   SpaPm        -   MapPm        -      Control
  //             SpaIndex       RegionIndex
  // For a NVDIMM which contains 1 BLK:
  //   SpaControl   -   MapControl   -      Control     -     BW        -     MapBw   -    SpaBw
  //             SpaIndex       RegionIndex         RegionIndex   RegionIndex      SpaIndex2
  //
  for (SpaIndex = 0; SpaIndex < mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE_TYPE]; SpaIndex++) {
    Spa = (EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *)mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE_TYPE][SpaIndex];
    if (!CompareGuid (&Spa->AddressRangeTypeGUID, &gNvdimmPersistentMemoryRegionGuid) && !CompareGuid (&Spa->AddressRangeTypeGUID, &gNvdimmControlRegionGuid)) {
      continue;
    }

    //
    // Use for-loop instead of LocateNfitStrucByIndex() because one SPA links to multiple MAPs.
    //
    for (MapIndex = 0; MapIndex < mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE]; MapIndex++) {
      Map = (EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *)mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE][MapIndex];
      if (Map->SPARangeStructureIndex != Spa->SPARangeStructureIndex) {
        continue;
      }
      if (Map->RegionOffset + Map->MemoryDeviceRegionSize > Spa->SystemPhysicalAddressRangeLength) {
        DEBUG ((DEBUG_ERROR, "Map Region Offset[%llx]/Size[%llx] is invalid, SPA Length[%llx]\n",
          Map->RegionOffset, Map->MemoryDeviceRegionSize, Spa->SystemPhysicalAddressRangeLength));
        Status = EFI_INVALID_PARAMETER;
        goto ErrorExit;
      }
      Nvdimm = LocateNvdimm (&mPmem.Nvdimms, &Map->NFITDeviceHandle, TRUE);
      if (Nvdimm == NULL) {
        //
        // Skip if creation fails.
        //
        continue;
      }

      //
      // Find FlushHintAddress
      //
      Nvdimm->FlushHintAddress = (EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE *)LocateNfitStrucByDeviceHandle (
        mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE_TYPE],
        mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE_TYPE],
        OFFSET_OF (EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE, NFITDeviceHandle),
        &Map->NFITDeviceHandle
      );

      //
      // Find Control Region
      //
      Control = (EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE *)LocateNfitStrucByIndex (
        mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE_TYPE],
        mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE_TYPE],
        OFFSET_OF (EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE, NVDIMMControlRegionStructureIndex),
        Map->NVDIMMControlRegionStructureIndex
      );
      if (Control == NULL) {
        DEBUG ((DEBUG_ERROR, "Miss Control Region[%d] for SPA[%d]!!\n",
          Map->NVDIMMControlRegionStructureIndex, Spa->SPARangeStructureIndex));
        Status = EFI_INVALID_PARAMETER;
        goto ErrorExit;
      }

      //
      // Find Interleave
      //
      Interleave = (EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE *)LocateNfitStrucByIndex (
        mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE_TYPE],
        mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE_TYPE],
        OFFSET_OF (EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE, InterleaveStructureIndex),
        Map->InterleaveStructureIndex
      );
      if (Interleave != NULL) {
        if (Map->InterleaveWays <= 1) {
          DEBUG ((DEBUG_ERROR, "1-way interleave [SPA = %d, Control = %d] shouldn't link to Interleave[%d]!!\n",
            Map->SPARangeStructureIndex, Map->NVDIMMControlRegionStructureIndex, Map->InterleaveStructureIndex));
          Status = EFI_INVALID_PARAMETER;
          goto ErrorExit;
        }
        if (Map->MemoryDeviceRegionSize != Spa->SystemPhysicalAddressRangeLength / Map->InterleaveWays) {
          DEBUG ((DEBUG_ERROR, "Region Size[%llx] != SPA Length[%llx] / Interleave way[%d]\n",
            Map->MemoryDeviceRegionSize, Spa->SystemPhysicalAddressRangeLength, Map->InterleaveWays));
          Status = EFI_INVALID_PARAMETER;
          goto ErrorExit;
        }
      }

      DEBUG ((DEBUG_INFO, "NVDIMM[%08x]: init...\n", *(UINT32 *)&Nvdimm->DeviceHandle));
      if (CompareGuid (&Spa->AddressRangeTypeGUID, &gNvdimmPersistentMemoryRegionGuid)) {
        Nvdimm->PmRegion[Nvdimm->PmRegionCount].Spa        = Spa;
        Nvdimm->PmRegion[Nvdimm->PmRegionCount].Map        = Map;
        Nvdimm->PmRegion[Nvdimm->PmRegionCount].Control    = Control;
        Nvdimm->PmRegion[Nvdimm->PmRegionCount].Interleave = Interleave;
        Nvdimm->PmRegionCount++;
        //
        // Each interation of Map structure may or may not create one PmRegion, so the PmRegionCount won't be bigger
        // than count of Map structures.
        //
        ASSERT (Nvdimm->PmRegionCount
          < mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE]);
      } else {
        Status = InitializeBlkParameters (&Nvdimm->BlkRegion[Nvdimm->BlkRegionCount], Spa, Map, Control, Interleave);
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }
        Nvdimm->BlkRegionCount++;
        //
        // Each interation of Map structure may or may not create one BlkRegion, so the BlkRegionCount won't be bigger
        // than count of Map structures.
        //
        ASSERT (Nvdimm->BlkRegionCount
          < mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE]);
      }
    }
  }
ErrorExit:
  if (EFI_ERROR (Status)) {
    if (Nvdimm != NULL) {
      FreeNvdimm (Nvdimm);
      FreeNvdimms (&mPmem.Nvdimms);
      FreeNfitStructs ();
    }
  }
  return Status;
}
