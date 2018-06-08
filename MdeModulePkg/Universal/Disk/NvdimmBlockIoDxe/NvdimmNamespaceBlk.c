#include "NvdimmBlockIoDxe.h"

#define BW_APERTURE_LENGTH   8192

extern CACHE_LINE_FLUSH CacheLineFlush;


UINT64
ByteOffsetToDpa (
  NVDIMM_NAMESPACE  *Namespace,
  UINT64            Offset,
  UINTN             *Size
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
    *Size = Namespace->Labels[Index].Label->RawSize - Offset;
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
  Flush data from an interleaved buffer.

  @param[in] InterleavedBuffer input interleaved buffer
  @param[in] LineSize line size of interleaved buffer
  @param[in] BufferSize number of bytes to copy
**/
VOID
ReadWriteInterleavedBuffer (
  IN     BOOLEAN Write,
  IN     VOID    *Buffer,
  IN     UINT8   **InterleavedBuffer,
  IN     UINT32  LineSize,
  IN     UINT32  BufferSize
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
  UINT64                            Dpa;
  UINTN                             Size;
  UINT32                            Remainder;
  UINT64                            ApertureCount;
  UINTN                             Index;
  UINT32                            Length;

  Nvdimm = Namespace->Labels[0].Nvdimm;
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
      Nvdimm->Blk.ControlCommand->Uint64 = CommandRegister.Uint64;
      WpqFlush (Nvdimm);

      if (Write) {
        ReadWriteInterleavedBuffer (TRUE, Buffer, Nvdimm->Blk.DataWindowAperture, Nvdimm->Blk.DataWindowInterleave->LineSize, Length);
      }

      FlushInterleavedBuffer (Nvdimm->Blk.DataWindowAperture, Nvdimm->Blk.DataWindowInterleave->LineSize, Length);
      WpqFlush (Nvdimm);
      while (Nvdimm->Blk.ControlStatus->Bits.Pending) {
        CpuPause ();
      }
      if (Nvdimm->Blk.ControlStatus->Bits.InvalidAddr || Nvdimm->Blk.ControlStatus->Bits.UncorrectableError
        || Nvdimm->Blk.ControlStatus->Bits.ReadMisMatch || Nvdimm->Blk.ControlStatus->Bits.DpaRangeLocked ||
        Nvdimm->Blk.ControlStatus->Bits.BwDisabled) {
        DEBUG ((DEBUG_ERROR, "BW request fails, status = %08x!!\n", Nvdimm->Blk.ControlStatus->Uint32));
        return EFI_DEVICE_ERROR;
      }

      if (!Write) {
        ReadWriteInterleavedBuffer (FALSE, Buffer, Nvdimm->Blk.DataWindowAperture, Nvdimm->Blk.DataWindowInterleave->LineSize, Length);
      }
      Dpa += Length / CACHE_LINE_SIZE;
      Buffer = (UINT8 *)Buffer + Length;
    }
  }
}

EFI_STATUS
InitializeBlkParameters (
  OUT BLK                                                                   *Blk,
  IN  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map,
  IN  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *Control,
  IN  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave
)
{
  RETURN_STATUS                                             RStatus;
  UINTN                                                     Index;
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *Spa;

  Blk->ControlMap        = Map;
  Blk->Control           = Control;
  Blk->ControlInterleave = Interleave;

  //
  // Find BW for BLOCK interface
  //
  ASSERT (Blk->Control != NULL);
  Blk->DataWindow = (EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE *)LocateNfitStrucByIndex (
    mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE_TYPE],
    mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE_TYPE],
    OFFSET_OF (EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE, NVDIMMControlRegionStructureIndex),
    Blk->Control->NVDIMMControlRegionStructureIndex
  );
  if (Blk->DataWindow == NULL) {
    DEBUG ((DEBUG_ERROR, "Miss BW for Control[%d]!\n", Blk->Control->NVDIMMControlRegionStructureIndex));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Find Map for BW: there could be multiple Map for the same NVDIMM,
  //                find the one that links to SPA whose AddressRangeTypeGUID is gNvdimmBlockDataWindowRegionGuid
  //
  UINTN MapIndex = 0;
  for (MapIndex = 0
    ; MapIndex < mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE]
    ; ) {
    Map = (EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *)LocateNfitStrucByDeviceHandle (
      &mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE][MapIndex],
      mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE] - MapIndex,
      OFFSET_OF (EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE, NFITDeviceHandle),
      &Blk->ControlMap->NFITDeviceHandle
    );
    if (Map == NULL) {
      break;
    }
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
      Blk->DataWindowMap = Map;
      Blk->DataWindowSpa = Spa;
      break;
    }

    MapIndex = (EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *)Map + 1
      - mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE][0];
  }
  if (Blk->DataWindowMap == NULL) {
    ASSERT (Blk->DataWindowSpa == NULL);
    DEBUG ((DEBUG_ERROR, "Miss MAP/SPA for BW[Control = %d]!\n", Blk->Control->NVDIMMControlRegionStructureIndex));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Find Interleave for BW.
  //
  Blk->DataWindowInterleave = (EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE *)LocateNfitStrucByIndex (
    mPmem.NfitStrucs[EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE_TYPE],
    mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE_TYPE],
    OFFSET_OF (EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE, InterleaveStructureIndex),
    Blk->DataWindowMap->InterleaveStructureIndex
  );
  if (Blk->DataWindowInterleave == NULL) {
    DEBUG ((DEBUG_ERROR, "Miss Interleave for BW[Control = %d]!\n", Blk->Control->NVDIMMControlRegionStructureIndex));
    return EFI_INVALID_PARAMETER;
  }

  RStatus = DeviceRegionOffsetToSpa (
    Blk->Control->CommandRegisterOffsetInBlockControlWindow,
    Blk->ControlSpa,
    Blk->ControlMap,
    Blk->ControlInterleave,
    (UINT8 **)&Blk->ControlCommand
  );
  if (RETURN_ERROR (RStatus)) {
    DEBUG ((DEBUG_ERROR, "Failed to calculate BlockControlCommand!\n"));
    return EFI_INVALID_PARAMETER;
  }

  RStatus = DeviceRegionOffsetToSpa (
    Blk->Control->StatusRegisterOffsetInBlockControlWindow,
    Blk->ControlSpa,
    Blk->ControlMap,
    Blk->ControlInterleave,
    (UINT8 **)&Blk->ControlStatus
  );
  if (RETURN_ERROR (RStatus)) {
    DEBUG ((DEBUG_ERROR, "Failed to calculate BlockControlStatus!\n"));
    return EFI_INVALID_PARAMETER;
  }

  Blk->NumberOfSegments = BW_APERTURE_LENGTH / Blk->DataWindowInterleave->LineSize;
  Blk->DataWindowAperture = AllocatePool (Blk->NumberOfSegments * sizeof (UINT64));
  if (Blk->DataWindowAperture == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < Blk->NumberOfSegments; Index++) {
    RStatus = DeviceRegionOffsetToSpa (
      Blk->DataWindow->BlockDataWindowStartOffset + Index * Blk->DataWindowInterleave->LineSize,
      Blk->DataWindowSpa,
      Blk->DataWindowMap,
      Blk->DataWindowInterleave,
      &Blk->DataWindowAperture[Index]
    );
    if (RETURN_ERROR (RStatus)) {
      DEBUG ((DEBUG_ERROR, "Failed to calculate BlockDataWindowAperture!\n"));
      return EFI_INVALID_PARAMETER;
    }
  }
}