/** @file

  Provide Block IO access support to NVDIMM namespace.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "NvdimmBlockIoDxe.h"

CACHE_LINE_FLUSH CacheLineFlush;


/**
  Initialize the cache line flush function.
**/
VOID
InitializeCpuCommands (
  VOID
  )
{
  CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_EBX  Ebx;

  AsmCpuidEx (
    CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS, CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_SUB_LEAF_INFO,
    NULL, &Ebx.Uint32, NULL, NULL
  );

  if (Ebx.Bits.CLFLUSHOPT == 1) {
    CacheLineFlush = AsmFlushCacheLineOpt;
    DEBUG ((DEBUG_INFO, "Flushing assigned to ClFlushOpt.\n"));
  } else {
    DEBUG ((DEBUG_INFO, "Flushing assigned to ClFlush.\n"));
    CacheLineFlush = (CACHE_LINE_FLUSH)AsmFlushCacheLine;
  }
}


/**
  Reset the Block Device.

  @param  This                 Indicates a pointer to the calling context.
  @param  ExtendedVerification Driver may perform diagnostics on reset.

  @retval EFI_SUCCESS          The device was reset.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

/**
  Flush the content update to NVDIMM.

  @param Nvdimm    Pointer to the NVDIMM instance.
**/
VOID
WpqFlush (
  IN CONST  NVDIMM    *Nvdimm
  )
{
  //
  // Make update durable : Using the memory controller
  // described by the ACPI NFIT tables for the NVDIMM for this IO, get a Flush Hint
  // Address for this controller and perform a WPQ Flush by executing a store with
  // any data value to the Flush Hint Address (which are in UC domain), followed
  // by an SFENCE
  //
  UINTN               Index;
  if (Nvdimm->FlushHintAddress != NULL) {
    for (Index = 0; Index < Nvdimm->FlushHintAddress->NumberOfFlushHintAddresses; Index++) {
      if (Nvdimm->FlushHintAddress->FlushHintAddress[Index] != MAX_UINT64) {
        //
        // Just use the first valid FlushHintAddress to perform WPQ flush.
        // Any value can be written to the hint address. 0XC is picked for no purpose.
        //
        *(UINT8 *)(UINTN)Nvdimm->FlushHintAddress->FlushHintAddress[Index] = 0XC;
        AsmStoreFence ();
        break;
      }
    }
  }
}

/**
  Perform byte-level of read or write operation on the specified namespace.

  @param Namespace   Pointer to NVDIMM_NAMESPACE.
  @param Write       TRUE indicates write operation; FALSE indicates read operation.
  @param Offset      The offset within the namespace.
  @param BufferSize  The size of the data to read or write.
  @param Buffer      Receive the data to read, or supply the data to write.

  @retval EFI_SUCCESS  The data is successfully read or written.
**/
EFI_STATUS
NvdimmBlockIoReadWriteBytes (
  IN NVDIMM_NAMESPACE               *Namespace,
  IN BOOLEAN                        Write,
  IN UINT64                         Offset,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  RETURN_STATUS                     RStatus;
  NVDIMM                            *Nvdimm;
  UINT64                            ByteLimit;
  UINTN                             Index;

  RStatus = SafeUint64Add (Offset, BufferSize, &ByteLimit);
  ASSERT_RETURN_ERROR (RStatus);
  ASSERT (ByteLimit < Namespace->TotalSize);

  Nvdimm = Namespace->Labels[0].Nvdimm;
  if (Namespace->Type == NamespaceTypePmem) {
    ASSERT (Nvdimm->PmMap->RegionOffset == 0);
    if (Write) {
      CopyMem (Namespace->PmSpaBase + Offset, Buffer, BufferSize);
    }
    for (Index = 0; Index < (BufferSize + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE; Index++) {
      CacheLineFlush (Namespace->PmSpaBase + Index * CACHE_LINE_SIZE);
    }

    //
    // TODO: May only WPQ Flush the affected NVDIMMs, instead of all NVDIMMs.
    //
    for (Index = 0; Index < Namespace->LabelCount; Index++) {
      ASSERT (Namespace->Labels[Index].Label->Dpa >= Namespace->Labels[Index].Nvdimm->PmMap->MemoryDevicePhysicalAddressRegionBase);
      WpqFlush (Namespace->Labels[Index].Nvdimm);
    }

    if (!Write) {
      CopyMem (Buffer, Namespace->PmSpaBase + Offset, BufferSize);
    }
    return EFI_SUCCESS;
  } else {
    return NvdimmBlkReadWriteBytes (Namespace, Write, Offset, BufferSize, Buffer);
  }
}

/**
  Block level access the media.

  @param  This       Indicates a pointer to the calling context.
  @param  Write      TRUE indicates write access.
  @param  MediaId    Id of the media, changes every time the media is replaced.
  @param  Lba        The starting Logical Block Address to read or write.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the buffer for the data. The caller is
                     responsible for either having implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The data was read or written correctly to the device.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The request contains LBAs that are not valid,
                                or the buffer is not on proper alignment.

**/
EFI_STATUS
NvdimmBlockIoReadWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        Write,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  EFI_STATUS                        Status;
  UINTN                             NumberOfBlocks;
  UINT64                            ByteOffset;
  NVDIMM_NAMESPACE                  *Namespace;
  UINTN                             Index;

  Namespace = NVDIMM_NAMESPACE_FROM_BLOCK_IO (This);
  if (MediaId != This->Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }
  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (BufferSize % Namespace->BlockSize != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }
  NumberOfBlocks = BufferSize / This->Media->BlockSize;
  if (Lba + NumberOfBlocks > This->Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if (CompareGuid (&Namespace->AddressAbstractionGuid, &gEfiBttAbstractionGuid)) {
    Status = EFI_SUCCESS;
    for (Index = 0; Index < NumberOfBlocks; Index++) {
      if (Write) {
        Status = BttWrite (Namespace->BttHandle, Lba + Index, (UINT8 *)Buffer + Index * This->Media->BlockSize);
      } else {
        Status = BttRead (Namespace->BttHandle, Lba + Index, (UINT8 *)Buffer + Index * This->Media->BlockSize);
      }
      if (EFI_ERROR (Status)) {
        break;
      }
    }
    return Status;
  } else {
    ByteOffset = MultU64x32 (Lba, Namespace->BlockSize);
    return NvdimmBlockIoReadWriteBytes (Namespace, Write, ByteOffset, BufferSize, Buffer);
  }
}

/**
  Read BufferSize bytes from Lba into Buffer.

  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    Id of the media, changes every time the media is replaced.
  @param  Lba        The starting Logical Block Address to read from.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the destination buffer for the data. The caller is
                     responsible for either having implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the read.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The read request contains LBAs that are not valid,
                                or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  return NvdimmBlockIoReadWriteBlocks (This, FALSE, MediaId, Lba, BufferSize, Buffer);
}

/**
  Write BufferSize bytes from Lba into Buffer.

  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    The media ID that the write request is for.
  @param  Lba        The starting logical block address to be written. The caller is
                     responsible for writing to only legitimate locations.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the source buffer for the data.

  @retval EFI_SUCCESS           The data was written correctly to the device.
  @retval EFI_WRITE_PROTECTED   The device can not be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHNAGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The write request contains LBAs that are not valid,
                                or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  )
{
  return NvdimmBlockIoReadWriteBlocks (This, TRUE, MediaId, Lba, BufferSize, Buffer);
}

/**
  Flush the Block Device.

  @param  This              Indicates a pointer to the calling context.

  @retval EFI_SUCCESS       All outstanding data was written to the device
  @retval EFI_DEVICE_ERROR  The device reported an error while writting back the data
  @retval EFI_NO_MEDIA      There is no media in the device.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoFlush (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

/**
  Initialize EFI_BLOCK_IO_PROTOCOL instance for the namespace.

  @param  Namespace   Pointer to NVDIMM_NAMESPACE.
**/
VOID
InitializeBlockIo (
  IN OUT NVDIMM_NAMESPACE   *Namespace
  )
{
  UINTN                     Index;

  if (CacheLineFlush == NULL) {
    InitializeCpuCommands ();
  }

  Namespace->BlockIo.Media          = &Namespace->Media;
  Namespace->Media.MediaId          = 0;
  Namespace->Media.RemovableMedia   = FALSE;
  Namespace->Media.MediaPresent     = TRUE;
  Namespace->Media.LogicalPartition = FALSE;
  Namespace->Media.ReadOnly         = Namespace->ReadOnly;
  Namespace->Media.WriteCaching     = FALSE;
  Namespace->Media.BlockSize        = (UINT32)Namespace->BlockSize;
  for (Index = 0; Index < Namespace->LabelCount; Index++) {
    Namespace->Media.IoAlign        = MAX (Namespace->Media.IoAlign, Namespace->Labels[Index].Label->Alignment);
  }
  Namespace->Media.LastBlock        = DivU64x32 (Namespace->TotalSize, Namespace->Media.BlockSize);
  Namespace->BlockIo.Revision       = EFI_BLOCK_IO_PROTOCOL_REVISION;
  Namespace->BlockIo.Reset          = NvdimmBlockIoReset;
  Namespace->BlockIo.ReadBlocks     = NvdimmBlockIoReadBlocks;
  Namespace->BlockIo.WriteBlocks    = NvdimmBlockIoWriteBlocks;
  Namespace->BlockIo.FlushBlocks    = NvdimmBlockIoFlush;
}