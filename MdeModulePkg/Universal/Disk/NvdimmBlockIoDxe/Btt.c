#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseCryptLib.h>
#include "InternalBtt.h"

#define CACHE_LINE_SIZE 64
BOOLEAN mRandomSeeded = FALSE;

//
// Look up table for next sequence number.
// Next sequence number of I is mSeqNext[I].
// I(= 0) is not a valid sequence number.
//                       0b01  0b10  0b11
//
UINT8  mSeqNext[] = { 0, 0b10, 0b11, 0b01 };

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
)
{
  ASSERT (Seq0 != Seq1);
  ASSERT (Seq0 <= 3);
  ASSERT (Seq1 <= 3);
  if (Seq0 == 0) {
    //
    // Choose Seq1
    //
    return 1;
  }
  if (Seq1 == 0) {
    return 0;
  }

  if (mSeqNext[Seq0] = Seq1) {
    return 1;
  } else {
    return 0;
  }
}

VOID
GenerateUuid (
  OUT GUID *Guid
)
{
  //
  // The version 4 UUID is meant for generating UUIDs from truly-random or
  // pseudo-random numbers.
  // The algorithm is as follows:
  // > Set the two most significant bits (bits 6 and 7) of the
  //   clock_seq_hi_and_reserved to zero and one, respectively.
  // > Set the four most significant bits (bits 12 through 15) of the
  //   time_hi_and_version field to the 4-bit version number from
  //   Section 4.1.3.
  // > Set all the other bits to randomly (or pseudo-randomly) chosen
  //   values.
  //
  BOOLEAN      Success;
  ASSERT (Guid != NULL);
  if (!mRandomSeeded) {
    Success = RandomSeed (NULL, 0);
    ASSERT (Success);
    mRandomSeeded = TRUE;
  }
  Success = RandomBytes ((UINT8 *)Guid, sizeof (*Guid));
  ASSERT (Success);

  //
  // Version 4 (Random GUID)
  //
  Guid->Data3 = BitFieldWrite16 (Guid->Data3, 12, 15, 4);
  Guid->Data4[0] = BitFieldWrite8 (Guid->Data4[0], 6, 7, 0b10);
}

BOOLEAN
ValidateFletcher64 (
  UINT32  *Data,
  UINTN   Count,
  UINT64  *Checksum
)
{
  UINT32  LoSum;
  UINT32  HiSum;
  UINTN   Index;
  UINT32  Uint32;

  LoSum = 0;
  HiSum = 0;

  for (Index = 0; Index < Count; Index++) {
    if ((&Data[Index] == (UINT32 *)Checksum) || (&Data[Index] == (UINT32 *)Checksum + 1)) {
      Uint32 = 0;
    } else {
      Uint32 = Data[Index];
    }
    LoSum += Data[Index];
    HiSum += LoSum;
  }

  return (BOOLEAN)((LShiftU64 (HiSum, 32) | LoSum) == *Checksum);
}

UINT64
CalculateFletcher64 (
  UINT32  *Data,
  UINTN   Count
)
{
  UINT32  LoSum;
  UINT32  HiSum;
  UINTN   Index;

  LoSum = 0;
  HiSum = 0;

  for (Index = 0; Index < Count; Index++) {
    LoSum += Data[Index];
    HiSum += LoSum;
  }

  return LShiftU64 (HiSum, 32) | LoSum;
}

VOID
DumpBttInfo (
  EFI_BTT_INFO_BLOCK       *BttInfo
)
{
  DEBUG ((DEBUG_INFO,
    "BttInfoBlock:\n"
    "  Sig: %a(%02x)\n"
    "  Uuid: %g\n"
    "  ParentUuid: %g\n"
    "  Flags: %08x\n"
    "  Major/Minor: %04x/%04x\n"
    "  External LbaSize/NLba: %d/%d\n"
    "  Internal LbaSize/NLba: %d/%d\n"
    "  NFree: %d\n"
    "  InfoSize: %d\n"
    "  Offset Next/Data/Map/Flog/Info: %d/%d/%d/%d/%d\n"
    "  Checksum: %016x\n",
    BttInfo->Sig, BttInfo->Sig[EFI_BTT_INFO_BLOCK_SIG_LEN - 1],
    &BttInfo->Uuid,
    &BttInfo->ParentUuid,
    BttInfo->Flags,
    BttInfo->Major, BttInfo->Minor,
    BttInfo->ExternalLbaSize, BttInfo->ExternalNLba,
    BttInfo->InternalLbaSize, BttInfo->InternalNLba,
    BttInfo->NFree,
    BttInfo->InfoSize,
    BttInfo->NextOff, BttInfo->DataOff, BttInfo->MapOff, BttInfo->FlogOff, BttInfo->InfoOff,
    BttInfo->Checksum
    ));
}
BOOLEAN
IsBttInfoValid (
  EFI_BTT_INFO_BLOCK    *BttInfo,
  BTT                   *Btt,
  UINT64                ArenaBase,
  UINT64                ArenaSize
)
{
  UINT64                FlogSize;
  UINT64                MapSize;

  ASSERT ((ArenaBase & (EFI_BTT_ALIGNMENT - 1)) == 0);
  ASSERT ((ArenaSize & (EFI_BTT_ALIGNMENT - 1)) == 0);

  if (CompareMem (BttInfo->Sig, EFI_BTT_INFO_BLOCK_SIGNATURE, sizeof (BttInfo->Sig)) != 0) {
    return FALSE;
  }
  if (!CompareGuid (&BttInfo->ParentUuid, &Btt->ParentUuid)) {
    return FALSE;
  }
  if (BttInfo->ExternalLbaSize != Btt->ExternalLbaSize) {
    return FALSE;
  }
  if (BttInfo->InternalLbaSize != Btt->InternalLbaSize) {
    return FALSE;
  }
  if (BttInfo->InfoSize != sizeof (*BttInfo)) {
    return FALSE;
  }
  if ((BttInfo->Major != EFI_BTT_INFO_BLOCK_MAJOR_VERSION) || (BttInfo->Minor != EFI_BTT_INFO_BLOCK_MINOR_VERSION)) {
    return FALSE;
  }
  if ((BttInfo->InternalNLba <= BttInfo->ExternalNLba) || (BttInfo->NFree != BttInfo->InternalNLba - BttInfo->ExternalNLba)) {
    return FALSE;
  }
  FlogSize = MultU64x32 (BttInfo->NFree, ALIGN_VALUE (sizeof (EFI_BTT_FLOG), EFI_BTT_FLOG_ENTRY_ALIGNMENT));
  FlogSize = ALIGN_VALUE (FlogSize, EFI_BTT_ALIGNMENT);
  if ((BttInfo->FlogOff < BttInfo->InfoSize) ||
      (BttInfo->FlogOff >= ArenaSize) ||
      ((BttInfo->FlogOff & (EFI_BTT_ALIGNMENT - 1)) != 0)
    ) {
    return FALSE;
  }

  MapSize = MultU64x32 (BttInfo->ExternalNLba, sizeof (EFI_BTT_MAP_ENTRY));
  MapSize = ALIGN_VALUE (MapSize, EFI_BTT_ALIGNMENT);
  if ((BttInfo->MapOff < BttInfo->FlogOff + FlogSize) ||
      (BttInfo->MapOff >= ArenaSize) ||
      ((BttInfo->MapOff & (EFI_BTT_ALIGNMENT - 1)) != 0)
    ) {
    return FALSE;
  }

  if ((BttInfo->DataOff < BttInfo->MapOff + MapSize) ||
      (BttInfo->DataOff >= ArenaSize) ||
      ((BttInfo->DataOff & (EFI_BTT_ALIGNMENT - 1)) != 0)
    ) {
    return FALSE;
  }

  if (BttInfo->InfoOff != ArenaSize - sizeof (*BttInfo)) {
    return FALSE;
  }

  if ((BttInfo->NextOff != 0) && (BttInfo->NextOff != ArenaSize)) {
    return FALSE;
  }

  return ValidateFletcher64 ((UINT32 *)BttInfo, sizeof (*BttInfo) / sizeof (UINT32), &BttInfo->Checksum);
}

VOID
DumpBttFlog (
  FLOG   *Flog
)
{
  UINTN  Index;
  for (Index = 0; Index < 2; Index++) {
    DEBUG ((DEBUG_INFO, "Flog[%d].Lba/OldMap/NewMap/Seq = %x/%08x/%08x/%x\n",
      Index, Flog[Index].Lba, Flog[Index].OldMap, Flog[Index].NewMap, Flog[Index].Seq));
  }
}

EFI_STATUS
BttInitializeFlogs (
  BTT_ARENA  *Arena
)
{
  UINT32             Index;
  FLOG_RUNTIME       *FlogRuntime;
  UINT64             FlogOff;

  Arena->Flogs = AllocateZeroPool (sizeof (*Arena->Flogs) * Arena->NFree);
  if (Arena->Flogs == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0, FlogOff = Arena->FlogOff, FlogRuntime = Arena->Flogs
    ; Index < Arena->NFree
    ; Index++, FlogOff += ALIGN_VALUE (sizeof (EFI_BTT_FLOG), EFI_BTT_FLOG_ENTRY_ALIGNMENT), FlogRuntime++
    ) {
    FlogRuntime->Offset         = FlogOff;
    FlogRuntime->Active         = 0;
    FlogRuntime->Flog[FlogRuntime->Active].Seq    = 0b01;
    FlogRuntime->Flog[FlogRuntime->Active].Lba    = Index;
    FlogRuntime->Flog[FlogRuntime->Active].OldMap = Arena->ExternalNLba + Index;
    FlogRuntime->Flog[FlogRuntime->Active].NewMap = Arena->ExternalNLba + Index;
    ZeroMem (&FlogRuntime->Flog[1 - FlogRuntime->Active], sizeof (FlogRuntime->Flog[1 - FlogRuntime->Active]));
  }
  return EFI_SUCCESS;
}

/**
  Load the flog entries at startup.
  Validadation and recovery are performed as well per UEFI Spec 6.3.6 Validating the Flogs entries at start-up.
**/
EFI_STATUS
BttLoadFlogs (
  BTT        *Btt,
  BTT_ARENA  *Arena
)
{
  EFI_STATUS         Status;
  UINTN              Index;
  FLOG_RUNTIME       *FlogRuntime;
  FLOG               *Flog;
  UINT64             FlogOff;
  UINT64             MapOffset;
  UINTN              Remainder;
  UINT8              CacheLine[CACHE_LINE_SIZE];
  EFI_BTT_MAP_ENTRY  *Map;

  Arena->Flogs = AllocateZeroPool (sizeof (*Arena->Flogs) * Arena->NFree);
  if (Arena->Flogs == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0, FlogOff = Arena->FlogOff, FlogRuntime = Arena->Flogs
    ; Index < Arena->NFree
    ; Index++, FlogOff += ALIGN_VALUE (sizeof (EFI_BTT_FLOG), EFI_BTT_FLOG_ENTRY_ALIGNMENT), FlogRuntime++
    ) {
    FlogRuntime->Offset = FlogOff;
    ASSERT (sizeof (FlogRuntime->Flog) == sizeof (EFI_BTT_FLOG));
    Status = Btt->RawAccess (Btt->Context, FALSE, FlogOff, sizeof (FlogRuntime->Flog), &FlogRuntime->Flog);
    if (EFI_ERROR (Status)) {
      break;
    }
    DumpBttFlog (FlogRuntime->Flog);
    if (FlogRuntime->Flog[0].Seq == FlogRuntime->Flog[1].Seq) {
      break;
    }
    if ((FlogRuntime->Flog[0].Seq > 3) || (FlogRuntime->Flog[1].Seq > 3)) {
      break;
    }
    if ((FlogRuntime->Flog[0].Lba >= Arena->ExternalNLba) || (FlogRuntime->Flog[1].Lba >= Arena->ExternalNLba)) {
      break;
    }
    FlogRuntime->Active = SequenceHigher (FlogRuntime->Flog[0].Seq, FlogRuntime->Flog[1].Seq);
    Flog = &FlogRuntime->Flog[FlogRuntime->Active];

    if (Flog->OldMap == Flog->NewMap) {
      continue;
    }

    //
    // Read the Map entry. Make sure access aligns on cache line.
    //
    MapOffset = Arena->MapOff + sizeof (EFI_BTT_MAP_ENTRY) * Flog->Lba;
    Remainder = MapOffset & (CACHE_LINE_SIZE - 1);
    MapOffset -= Remainder;
    Status = Btt->RawAccess (Btt->Context, FALSE, MapOffset, sizeof (CacheLine), CacheLine);
    if (EFI_ERROR (Status)) {
      break;
    }

    Map = (EFI_BTT_MAP_ENTRY *)&CacheLine[Remainder];
    if ((Map->Error == 0) && (Map->Zero == 0)) {
      Map->Error      = 1;
      Map->Zero       = 1;
      Map->PostMapLba = Flog->Lba;
    }
    if ((Flog->NewMap != *(UINT32 *)Map) && (Flog->OldMap == *(UINT32 *)Map)) {
      //
      // Last write didn't complete. Update the Map entry to recover.
      //
      *(UINT32 *)Map = Flog->NewMap;
      Status = Btt->RawAccess (Btt->Context, TRUE, MapOffset, sizeof (CacheLine), CacheLine);
      if (EFI_ERROR (Status)) {
        break;
      }
    }
  }
  if (Index != Arena->NFree) {
    return EFI_INVALID_PARAMETER;
  }
  return EFI_SUCCESS;
}

EFI_STATUS
BttInitializeArena (
  OUT BTT_ARENA  *Arena,
  IN  UINT32     NFree,
  IN  UINT32     InternalLbaSize,
  IN  UINT64     Base,
  IN  UINT64     Size
  )
{
  UINT64               FlogSize;
  UINT64               DataAndMapSize;
  UINT64               MapSize;

  FlogSize = MultU64x32 (NFree, ALIGN_VALUE (sizeof (EFI_BTT_FLOG), EFI_BTT_FLOG_ENTRY_ALIGNMENT));
  FlogSize = ALIGN_VALUE (FlogSize, EFI_BTT_ALIGNMENT);

  DataAndMapSize = Size - 2 * sizeof (EFI_BTT_INFO_BLOCK) - FlogSize;
  Arena->NFree   = NFree;
  //
  // Calculate the minimal InternalNLba
  //
  // (InternalNLba - NFree) * sizeof (EFI_BTT_MAP_ENTRY) + Delta#1 + InternalNLba * InternalLbaSize + Delta#2
  //       = DataAndMapSize
  // So, InternalNLba = (DataAndMapSize + NFree * sizeof (EFI_BTT_MAP_ENTRY) - Delta#1 - Delta#2)
  //       / (InternalLbaSize + sizeof (EFI_BTT_MAP_ENTRY))
  //
  // Delta#1 < EFI_BTT_ALIGNMENT, Delta#2 < EFI_BTT_ALIGNMENT
  // So, InternalNLba >= (DataAndMapSize + NFree * sizeof (EFI_BTT_MAP_ENTRY) - EFI_BTT_ALIGNMENT * 2)
  //       / (InternalLbaSize + sizeof (EFI_BTT_MAP_ENTRY))
  // NOTE: InternalLbaSize >= 512, so 512GB(MAX Arena Size) / 512(InternalLbaSize) = 1G(InternalNLba)
  //
  Arena->InternalNLba = (UINT32)DivU64x64Remainder (
    DataAndMapSize + Arena->NFree * sizeof (EFI_BTT_MAP_ENTRY) - EFI_BTT_ALIGNMENT * 2,
    InternalLbaSize + sizeof (EFI_BTT_MAP_ENTRY),
    NULL
  );

  //
  // Calculate the exact InternalNLba
  //
  while (TRUE) {
    MapSize = MultU64x32 (Arena->InternalNLba + 1 - Arena->NFree, sizeof (EFI_BTT_MAP_ENTRY));
    MapSize = ALIGN_VALUE (MapSize, EFI_BTT_ALIGNMENT);
    if (DivU64x32 (DataAndMapSize - MapSize, InternalLbaSize) >= Arena->InternalNLba + 1) {
      Arena->InternalNLba++;
    } else {
      break;
    }
  }
  GenerateUuid (&Arena->Uuid);
  Arena->ExternalNLba = Arena->InternalNLba - Arena->NFree;
  Arena->Base         = Base;
  Arena->Size         = Size;
  Arena->FlogOff      = Arena->Base + ALIGN_VALUE (sizeof (EFI_BTT_INFO_BLOCK), EFI_BTT_ALIGNMENT);
  Arena->MapOff       = Arena->FlogOff + FlogSize;
  Arena->DataOff      = Arena->MapOff + ALIGN_VALUE (
    MultU64x32 (Arena->ExternalNLba, sizeof (EFI_BTT_MAP_ENTRY)), EFI_BTT_ALIGNMENT
  );
  return BttInitializeFlogs (Arena);
}

EFI_STATUS
BttWriteArena (
  BTT        *Btt,
  BTT_ARENA  *Arena
)
{

  EFI_STATUS         Status;
  EFI_BTT_INFO_BLOCK BttInfo;
  UINT32             Index;
  FLOG_RUNTIME       *FlogRuntime;

  ZeroMem (&BttInfo, sizeof (BttInfo));
  CopyMem (&BttInfo.Sig, EFI_BTT_INFO_BLOCK_SIGNATURE, sizeof (BttInfo.Sig));
  BttInfo.Major = EFI_BTT_INFO_BLOCK_MAJOR_VERSION;
  BttInfo.Minor = EFI_BTT_INFO_BLOCK_MINOR_VERSION;
  BttInfo.Flags = Arena->Flags;
  CopyGuid (&BttInfo.ParentUuid, &Btt->ParentUuid);
  CopyGuid (&BttInfo.Uuid, &Arena->Uuid);
  BttInfo.ExternalLbaSize = Btt->ExternalLbaSize;
  BttInfo.InternalLbaSize = Btt->InternalLbaSize;
  BttInfo.ExternalNLba    = Arena->ExternalNLba;
  BttInfo.InternalNLba    = Arena->InternalNLba;
  BttInfo.NFree           = Arena->NFree;
  BttInfo.InfoSize        = sizeof (BttInfo);
  BttInfo.FlogOff         = Arena->FlogOff - Arena->Base;
  BttInfo.MapOff          = Arena->MapOff - Arena->Base;
  BttInfo.DataOff         = Arena->DataOff - Arena->Base;
  BttInfo.InfoOff         = Arena->Size - ALIGN_VALUE (sizeof (BttInfo), EFI_BTT_ALIGNMENT);
  BttInfo.NextOff         = Arena->Size == SIZE_512GB ? Arena->Size : 0;
  BttInfo.Checksum        = CalculateFletcher64 ((UINT32 *)&BttInfo, BttInfo.InfoSize / sizeof (UINT32));

  Status = Btt->RawAccess (Btt->Context, TRUE, Arena->Base, BttInfo.InfoSize, &BttInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = Btt->RawAccess (Btt->Context, TRUE, Arena->Base + Arena->Size - BttInfo.InfoSize, BttInfo.InfoSize, &BttInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Write Flog
  //
  for (Index = 0, FlogRuntime = Arena->Flogs
    ; Index < Arena->NFree
    ; Index++, FlogRuntime++
    ) {
    Status = Btt->RawAccess (Btt->Context, TRUE, FlogRuntime->Offset, sizeof (FlogRuntime->Flog), &FlogRuntime->Flog);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // Write Map (zero)
  //
  UINT32 NumberOfMapPerCacheLine = CACHE_LINE_SIZE / sizeof (EFI_BTT_MAP_ENTRY);

  UINT32 NumberOfCacheLine = (Arena->ExternalNLba + NumberOfMapPerCacheLine - 1) / NumberOfMapPerCacheLine;
  ZeroMem (&BttInfo, CACHE_LINE_SIZE);
  for (Index = 0; NumberOfCacheLine; Index++) {
    Status = Btt->RawAccess (Btt->Context, TRUE, Arena->MapOff + Index * CACHE_LINE_SIZE, CACHE_LINE_SIZE, &BttInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS
BttLoadArena (
  BTT        *Btt,
  BTT_ARENA  *Arena,
  UINT64     Base,
  UINT64     Size
)
{
  EFI_STATUS           Status;
  UINT64               BackupOffset;
  EFI_BTT_INFO_BLOCK   BttInfo;
  BOOLEAN              BttValid;

  Status = Btt->RawAccess (Btt->Context, FALSE, Base, sizeof (EFI_BTT_INFO_BLOCK), &BttInfo);
  if (EFI_ERROR (Status)) {
    BttValid = FALSE;
  } else {
    DEBUG ((DEBUG_INFO, "Primary BTT @ %lx:\n", Base));
    DumpBttInfo (&BttInfo);
    BttValid = IsBttInfoValid (&BttInfo, Btt, Base, Size);
  }

  if (!BttValid) {
    BackupOffset = Base + Size - sizeof (EFI_BTT_INFO_BLOCK);
    Status = Btt->RawAccess (Btt->Context, FALSE, BackupOffset, sizeof (EFI_BTT_INFO_BLOCK), &BttInfo);
    if (EFI_ERROR (Status)) {
      BttValid = FALSE;
    } else {
      DEBUG ((DEBUG_INFO, "Backup BTT @ %lx:\n", BackupOffset));
      DumpBttInfo (&BttInfo);
      BttValid = IsBttInfoValid (&BttInfo, Btt, Base, Size);
    }

    //
    // Copy valid backup BTT INFO Block overwrite Primary one
    // Write may fail. Ignore it.
    //
    if (BttValid) {
      Btt->RawAccess (Btt->Context, TRUE, Base, sizeof (EFI_BTT_INFO_BLOCK), &BttInfo);
    }
  }

  if (!BttValid) {
    DEBUG ((DEBUG_ERROR, "BTT Infos are invalid!\n"));
    return EFI_INVALID_PARAMETER;
  }

  CopyGuid (&Arena->Uuid, &BttInfo.Uuid);
  Arena->Flags        = BttInfo.Flags;
  Arena->InternalNLba = BttInfo.InternalNLba;
  Arena->ExternalNLba = BttInfo.ExternalNLba;
  Arena->NFree        = BttInfo.NFree;
  Arena->Base         = Base;
  Arena->Size         = Size;
  Arena->FlogOff      = Base + BttInfo.FlogOff;
  Arena->MapOff       = Base + BttInfo.MapOff;
  Arena->DataOff      = Base + BttInfo.DataOff;

  Btt->ExternalNLba   += BttInfo.ExternalNLba;
  if (Base == 0) {
    Btt->ExternalLbaSize = BttInfo.ExternalLbaSize;
    Btt->InternalLbaSize = BttInfo.InternalLbaSize;
  }

  Status = BttLoadFlogs (Btt, Arena);
  //
  // Set the Arena error flag when fail to load Flog.
  // This allows read-only access.
  //
  if (EFI_ERROR (Status)) {
    Arena->Flags |= EFI_BTT_INFO_BLOCK_FLAGS_ERROR;
  }
  return EFI_SUCCESS;
}

VOID
EFIAPI
BttRelease (
  IN     BTT_HANDLE BttHandle
)
{
  UINTN             Index;
  BTT               *Btt;

  ASSERT (BttHandle != NULL);

  Btt = (BTT *)BttHandle;
  if (Btt->Arenas != NULL) {
    for (Index = 0; Index < Btt->NumberOfArenas; Index++) {
      if (Btt->Arenas[Index].Flogs != NULL) {
        FreePool (Btt->Arenas[Index].Flogs);
      }
    }
    FreePool (Btt->Arenas);
  }
  FreePool (Btt);
}

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
)
{
  EFI_STATUS           Status;
  UINT64               Remainder;
  UINT64               Base;
  UINT64               Size;
  UINT32               Index;
  BTT                  *Btt;

  if ((BttHandle == NULL) || (*TotalSize < SIZE_16MB) || (ParentUuid == NULL) || (RawAccess == NULL) || (NFree == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Btt = AllocateZeroPool (sizeof (BTT));
  if (Btt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&Btt->ParentUuid, ParentUuid);
  Btt->RawSize   = *TotalSize;
  Btt->RawAccess = RawAccess;
  Btt->Context   = Context;
  //
  // Safe to use UINT32 type for BttInfoCount: MAX_UINT64 / SIZE_512 < MAX_UINT32
  //
  Btt->NumberOfArenas = (UINT32)DivU64x64Remainder (Btt->RawSize, SIZE_512GB, &Remainder);
  if (Remainder >= SIZE_16MB) {
    Btt->NumberOfArenas++;
  }
  Btt->Arenas = AllocatePool (sizeof (*Btt->Arenas) * Btt->NumberOfArenas);
  if (Btt->Arenas == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for Arenas array!\n"));
    FreePool (Btt);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // InternalLbaSize must be >= 512 and align on 64-byte cache line.
  //
  Btt->ExternalLbaSize = ExternalLbaSize;
  Btt->InternalLbaSize = ALIGN_VALUE (Btt->ExternalLbaSize, CACHE_LINE_SIZE);
  Btt->InternalLbaSize = MAX (512, Btt->InternalLbaSize);
  Btt->ExternalNLba    = 0;
  for (Index = 0; Index < Btt->NumberOfArenas; Index++) {
    Base = MultU64x32 (SIZE_512GB, Index);
    Size = MIN (Btt->RawSize - Base, SIZE_512GB);
    if ((Size & (EFI_BTT_ALIGNMENT - 1)) != 0) {
      Size -= (Size & (EFI_BTT_ALIGNMENT - 1));
    }
    Status = BttInitializeArena (&Btt->Arenas[Index], NFree, Btt->InternalLbaSize, Base, Size);
    if (EFI_ERROR (Status)) {
      break;
    }
    Btt->ExternalNLba += Btt->Arenas[Index].ExternalNLba;
  }
  if (Index != Btt->NumberOfArenas) {
    BttRelease ((BTT_HANDLE)Btt);
    return EFI_INVALID_PARAMETER;
  }
  *BlockSize = Btt->ExternalLbaSize;
  *TotalSize = MultU64x32 (Btt->ExternalNLba, Btt->ExternalLbaSize);
  *BttHandle = (BTT_HANDLE)Btt;

  Status = EFI_SUCCESS;
  for (Index = 0; Index < Btt->NumberOfArenas; Index++) {
    Status = BttWriteArena (Btt, &Btt->Arenas[Index]);
    if (EFI_ERROR (Status)) {
      break;
    }
  }
  return Status;
}

EFI_STATUS
EFIAPI
BttLoad (
  OUT    BTT_HANDLE     *BttHandle,
  IN     GUID           *ParentUuid,
  IN OUT UINT64         *TotalSize,
  IN OUT UINT32         *BlockSize,
  IN     BTT_RAW_ACCESS RawAccess,
  IN     VOID           *Context
  )
{
  EFI_STATUS           Status;
  UINT64               Remainder;
  UINT64               Base;
  UINT64               Size;
  UINT32               Index;
  BTT                  *Btt;

  if ((BttHandle == NULL) || (*TotalSize < SIZE_16MB) || (ParentUuid == NULL) || (RawAccess == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Btt = AllocateZeroPool (sizeof (BTT));
  if (Btt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&Btt->ParentUuid, ParentUuid);
  Btt->RawSize   = *TotalSize;
  Btt->RawAccess = RawAccess;
  Btt->Context   = Context;
  //
  // Safe to use UINT32 type for BttInfoCount: MAX_UINT64 / SIZE_512 < MAX_UINT32
  //
  Btt->NumberOfArenas = (UINT32)DivU64x64Remainder (Btt->RawSize, SIZE_512GB, &Remainder);
  if (Remainder >= SIZE_16MB) {
    Btt->NumberOfArenas++;
  }
  Btt->Arenas = AllocatePool (sizeof (*Btt->Arenas) * Btt->NumberOfArenas);
  if (Btt->Arenas == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for Arenas array!\n"));
    FreePool (Btt);
    return EFI_OUT_OF_RESOURCES;
  }
  for (Index = 0; Index < Btt->NumberOfArenas; Index++) {
    Base = MultU64x32 (SIZE_512GB, Index);
    Size = MIN (Btt->RawSize - Base, SIZE_512GB);
    if ((Size & (EFI_BTT_ALIGNMENT - 1)) != 0) {
      Size -= (Size & (EFI_BTT_ALIGNMENT - 1));
    }
    Status = BttLoadArena (Btt, &Btt->Arenas[Index], Base, Size);
    if (EFI_ERROR (Status)) {
      break;
    }
  }
  if (Index != Btt->NumberOfArenas) {
    BttRelease ((BTT_HANDLE)Btt);
    return EFI_INVALID_PARAMETER;
  }
  *BlockSize = Btt->ExternalLbaSize;
  *TotalSize = MultU64x32 (Btt->ExternalNLba, *BlockSize);
  *BttHandle = (BTT_HANDLE)Btt;
  return EFI_SUCCESS;
}

EFI_STATUS
BttLbaToArenaLba(
  IN     BTT       *Btt,
  IN     UINT64    Lba,
     OUT BTT_ARENA **Arena,
     OUT UINT32    *ArenaLba
  )
{
  UINT32  Index;

  ASSERT (Btt != NULL);
  ASSERT (Arena != NULL);

  for(Index = 0; Index < Btt->NumberOfArenas; Index++) {
    if(Lba < Btt->Arenas [Index].ExternalNLba) {
      break;
    } else {
      Lba -= Btt->Arenas [Index].ExternalNLba;
    }
  }

  if(Index == Btt->NumberOfArenas) {
    return EFI_INVALID_PARAMETER;
  }

  *Arena    = &Btt->Arenas [Index];
  *ArenaLba = (UINT32)Lba;

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS        Status;
  BTT               *Btt;
  BTT_ARENA         *Arena;
  UINT32            PreMapLba;
  UINT64            MapOffset;
  UINT32            Remainder;
  UINT8             CacheLine[CACHE_LINE_SIZE];
  EFI_BTT_MAP_ENTRY *Map;
  UINT32            PostMapLba;
  UINT64            DataOffset;

  ASSERT (BttHandle != NULL);

  Btt = (BTT *)BttHandle;
  if (Lba >= Btt->ExternalNLba) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Convert Lba to Lba in the arena
  //
  Status = BttLbaToArenaLba(Btt, Lba, &Arena, &PreMapLba);
  ASSERT_EFI_ERROR (Status);

  //
  // Get the map
  //
  MapOffset  = Arena->MapOff + sizeof(EFI_BTT_MAP_ENTRY) * PreMapLba;
  Remainder  = MapOffset & (CACHE_LINE_SIZE - 1);
  MapOffset -= Remainder;
  Status = Btt->RawAccess (Btt->Context, FALSE, MapOffset, sizeof (CacheLine), CacheLine);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Map = (EFI_BTT_MAP_ENTRY *)&CacheLine[Remainder];

  //
  // Error Zero
  //  1     1     -> Normal map entry
  //  1     0     -> Error
  //  0     1     -> Zero
  //  0     0     -> Identify mapping
  //
  if (Map->Error == 1) {
    if (Map->Zero == 0) {
      //
      // Error entry
      //
      return EFI_ABORTED;
    } else {
      //
      // Normal entry
      //
      PostMapLba = Map->PostMapLba;
    }
  } else {
    if (Map->Zero == 1) {
      //
      // Zero entry
      //
      ZeroMem (Buffer, Btt->ExternalLbaSize);
      return EFI_SUCCESS;
    } else {
      //
      // Identity mapping
      //
      PostMapLba = PreMapLba;
    }
  }

  DataOffset = Arena->DataOff + MultU64x32 (PostMapLba, Btt->InternalLbaSize);
  DEBUG ((DEBUG_INFO, "Lba = %lx -> LbaBtt = %x, Offset[B] = %lx", Lba, PostMapLba, DataOffset));

  return Btt->RawAccess (Btt->Context, FALSE, DataOffset, Btt->ExternalLbaSize, Buffer);
}


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
  )
{
  EFI_STATUS        Status;
  BTT               *Btt;
  BTT_ARENA         *Arena;
  FLOG_RUNTIME      *FlogRuntime;
  FLOG              *ActiveFlog;
  FLOG              *InactiveFlog;
  UINT32            PreMapLba;
  UINT64            MapOffset;
  UINT64            DataOffset;
  UINT32            Remainder;
  UINT8             CacheLine[CACHE_LINE_SIZE];
  EFI_BTT_MAP_ENTRY *Map;
  EFI_BTT_MAP_ENTRY *FreeMap;

  ASSERT (BttHandle != NULL);

  Btt = (BTT *)BttHandle;
  if (Lba >= Btt->ExternalNLba) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Convert Lba to Lba in the arena
  //
  Status = BttLbaToArenaLba (Btt, Lba, &Arena, &PreMapLba);
  ASSERT_EFI_ERROR (Status);

  //
  // Arena in error state is read-only.
  //
  if ((Arena->Flags & EFI_BTT_INFO_BLOCK_FLAGS_ERROR) != 0) {
    DEBUG ((DEBUG_ERROR, "Arena cannot be written: Flag = %x!\n", Arena->Flags));
    return EFI_ABORTED;
  }

  FlogRuntime  = &Arena->Flogs[0];
  ActiveFlog   = &FlogRuntime->Flog[FlogRuntime->Active];
  InactiveFlog = &FlogRuntime->Flog[1 - FlogRuntime->Active];

  //
  // 1. Write data to free block first. (Old data will be used when this is interrupted.)
  //
  FreeMap = (EFI_BTT_MAP_ENTRY *)&ActiveFlog->OldMap;
  ASSERT ((FreeMap->Zero == 1) && (FreeMap->Error == 1) && (FreeMap->PostMapLba >= Arena->ExternalNLba));
  DataOffset = Arena->DataOff + MultU64x32 (FreeMap->PostMapLba, Btt->InternalLbaSize);
  DEBUG ((DEBUG_INFO, "LBA=%lx -> LBAbtt=%x, Offset[B]=%lx", Lba, FreeMap->PostMapLba, DataOffset));
  Status = Btt->RawAccess (Btt->Context, TRUE, DataOffset, Btt->ExternalLbaSize, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // 2. Get the original LBA mapping.
  //
  MapOffset = Arena->MapOff + sizeof (EFI_BTT_MAP_ENTRY) * PreMapLba;
  Remainder = MapOffset & (CACHE_LINE_SIZE - 1);
  MapOffset -= Remainder;
  Status = Btt->RawAccess (Btt->Context, FALSE, MapOffset, sizeof (CacheLine), CacheLine);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Map = (EFI_BTT_MAP_ENTRY *)&CacheLine[Remainder];
  if ((Map->Error == 0) && (Map->Zero == 0)) {
    //
    // Identity mapping
    //
    Map->Error      = 1;
    Map->Zero       = 1;
    Map->PostMapLba = PreMapLba;
  }

  //
  // 3. Update inactive Flog, update Lba/OldMap/NewMap and Seq in two steps.
  //   3.1. Update inactive Flog.OldMap to point to retired internal LBA (retrieved from original mapping).
  //   3.2. Update inactive Flog.NewMap to point to free LBA (we just wrote data to that block).
  //
  InactiveFlog->Lba    = PreMapLba;
  InactiveFlog->OldMap = *(UINT32 *)&Map;
  InactiveFlog->NewMap = *(UINT32 *)&FreeMap;
  Status = Btt->RawAccess (Btt->Context, TRUE, FlogRuntime->Offset, sizeof (FlogRuntime->Flog), &FlogRuntime->Flog);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  //
  //   3.3. Update inactive Flog.Seq to activate the Flog.
  // Active flog's sequence number shouldn't be 0.
  // This make sure correct next sequence number is assigned to original inactive flog.
  //
  ASSERT (ActiveFlog->Seq != 0);
  InactiveFlog->Seq = mSeqNext[ActiveFlog->Seq];
  Status = Btt->RawAccess (Btt->Context, TRUE, FlogRuntime->Offset, sizeof (FlogRuntime->Flog), &FlogRuntime->Flog);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  //   3.4. Update the active state stored in memory.
  //
  FlogRuntime->Active = 1 - FlogRuntime->Active;

  //
  // 4. Update the LBA mapping to point to new internal LBA.
  //
  *Map = *FreeMap;
  return Btt->RawAccess (Btt->Context, FALSE, MapOffset, sizeof (CacheLine), CacheLine);
}
