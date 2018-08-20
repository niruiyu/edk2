/** @file

  Provide NVDIMM BLK mode support.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "NvdimmBlockIoDxe.h"

#define BW_APERTURE_LENGTH   8192

extern CACHE_LINE_FLUSH CacheLineFlush;


/**
  Convert the namespace offset to the device physical address.

  @param Namespace The BLK namespace.
  @param Offset    The namespace offset.
  @param Size      Return the updated size considering access shouldn't across label.

  @return The device physical address.
**/
UINT64
ByteOffsetToDpa (
  IN NVDIMM_NAMESPACE  *Namespace,
  IN UINT64            Offset,
  IN UINTN             *Size
  )
{
  UINTN             Index;
  ASSERT (Namespace->Type == NamespaceTypeBlock);

  //
  // Lba boundary check happens in ReadBlocks/WriteBlocks.
  //
  ASSERT (Offset < Namespace->TotalSize);

  for (Index = 0; Index < Namespace->LabelCount; Index++) {
    if (Offset < Namespace->Labels[Index].Label->RawSize) {
      break;
    }
    Offset -= Namespace->Labels[Index].Label->RawSize;
  }
  ASSERT (Index != Namespace->LabelCount);

  //
  // Cannot read more than what is left in the label.
  //
  if (Offset + *Size > Namespace->Labels[Index].Label->RawSize) {
    *Size = (UINTN)(Namespace->Labels[Index].Label->RawSize - Offset);
  }

  return Offset + Namespace->Labels[Index].Label->Dpa;
}

/**
  Flush data from an interleaved buffer.

  @param[in] InterleavedBuffer input interleaved buffer
  @param[in] LineSize line size of interleaved buffer
  @param[in] BufferSize number of bytes to copy
**/
VOID
FlushInterleavedBuffer (
  IN     UINT8  **InterleavedBuffer,
  IN     UINT32 LineSize,
  IN     UINT32 BufferSize
  )
{
  UINT32 NumberOfSegments;
  UINT32 SegmentIndex;
  UINT32 CacheLineIndex;

  ASSERT ((InterleavedBuffer != NULL) && (LineSize != 0));
  ASSERT (CacheLineFlush != NULL);

  NumberOfSegments = (BufferSize + LineSize - 1) / LineSize;

  for (SegmentIndex = 0; SegmentIndex < NumberOfSegments; SegmentIndex++) {
    if (LineSize > BufferSize) {
      //
      // For the last line.
      //
      LineSize = BufferSize;
    }
    for (CacheLineIndex = 0; CacheLineIndex < (LineSize + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE; CacheLineIndex++) {
      CacheLineFlush(InterleavedBuffer[SegmentIndex] + CacheLineIndex * CACHE_LINE_SIZE);
    }
    BufferSize -= LineSize;
  }
}

/**
  Read or write the interleaved buffer.
  @param Write             TRUE indicates write operation.
  @param Buffer            The data to receive the content or to write to the NVDIMM.
  @param BufferSize        Number of bytes to read or write.
  @param InterleavedBuffer The interleaved buffer.
  @param LineSize          Line size of interleaved buffer.
**/
VOID
ReadWriteInterleavedBuffer (
  IN     BOOLEAN Write,
  IN OUT VOID    *Buffer,
  IN     UINT32  BufferSize,
  IN     UINT8   **InterleavedBuffer,
  IN     UINT32  LineSize
  )
{
  UINT32 NumberOfSegments;
  UINT32 SegmentIndex;

  ASSERT (Buffer != NULL);
  ASSERT ((InterleavedBuffer != NULL) && (LineSize != 0));

  NumberOfSegments = (BufferSize + LineSize - 1) / LineSize;

  for (SegmentIndex = 0; SegmentIndex < NumberOfSegments; SegmentIndex++) {
    if (LineSize > BufferSize) {
      //
      // For the last line.
      //
      LineSize = BufferSize;
    }
    if (Write) {
      CopyMem (InterleavedBuffer[SegmentIndex], Buffer, LineSize);
    } else {
      CopyMem (Buffer, InterleavedBuffer[SegmentIndex], LineSize);
    }
    Buffer = (UINT8 *)Buffer + LineSize;
    BufferSize -= LineSize;
  }
}

/**
  Read or write the NVDIMM BLK namespace.

  @param Namespace  The NVDIMM BLK namespace to access.
  @param Write      TRUE indicate write operation.
  @param Offset     The offset in the namespace.
  @param BufferSize Size of the data to read or write.
  @param Buffer     The data to read or write.

  @retval EFI_SUCCESS The BLK namespace acess succeeds.
**/
EFI_STATUS
NvdimmBlkReadWriteBytes (
  IN NVDIMM_NAMESPACE               *Namespace,
  IN BOOLEAN                        Write,
  IN UINT64                         Offset,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  BW_COMMAND_REGISTER               CommandRegister;
  NVDIMM                            *Nvdimm;
  NVDIMM_BLK_REGION                 *BlkRegion;
  UINT64                            Dpa;
  UINTN                             Size;
  UINT32                            Remainder;
  UINT64                            ApertureCount;
  UINTN                             Index;
  UINT32                            Length;

  Nvdimm    = Namespace->Labels[0].Nvdimm;
  BlkRegion = Namespace->Labels[0].BlkRegion;
  //
  // All labels are in the same NVDIMM.
  //
  while (BufferSize != 0) {
    //
    // Size holds what's left in current label
    //
    Size = BufferSize;
    Dpa = ByteOffsetToDpa (Namespace, Offset, &Size);
    ASSERT (Dpa != MAX_UINT64);
    BufferSize -= Size;

    //
    // Make sure Dpa is aligned in cache line (64 bytes)
    //
    Dpa = DivU64x32Remainder (Dpa, CACHE_LINE_SIZE, &Remainder);
    ASSERT (Remainder == 0);
    ASSERT (BW_APERTURE_LENGTH % CACHE_LINE_SIZE == 0);

    ApertureCount = (Size + BW_APERTURE_LENGTH - 1) / BW_APERTURE_LENGTH;

    for (Index = 0; Index < ApertureCount; Index++) {

      if (Size < BW_APERTURE_LENGTH) {
        Length = (UINT32)Size;
      } else {
        Length = BW_APERTURE_LENGTH;
      }
      Size -= Length;

      CommandRegister.Uint64 = Dpa;
      CommandRegister.Bits.Write = Write ? 1 : 0;
      CommandRegister.Bits.Size = (Length + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
      BlkRegion->ControlCommand->Uint64 = CommandRegister.Uint64;
      WpqFlush (Nvdimm);

      if (Write) {
        ReadWriteInterleavedBuffer (TRUE, Buffer, Length, BlkRegion->DataWindowAperture, BlkRegion->DataWindowInterleave->LineSize);
      }

      FlushInterleavedBuffer (BlkRegion->DataWindowAperture, BlkRegion->DataWindowInterleave->LineSize, Length);
      WpqFlush (Nvdimm);
      while (BlkRegion->ControlStatus->Bits.Pending) {
        CpuPause ();
      }
      if (BlkRegion->ControlStatus->Bits.InvalidAddr || BlkRegion->ControlStatus->Bits.UncorrectableError
        || BlkRegion->ControlStatus->Bits.ReadMisMatch || BlkRegion->ControlStatus->Bits.DpaRangeLocked ||
        BlkRegion->ControlStatus->Bits.BwDisabled) {
        DEBUG ((DEBUG_ERROR, "BW request fails, status = %08x!!\n", BlkRegion->ControlStatus->Uint32));
        return EFI_DEVICE_ERROR;
      }

      if (!Write) {
        ReadWriteInterleavedBuffer (FALSE, Buffer, Length, BlkRegion->DataWindowAperture, BlkRegion->DataWindowInterleave->LineSize);
      }
      Dpa += Length / CACHE_LINE_SIZE;
      Buffer = (UINT8 *)Buffer + Length;
    }
  }
  return EFI_SUCCESS;
}

/**
  Initialize the NVDIMM BLK namespace parameters.

  @param Blk         The BLK parameters to initialize.
  @param Spa         The spa structure.
  @param Map         The map structure.
  @param Control     The control region structure.
  @param Interleave  The interleave structure.

  @retval EFI_SUCCESS           The BLK namespace parameters are initialized.
  @retval EFI_INVALID_PARAMETER The NFIT ACPI structures contain invalid data.
**/
EFI_STATUS
InitializeBlkParameters (
  OUT NVDIMM_REGION                                                         *Region,
  OUT NVDIMM_BLK_REGION                                                     *BlkRegion,
  IN  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *Spa,
  IN  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map,
  IN  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *Control,
  IN  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave
  )
{
  RETURN_STATUS                                             RStatus;
  UINTN                                                     Index;
  UINTN                                                     MapIndex;

  Region->Spa        = Spa;
  Region->Map        = Map;
  Region->Control    = Control;
  Region->Interleave = Interleave;

  //
  // Find BW for BLOCK interface
  //
  ASSERT (Control != NULL);
  BlkRegion->DataWindow = (EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE *)LocateNfitStrucByIndex (
    mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE_TYPE],
    mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE_TYPE],
    OFFSET_OF (EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE, NVDIMMControlRegionStructureIndex),
    Region->Control->NVDIMMControlRegionStructureIndex
  );
  if (BlkRegion->DataWindow == NULL) {
    DEBUG ((DEBUG_ERROR, "Miss BW for Control[%d]!\n", Region->Control->NVDIMMControlRegionStructureIndex));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Find Map for BW: there could be multiple Map for the same NVDIMM,
  //                find the one that links to SPA whose AddressRangeTypeGUID is gNvdimmBlockDataWindowRegionGuid
  //
  for (MapIndex = 0
    ; MapIndex < mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE]
    ; ) {
    Map = (EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *)LocateNfitStrucByDeviceHandle (
      &mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE][MapIndex],
      mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE] - MapIndex,
      OFFSET_OF (EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE, NFITDeviceHandle),
      &Region->Map->NFITDeviceHandle
    );
    if (Map == NULL) {
      break;
    }
    //
    // Find the accordingly Spa
    //
    Spa = (EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *)LocateNfitStrucByIndex (
      mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE_TYPE],
      mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE_TYPE],
      OFFSET_OF (EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE, SPARangeStructureIndex),
      Map->SPARangeStructureIndex
    );
    if (Spa == NULL) {
      break;
    }
    if (CompareGuid (&Spa->AddressRangeTypeGUID, &gNvdimmBlockDataWindowRegionGuid)) {
      //
      // Found the Map and Spa for the Data Window.
      //
      BlkRegion->DataWindowMap = Map;
      BlkRegion->DataWindowSpa = Spa;
      break;
    }

    //
    // Haven't found. Update MapIndex to find the next Map.
    //
    MapIndex = (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)Map + 1
      - mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE][0];
  }
  if (BlkRegion->DataWindowMap == NULL) {
    ASSERT (BlkRegion->DataWindowSpa == NULL);
    DEBUG ((DEBUG_ERROR, "Miss MAP/SPA for BW[Control = %d]!\n", Region->Control->NVDIMMControlRegionStructureIndex));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Find Interleave for BW.
  //
  BlkRegion->DataWindowInterleave = (EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE *)LocateNfitStrucByIndex (
    mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE_TYPE],
    mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE_TYPE],
    OFFSET_OF (EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE, InterleaveStructureIndex),
    BlkRegion->DataWindowMap->InterleaveStructureIndex
  );
  if (BlkRegion->DataWindowInterleave == NULL) {
    DEBUG ((DEBUG_ERROR, "Miss Interleave for BW[Control = %d]!\n", Region->Control->NVDIMMControlRegionStructureIndex));
    return EFI_INVALID_PARAMETER;
  }

  RStatus = DeviceRegionOffsetToSpa (
    Region->Control->CommandRegisterOffsetInBlockControlWindow,
    Region->Spa,
    Region->Map,
    Region->Interleave,
    (UINT8 **)&BlkRegion->ControlCommand
  );
  if (RETURN_ERROR (RStatus)) {
    DEBUG ((DEBUG_ERROR, "Failed to calculate BlockControlCommand!\n"));
    return EFI_INVALID_PARAMETER;
  }

  RStatus = DeviceRegionOffsetToSpa (
    Region->Control->StatusRegisterOffsetInBlockControlWindow,
    Region->Spa,
    Region->Map,
    Region->Interleave,
    (UINT8 **)&BlkRegion->ControlStatus
  );
  if (RETURN_ERROR (RStatus)) {
    DEBUG ((DEBUG_ERROR, "Failed to calculate BlockControlStatus!\n"));
    return EFI_INVALID_PARAMETER;
  }

  BlkRegion->NumberOfSegments = BW_APERTURE_LENGTH / BlkRegion->DataWindowInterleave->LineSize;
  BlkRegion->DataWindowAperture = AllocatePool (BlkRegion->NumberOfSegments * sizeof (UINT64));
  if (BlkRegion->DataWindowAperture == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < BlkRegion->NumberOfSegments; Index++) {
    RStatus = DeviceRegionOffsetToSpa (
      BlkRegion->DataWindow->BlockDataWindowStartOffset + Index * BlkRegion->DataWindowInterleave->LineSize,
      BlkRegion->DataWindowSpa,
      BlkRegion->DataWindowMap,
      BlkRegion->DataWindowInterleave,
      &BlkRegion->DataWindowAperture[Index]
    );
    if (RETURN_ERROR (RStatus)) {
      DEBUG ((DEBUG_ERROR, "Failed to calculate BlockDataWindowAperture!\n"));
      return EFI_INVALID_PARAMETER;
    }
  }
  return EFI_SUCCESS;
}