#include <Base.h>
#include <Library/CpuPageTableLib.h>
#undef NULL
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>

#define DivU64x32(a, b)  ((a) / (b))
#define LShiftU64(a, b)  ((UINT64)(a) << (b))

/**
  Return a boolean.

  @return boolean
**/
BOOLEAN
RandomBoolean (
  VOID
  )
{
  return (BOOLEAN)(rand () > RAND_MAX / 2);
}

/**
  Return a 32bit random number.

  @param Start  Start of the random number range.
  @param Limit  Limit of the random number range.
  @return 32bit random number
**/
UINT32
Random32 (
  UINT32  Start,
  UINT32  Limit
  )
{
  return (UINT32)(((double)rand () / RAND_MAX) * (Limit - Start)) + Start;
}

/**
  Return a 64bit random number.

  @param Start  Start of the random number range.
  @param Limit  Limit of the random number range.
  @return 64bit random number
**/
UINT64
Random64 (
  UINT64  Start,
  UINT64  Limit
  )
{
  return (UINT64)(((double)rand () / RAND_MAX) * (Limit - Start)) + Start;
}

VOID
GenerateRandomMapEntry (
  OUT IA32_MAP_ENTRY  *Map,
  IN  UINTN           *MapCount
  )
{
  CONST UINT64  MaxAddress = LShiftU64 (1, 52);
  UINTN         Index;
  UINT64        AvgRangeSize;
  UINT64        LinearAddress;
  UINTN         NewIndex;

  AvgRangeSize = DivU64x32 (MaxAddress, (UINT32)*MapCount);

  LinearAddress = 0;

  for (Index = 0; Index < *MapCount && LinearAddress < MaxAddress; ) {
    Map[Index].LinearAddress = LinearAddress;
    Map[Index].Length          = Random64 (SIZE_4KB, AvgRangeSize) & ~0xFFFull;
    if (Map[Index].Length > MaxAddress - Map[Index].LinearAddress) {
      Map[Index].Length = MaxAddress - Map[Index].LinearAddress;
    }

    LinearAddress = Map[Index].LinearAddress + Map[Index].Length;

    if (RandomBoolean ()) {
      //
      // Randomly skip mapping certain range.
      //
      continue;
    }

    Map[Index].Attribute.Uint64             = Map[Index].LinearAddress;
    Map[Index].Attribute.Bits.Present       = 1;
    Map[Index].Attribute.Bits.ReadWrite     = RandomBoolean ();
    Map[Index].Attribute.Bits.WriteThrough  = RandomBoolean ();
    Map[Index].Attribute.Bits.CacheDisabled = RandomBoolean ();
    Map[Index].Attribute.Bits.Pat           = RandomBoolean ();
    Map[Index].Attribute.Bits.Nx            = RandomBoolean ();

    Index++;
  }

  *MapCount = Index;
  if (*MapCount == 0) {
    return;
  }

  //
  // Combine adjacent ranges
  //
  for (NewIndex = 0, Index = 1; Index < *MapCount; Index++) {
    if ((Map[Index].LinearAddress == Map[NewIndex].LinearAddress + Map[NewIndex].Length) &&
        (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[NewIndex].Attribute)) &&
        (IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[NewIndex].Attribute) + Map[NewIndex].Length)
        )
    {
      Map[NewIndex].Length += Map[Index].Length;
    } else {
      //
      // If every time else is hit, NewIndex = Index - 1, Index < *MapCount
      // so, below memcpy doesn't access outside.
      //
      NewIndex++;
      memcpy (&Map[NewIndex], &Map[Index], sizeof (IA32_MAP_ENTRY));
    }
  }

  *MapCount = NewIndex + 1;
}

VOID
DumpMapEntry (
  UINTN           MapIndex,
  IA32_MAP_ENTRY  *MapEntry
  )
{
  printf (
    "  %02d: {0x%016llx, 0x%016llx, 0x%016llx}\n",
    (UINT32)MapIndex,
    MapEntry->LinearAddress,
    MapEntry->LinearAddress + MapEntry->Length,
    MapEntry->Attribute.Uint64
    );
}

VOID
DumpMap (
  IA32_MAP_ENTRY  *Map,
  UINTN           MapCount
  )
{
  UINTN  Index;

  for (Index = 0; Index < MapCount; Index++) {
    DumpMapEntry (Index, &Map[Index]);
  }
}

BOOLEAN
FuzzyTest (
  VOID   *Buffer,
  UINTN  BufferSize
  )
{
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      SrcMap[50];
  IA32_MAP_ATTRIBUTE  MapMask;
  UINTN               SrcMapCount;
  IA32_MAP_ENTRY      Map[100];
  UINTN               MapCount;
  UINTN               Index;
  UINTN               PageTable;

  SrcMapCount = Random32 (1, ARRAY_SIZE (SrcMap));
  GenerateRandomMapEntry (SrcMap, &SrcMapCount);
  MapMask.Uint64 = MAX_UINT64;

  PageTable = 0;
  for (Index = 0; Index < SrcMapCount; Index++) {
    Status = PageTableMap (
               &PageTable,
               Buffer,
               &BufferSize,
               TRUE,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Length,
               &SrcMap[Index].Attribute,
               &MapMask
               );
    assert (Status == RETURN_SUCCESS);
  }

  MapCount = ARRAY_SIZE (Map);
  Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
  assert (Status == RETURN_SUCCESS);
  if ((SrcMapCount != MapCount) || (memcmp (SrcMap, Map, MapCount * sizeof (IA32_MAP_ENTRY)) != 0)) {
    printf ("FAIL:\n");
    if (SrcMapCount != MapCount) {
      printf ("  SrcMapCount/MapCount = %d/%d\n", (UINT32)SrcMapCount, (UINT32)MapCount);
      DumpMap (SrcMap, SrcMapCount);
      printf ("--------------------------\n");
      DumpMap (Map, MapCount);
    } else {
      for (Index = 0; Index < MapCount; Index++) {
        if (memcmp (&SrcMap[Index], &Map[Index], sizeof (IA32_MAP_ENTRY)) != 0) {
          DumpMapEntry (Index, &SrcMap[Index]);
          DumpMapEntry (Index, &Map[Index]);
          printf ("--------------------------\n");
        }
      }
    }

    return FALSE;
  } else {
    printf ("PASS!!!\n");
    return TRUE;
  }
}

VOID
StaticTest (
  VOID   *Buffer,
  UINTN  BufferSize
  )
{
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      SrcMap[20];
  IA32_MAP_ENTRY      Map[100];
  IA32_MAP_ATTRIBUTE  MapAttribute, MapMask;
  UINTN               MapCount;
  UINTN               Index;
  UINTN               PageTable;
  UINT64              MapArray[][3] = {
    { 0x0000000000000000, 0x0000f6574203d000, 0x8000000000000001 },
    { 0x0001d7b65a175000, 0x0002e115c22b7000, 0x0001d7b65a175019 },
    { 0x00040a6814d01000, 0x00050bef6d341000, 0x00040a6814d01001 },
    { 0x00060c1418281000, 0x000674ca3ee9b000, 0x00060c1418281001 },
    { 0x000674ca3ee9b000, 0x00076486c90d7000, 0x000674ca3ee9b091 },
    { 0x00076486c90d7000, 0x0009093cbd240000, 0x00076486c90d7011 },
    { 0x0009093cbd240000, 0x000a0ae16b180000, 0x0009093cbd240013 },
    { 0x000a0ae16b180000, 0x000a0b216b981000, 0x000a0ae16b180003 },
    { 0x000a0b216b981000, 0x000a2ee45dc8a000, 0x000a0b216b981083 },
    { 0x000a2ee45dc8a000, 0x000ab3a2bc9ab000, 0x000a2ee45dc8a001 },
    { 0x000c4f6b49812000, 0x000c673379118000, 0x000c4f6b49812081 },
  };

  for (Index = 0; Index < ARRAY_SIZE (MapArray); Index++) {
    SrcMap[Index].LinearAddress    = MapArray[Index][0];
    SrcMap[Index].Length             = MapArray[Index][1] - MapArray[Index][0];
    SrcMap[Index].Attribute.Uint64 = MapArray[Index][2];
  }

  MapMask.Uint64 = MAX_UINT64;

  PageTable = 0;
  for (Index = 0; Index < ARRAY_SIZE (MapArray); Index++) {
    Status = PageTableMap (
               &PageTable,
               Buffer,
               &BufferSize,
               TRUE,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Length,
               &SrcMap[Index].Attribute,
               &MapMask
               );
    assert (Status == RETURN_SUCCESS);

    MapCount = ARRAY_SIZE (Map);
    Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
    printf ("PageTableParse returns with status = %llx\n", Status);
    DumpMap (Map, MapCount);
  }

  MapAttribute.Uint64         = 0;
  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 1;

  MapMask.Uint64                    = 0;
  MapMask.Bits.Present              = 1;
  MapMask.Bits.ReadWrite            = 1;
  MapMask.Bits.PageTableBaseAddress = 1;
  MapMask.Bits.Nx                   = 1;
  MapMask.Bits.CacheDisabled        = 1;

  Status = PageTableMap (&PageTable, Buffer, &BufferSize, TRUE, 0, SIZE_4GB, &MapAttribute, &MapMask);
  assert (Status == 0);

  MapCount = ARRAY_SIZE (Map);
  Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
  printf ("PageTableParse returns with status = %llx\n", Status);
  DumpMap (Map, MapCount);
  assert (MapCount == 1);
  assert (Map[0].LinearAddress == 0);
  assert (Map[0].Length == SIZE_4GB);
  IA32_MAP_ATTRIBUTE  MapAttribute4G;

  MapAttribute4G.Uint64         = 0;
  MapAttribute4G.Bits.Present   = 1;
  MapAttribute4G.Bits.ReadWrite = 1;
  assert (MapAttribute.Uint64 == Map[0].Attribute.Uint64);

  MapAttribute.Bits.Present = 0;
  Status                    = PageTableMap (&PageTable, Buffer, &BufferSize, TRUE, 0x60000, 0xA0000 - 0x60000, &MapAttribute, &MapMask);
  assert (Status == 0);

  MapAttribute.Bits.ReadWrite = 0;
  MapAttribute.Bits.Nx        = 1;
  Status                      = PageTableMap (&PageTable, Buffer, &BufferSize, TRUE, 0x17ca00000, 0x7ca0000000 - 0x17ca00000, &MapAttribute, &MapMask);
  assert (Status == 0);
  MapCount = ARRAY_SIZE (Map);
  Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
  printf ("PageTableParse returns with status = %llx\n", Status);
  DumpMap (Map, MapCount);
}

#define FUZZY
int
main (
  VOID
  )
{
  UINT32  Count;
  UINTN   BufferSize;
  VOID    *Buffer;
  UINTN   PassCount;

  srand ((unsigned int)time (NULL));

  BufferSize = SIZE_32MB;
  Buffer     = _aligned_malloc ((size_t)BufferSize, SIZE_4KB);
  Count      = 0;
  PassCount  = 0;

 #ifdef FUZZY
  while (Count++ < 1000) {
    printf ("FuzzyTest:%d\n", Count);
    if (FuzzyTest (Buffer, BufferSize)) {
      PassCount++;
    }

    printf ("=========== Pass Rate = %.2f%% (%d / %d) ============================\n", (double)PassCount * 100 / Count, (UINT32)PassCount, (UINT32)Count);
  }

 #else
  StaticTest (Buffer, BufferSize);
 #endif

  return 0;
}
