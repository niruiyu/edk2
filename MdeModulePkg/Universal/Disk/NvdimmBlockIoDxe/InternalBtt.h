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
  OUT VOID                          *Buffer
  );

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

VOID
EFIAPI
BttRelease (
  IN  BTT_HANDLE     BttHandle
  );
EFI_STATUS
EFIAPI
BttInitialize (
  OUT    BTT_HANDLE     *BttHandle,
  IN     GUID           *ParentUuid,
  IN     UINT32         NFree,
  IN     UINT32         ExternalLbaSize,
  IN OUT UINT64         *TotalSize,
  IN OUT UINT32         *BlockSize,
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
BOOLEAN
ValidateFletcher64 (
  UINT32  *Data,
  UINTN   Count,
  UINT64  *Checksum
);
#endif