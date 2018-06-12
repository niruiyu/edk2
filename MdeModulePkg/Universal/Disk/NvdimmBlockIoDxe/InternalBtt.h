/** @file

  Provide BTT related functions prototype.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#ifndef _INTERNAL_BTT_H_
#define _INTERNAL_BTT_H_
#include <Uefi.h>
#include <Guid/Btt.h>

typedef  VOID    *BTT_HANDLE;
typedef
EFI_STATUS
(EFIAPI *BTT_RAW_ACCESS) (
  IN VOID                           *Context,
  IN BOOLEAN                        Write,
  IN UINT64                         Offset,
  IN UINTN                          BufferSize,
  IN OUT VOID                       *Buffer
  );

/**
  Load the BTT layout meta data and create the BTT handle.

  @param BttHandle  Return the BTT handle for the media.
  @param ParentUuid The UUID of the media.
  @param TotalSize  On input, it is the raw size of the media.
                    On output, return the total external available size.
  @param BlockSize  Return the external block size.
  @param RawAccess  Pointer to the routine that can access the raw media.
  @param Context    The context passed to RawAccess routine.

  @retval EFI_INVALID_PARAMETER One of the input parameters is invalid.
  @retval EFI_INVALID_PARAMETER The BTT layout meta data is invalid.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource for loading the BTT layout.
  @retval EFI_SUCCESS           The BTT layout meta data is successfully loaded.
**/
EFI_STATUS
EFIAPI
BttLoad (
  OUT    BTT_HANDLE     *BttHandle,
  IN     GUID           *ParentUuid,
  IN OUT UINT64         *TotalSize,
  IN OUT UINT32         *BlockSize,
  IN     BTT_RAW_ACCESS RawAccess,
  IN     VOID           *Context
  );

/**
  Release the resources occupied by the BTT layout.

  @param BttHandle The BTT layout handle.
**/
VOID
EFIAPI
BttRelease (
  IN  BTT_HANDLE     BttHandle
  );

/**
  Initialize the BTT layout and return the BTT handle.

  @param BttHandle       Return the initialized BTT handle.
  @param ParentUuid      The UUID of the namespace where BTT is in.
  @param NFree           Number of free blocks.
  @param ExternalLbaSize External LBA size.
  @param TotalSize       On input, it is the size of the RAW area.
                         On output, return the external available size of the BTT.
  @param RawAccess       Pointer to the routine that can access the raw media.
  @param Context         The context passed to RawAccess routine.

  @retval EFI_INVALID_PARAMETER One of the input parameters is invalid.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource to initialize the BTT layout.
  @retval EFI_SUCCESS           The BTT layout meta data is successfully initialized.
**/
EFI_STATUS
EFIAPI
BttInitialize (
  OUT    BTT_HANDLE     *BttHandle,
  IN     GUID           *ParentUuid,
  IN     UINT32         NFree,
  IN     UINT32         ExternalLbaSize,
  IN OUT UINT64         *TotalSize,
  IN     BTT_RAW_ACCESS RawAccess,
  IN     VOID           *Context
  );
#pragma pack(1)
typedef struct {
  UINT32 Lba;
  UINT32 OldMap;
  UINT32 NewMap;
  UINT32 Seq;
} FLOG;
#pragma pack()

VERIFY_SIZE_OF (EFI_BTT_FLOG, sizeof (FLOG) * 2);

/**
Structure for keeping Flog Entries in runtime
**/
typedef struct _FLOG_RUNTIME {
  FLOG         Flog[2];       ///< Array style of EFI_BTT_FLOG structure.
  UINT64       Offset;        ///< Offset of EFI_BTT_FLOG structure in NVDIMM.
  UINT8        Active;        ///< 0 or 1 to indicate which Flog is active.
} FLOG_RUNTIME;

typedef struct {
  ///
  /// UUID identifying this BTT instance.
  ///
  GUID         Uuid;

  ///
  /// Attributes of this BTT Info Block.
  ///
  UINT32       Flags;

  UINT64       Base;
  UINT64       Size;

  ///
  /// Advertised number of LBAs in this arena.
  ///
  UINT32       ExternalNLba;

  ///
  /// Number of internal blocks in the arena data area.
  ///
  UINT32       InternalNLba;

  ///
  /// Number of free blocks maintained for writes to this arena.
  ///
  UINT32       NFree;

  ///
  /// Offset of the data area for this arena, relative to the beginning of this arena.
  ///
  UINT64       DataOff;

  ///
  /// Offset of the map for this arena, relative to the beginning of this arena.
  ///
  UINT64       MapOff;

  ///
  /// Offset of the flog for this arena, relative to the beginning of this arena.
  ///
  UINT64       FlogOff;

  FLOG_RUNTIME *Flogs;
} BTT_ARENA;

typedef struct {
  GUID             ParentUuid;
  UINT64           RawSize;
  UINT64           ExternalNLba;
  UINT32           ExternalLbaSize;
  UINT32           InternalLbaSize;
  UINT32           NumberOfArenas;
  BTT_ARENA        *Arenas;
  BTT_RAW_ACCESS   RawAccess;
  VOID             *Context;
} BTT;

/**
  Return the fletcher 64 checksum of the binary data.

  @param Data     The binary data.
  @param Count    The number of 4-byte elements in the binary data.

  @return  The fletcher 64 checksum.
**/
UINT64
CalculateFletcher64 (
  UINT32  *Data,
  UINTN   Count
  );

/**
  Compare two sequence numbers to determine which one is higher.
  01 < 10 < 11 < 01 < ...

  @retval 0  Seq0 is higher.
  @retval 1  Seq1 is higher.
**/
UINT8
SequenceHigher (
  UINT32  Seq0,
  UINT32  Seq1
  );

/**
  Read a block from a Block Translation Table (BTT) layout media.

  @param [in]  BttHandle  BTT layout media handle
  @param [in]  Lba        Logical block address to be read.
  @param [out] Buffer     Receive the content read out.

  @retval EFI_SUCCESS  The routine succeeds.
**/
EFI_STATUS
BttRead (
  IN  BTT_HANDLE BttHandle,
  IN  UINT64     Lba,
  OUT VOID       *Buffer
  );

/**
  Writes a block to a Block Translation Table (BTT) layout media.

  @param [in] BttHandle  BTT layout media handle
  @param [in] Lba        Logical block address to be written.
  @param [in] Buffer     The content to write.

  @retval EFI_SUCCESS  The routine succeeds.
**/
EFI_STATUS
EFIAPI
BttWrite (
  IN  BTT_HANDLE BttHandle,
  IN  UINT64     Lba,
  IN  VOID       *Buffer
  );

/**
  Check whether the fletcher 64 checksum of the binary data is valid.

  @param Data     The binary data.
  @param Count    The number of 4-byte elements in the binary data.
  @param Checksum Pointer to the location where the checksum is stored.
                  It must be within the binary data.

  @retval TRUE  The fletcher 64 checksum is valid.
  @retval FALSE The fletcher 64 checksum is not valid.
**/
BOOLEAN
IsFletcher64Valid (
  UINT32  *Data,
  UINTN   Count,
  UINT64  *Checksum
  );
#endif
