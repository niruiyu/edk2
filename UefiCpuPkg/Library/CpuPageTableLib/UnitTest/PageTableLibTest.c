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
MergeMap(
  IA32_MAP_ENTRY *Map,
  UINTN          *MapCount
)
{
  UINTN         NewIndex;
  UINTN         Index;

  if (*MapCount == 0) {
    return;
  }
  //
  // Combine adjacent ranges
  //
  for (NewIndex = 0, Index = 1; Index < *MapCount; Index++) {
    if ((Map[Index].LinearAddress == Map[NewIndex].LinearAddress + Map[NewIndex].Length) &&
      (IA32_MAP_ATTRIBUTE_ATTRIBUTES(&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_ATTRIBUTES(&Map[NewIndex].Attribute)) &&
      (IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS(&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS(&Map[NewIndex].Attribute) + Map[NewIndex].Length)
      )
    {
      Map[NewIndex].Length += Map[Index].Length;
    }
    else {
      //
      // If every time else is hit, NewIndex = Index - 1, Index < *MapCount
      // so, below memcpy doesn't access outside.
      //
      NewIndex++;
      memcpy(&Map[NewIndex], &Map[Index], sizeof(IA32_MAP_ENTRY));
    }
  }

  *MapCount = NewIndex + 1;
}

VOID
GenerateRandomMapEntry (
  IN  UINT64          MaxAddress,
  OUT IA32_MAP_ENTRY  *Map,
  IN  UINTN           *MapCount,
  IN  UINT64          AddressMask
  )
{
  UINTN         Index;
  UINT64        AvgRangeSize;
  UINT64        LinearAddress;

  AvgRangeSize = DivU64x32 (MaxAddress, (UINT32)*MapCount);

  LinearAddress = 0;

  for (Index = 0; Index < *MapCount && LinearAddress < MaxAddress; ) {
    Map[Index].LinearAddress = LinearAddress;
    Map[Index].Length        = Random64 (SIZE_4KB, AvgRangeSize) & AddressMask;
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
  
  MergeMap(Map, MapCount);
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

int
CompareMapEntry (
  IA32_MAP_ENTRY  *Left,
  IA32_MAP_ENTRY  *Right
  )
{
  if (Left->LinearAddress < Right->LinearAddress) {
    return -1;
  } else if (Left->LinearAddress == Right->LinearAddress) {
    return 0;
  } else {
    return 1;
  }
}

RETURN_STATUS
SetMemoryType (
  IN IA32_MAP_ENTRY  *Map,
  IN UINTN           Capacity,
  IN OUT UINTN       *Count,
  IN UINT64          BaseAddress,
  IN UINT64          Length,
  IN UINT64          Attribute
  )
{
  UINTN   Index;
  UINT64  Limit;
  UINT64  LengthLeft;
  UINT64  LengthRight;
  UINTN   StartIndex;
  UINTN   EndIndex;
  UINTN   DeltaCount;
  IA32_MAP_ENTRY OrgEndEntry;

  LengthRight = 0;
  LengthLeft  = 0;
  Limit       = BaseAddress + Length;
  StartIndex  = *Count;
  EndIndex    = *Count;
  for (Index = 0; Index < *Count; Index++) {
    if ((StartIndex == *Count) &&
        (Map[Index].LinearAddress <= BaseAddress) &&
        (BaseAddress < Map[Index].LinearAddress + Map[Index].Length))
    {
      StartIndex = Index;
      LengthLeft = BaseAddress - Map[Index].LinearAddress;
    }

    if ((EndIndex == *Count) &&
        (Map[Index].LinearAddress < Limit) &&
        (Limit <= Map[Index].LinearAddress + Map[Index].Length))
    {
      EndIndex    = Index;
      LengthRight = Map[Index].LinearAddress + Map[Index].Length - Limit;
      break;
    }
  }

  assert (StartIndex != *Count && EndIndex != *Count);
  if ((StartIndex == EndIndex) && (Map[StartIndex].Attribute.Uint64 == Attribute)) {
    return RETURN_ALREADY_STARTED;
  }

  //
  // The type change may cause merging with previous range or next range.
  // Update the StartIndex, EndIndex, BaseAddress, Length so that following
  // logic doesn't need to consider merging.
  //
  if (StartIndex != 0) {
    if ((LengthLeft == 0) && (Map[StartIndex - 1].Attribute.Uint64 == Attribute)) {
      StartIndex--;
      Length      += Map[StartIndex].Length;
      BaseAddress -= Map[StartIndex].Length;
      Attribute   -= Map[StartIndex].Length;
    }
  }

  if (EndIndex != (*Count) - 1) {
    if ((LengthRight == 0) && (Map[EndIndex + 1].Attribute.Uint64 == Attribute)) {
      EndIndex++;
      Length += Map[EndIndex].Length;
    }
  }

  //
  // |- 0 -|- 1 -|- 2 -|- 3 -| StartIndex EndIndex DeltaCount  Count (Count = 4)
  //   |++++++++++++++++++|    0          3         1=3-0-2    3
  //   |+++++++|               0          1        -1=1-0-2    5
  //   |+|                     0          0        -2=0-0-2    6
  // |+++|                     0          0        -1=0-0-2+1  5
  //
  //
  DeltaCount = EndIndex - StartIndex - 2;
  if (LengthLeft == 0) {
    DeltaCount++;
  }

  if (LengthRight == 0) {
    DeltaCount++;
  }

  if (*Count - DeltaCount > Capacity) {
    return RETURN_OUT_OF_RESOURCES;
  }

  //
  // Reserve (-DeltaCount) space
  //
  memcpy (&OrgEndEntry, &Map[EndIndex], sizeof(IA32_MAP_ENTRY));
  memcpy (&Map[EndIndex + 1 - DeltaCount], &Map[EndIndex + 1], (*Count - EndIndex - 1) * sizeof (Map[0]));
  *Count -= DeltaCount;

  if (LengthLeft != 0) {
    Map[StartIndex].Length = LengthLeft;
    StartIndex++;
  }

  if (LengthRight != 0) {
    Map[EndIndex - DeltaCount].Attribute.Uint64 = BaseAddress + Length + OrgEndEntry.Attribute.Uint64 - OrgEndEntry.LinearAddress;
    Map[EndIndex - DeltaCount].LinearAddress    = BaseAddress + Length;
    Map[EndIndex - DeltaCount].Length           = LengthRight;
  }

  Map[StartIndex].LinearAddress    = BaseAddress;
  Map[StartIndex].Length           = Length;
  Map[StartIndex].Attribute.Uint64 = Attribute;
  return RETURN_SUCCESS;
}

IA32_MAP_ENTRY *
NormalizeMap (
  UINT64          MaxAddress,
  IA32_MAP_ENTRY  *Map,
  UINTN           *MapCount
  )
{
  IA32_MAP_ENTRY  *DstMap;
  UINTN           Index;
  UINTN           DstMapCount;

  DstMapCount                = 1;
  DstMap                     = malloc ((*MapCount * 5) * sizeof (IA32_MAP_ENTRY));
  DstMap[0].LinearAddress    = 0;
  DstMap[0].Length           = MaxAddress;
  DstMap[0].Attribute.Uint64 = 0;
  for (Index = 0; Index < *MapCount; Index++) {
    SetMemoryType (DstMap, *MapCount * 3, &DstMapCount, Map[Index].LinearAddress, Map[Index].Length, Map[Index].Attribute.Uint64);
  }

  for (Index = 0; Index < DstMapCount; Index++) {
    if (DstMap[Index].Attribute.Bits.Present == 0) {
      memcpy (&DstMap[Index], &DstMap[Index + 1], (DstMapCount - Index) * sizeof (IA32_MAP_ENTRY));
      DstMapCount--;
    }
  }

  *MapCount = DstMapCount;


  MergeMap(DstMap, MapCount);

  return DstMap;
}

UINT64
GetMaxAddress(
  PAGING_MODE  Mode
)
{
  switch (Mode) {
  case Paging32bit:
  case PagingPae:
    return SIZE_4GB;

  case Paging4Level:
  case Paging4Level1GB:
  case Paging5Level:
  case Paging5Level1GB:
    return 1ull << MIN(12 + (Mode >> 8) * 9, 52);

  default:
    assert(0);
    return 0;
  }
}

BOOLEAN
FuzzyTest (
  PAGING_MODE  Mode,
  VOID   *Buffer,
  UINTN  BufferSize
  )
{
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      SrcMap[10], *NormalizedMap;
  IA32_MAP_ATTRIBUTE  MapMask;
  UINTN               SrcMapCount;
  UINTN               UseMapCount;
  IA32_MAP_ENTRY      *Map;
  UINTN               MapCount;
  UINTN               Index;
  UINTN               PageTable;
  UINTN               AddressMaskIndex;
  BOOLEAN             Identical;
  UINT64              MaxAddress;

  UINT64              AddressMask[] = {
    ~(SIZE_1GB - 1),
    ~(SIZE_2MB - 1),
    ~(SIZE_4KB - 1)
  };
  UINTN               AddressMaskUsed[] = {
    FALSE,
    FALSE,
    FALSE
  };
  MaxAddress = GetMaxAddress(Mode);

  SrcMapCount = 0;
  for (Index = 0; Index < ARRAY_SIZE(AddressMask); Index++) {
    do {
      AddressMaskIndex = Random32(0, ARRAY_SIZE(AddressMask) - 1);
    } while (AddressMaskUsed[AddressMaskIndex]);
    AddressMaskUsed[AddressMaskIndex] = TRUE;
    //
    // UseMapCount will be random number between following ranges:
    //   [1, AvailableMapCount/3]
    //   [1, AvailableMapCount/2]
    //   [1, AvailableMapCount/1]
    //
    UseMapCount = (UINTN) Random32(0, (UINT32) (ARRAY_SIZE(SrcMap) - SrcMapCount) / (ARRAY_SIZE(AddressMask) - Index));
    if (UseMapCount == 0) {
      UseMapCount = 1;
    }
    GenerateRandomMapEntry(MaxAddress, &SrcMap[SrcMapCount], &UseMapCount, AddressMask[AddressMaskIndex]);
    SrcMapCount += UseMapCount;
  }
  MapMask.Uint64 = MAX_UINT64;

  DumpMap (SrcMap, SrcMapCount);
  PageTable = 0;
  for (Index = 0; Index < SrcMapCount; Index++) {
    UINTN  RequiredSize = 0;
    Status = PageTableMap (
               &PageTable,
               Mode,
               NULL,
               &RequiredSize,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Length,
               &SrcMap[Index].Attribute,
               &MapMask
               );
    assert (Status == RETURN_SUCCESS || Status == RETURN_BUFFER_TOO_SMALL);

    UINTN  OriginalSize = BufferSize;
    Status = PageTableMap (
               &PageTable,
               Mode,
               Buffer,
               &BufferSize,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Length,
               &SrcMap[Index].Attribute,
               &MapMask
               );
    assert (Status == RETURN_SUCCESS);
    assert (OriginalSize - BufferSize == RequiredSize);
  }

  NormalizedMap = NormalizeMap (MaxAddress, SrcMap, &SrcMapCount);

  MapCount = 0;
  Status = PageTableParse(PageTable, Mode, NULL, &MapCount);
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    assert(MapCount != 0);
    Map = malloc(MapCount * sizeof(IA32_MAP_ENTRY));
    Status = PageTableParse(PageTable, Mode, Map, &MapCount);
  }
  assert (Status == RETURN_SUCCESS);
  if ((SrcMapCount != MapCount) || (memcmp (NormalizedMap, Map, MapCount * sizeof (IA32_MAP_ENTRY)) != 0)) {
    printf ("FAIL:\n");
    if (SrcMapCount != MapCount) {
      printf ("  SrcMapCount/MapCount = %d/%d\n", (UINT32)SrcMapCount, (UINT32)MapCount);
      DumpMap (NormalizedMap, SrcMapCount);
      printf ("--------------------------\n");
      DumpMap (Map, MapCount);
    } else {
      for (Index = 0; Index < MapCount; Index++) {
        if (memcmp (&NormalizedMap[Index], &Map[Index], sizeof (IA32_MAP_ENTRY)) != 0) {
          DumpMapEntry (Index, &NormalizedMap[Index]);
          DumpMapEntry (Index, &Map[Index]);
          printf ("--------------------------\n");
        }
      }
    }

    Identical = FALSE;
  } else {
    printf ("PASS!!!\n");
    Identical = TRUE;
  }
  free(NormalizedMap);
  free(Map);
  return Identical;
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
  IA32_MAP_ATTRIBUTE  MapMask;
  UINTN               MapCount;
  UINTN               Index;
  UINTN               PageTable;
  UINT64              MapArray[][3] = {
    {0x0000000000000000, 0x00017bc2c0000000, 0x0000000000000083},
    {0x00017bc2c0000000, 0x000242f440000000, 0x80017bc2c000000b},
    {0x000bbbc777600000, 0x000dae2b5c200000, 0x000bbbc777600081},
    {0x000dae2b5c200000, 0x0010000000000000, 0x000dae2b5c200019},
    {0x0000000000000000, 0x00045dde11117000, 0x800000000000009b},
    {0x0005ce2b9c573000, 0x000612b6d0185000, 0x8005ce2b9c573009},
    {0x000612b6d0185000, 0x000a0ea972a84000, 0x800612b6d0185091},
    };

  for (Index = 0; Index < ARRAY_SIZE (MapArray); Index++) {
    SrcMap[Index].LinearAddress    = MapArray[Index][0];
    SrcMap[Index].Length           = MapArray[Index][1] - MapArray[Index][0];
    SrcMap[Index].Attribute.Uint64 = MapArray[Index][2];
  }
  MapCount = ARRAY_SIZE(MapArray);
  IA32_MAP_ENTRY* NormalizedMap = NormalizeMap(1ull << 52, SrcMap, &MapCount);

  MapMask.Uint64 = MAX_UINT64;

  PageTable = 0;
  for (Index = 0; Index < ARRAY_SIZE (MapArray); Index++) {
    UINTN  RequiredSize = 0;
    Status = PageTableMap (
               &PageTable,
               5,
               NULL,
               &RequiredSize,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Length,
               &SrcMap[Index].Attribute,
               &MapMask
               );
    assert (Status == RETURN_SUCCESS || Status == RETURN_BUFFER_TOO_SMALL);

    UINTN  OriginalSize = BufferSize;
    Status = PageTableMap (
               &PageTable,
               5,
               Buffer,
               &BufferSize,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Length,
               &SrcMap[Index].Attribute,
               &MapMask
               );
    assert (Status == RETURN_SUCCESS);

    assert (OriginalSize - BufferSize == RequiredSize);

    MapCount = ARRAY_SIZE (Map);
    Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
    printf ("PageTableParse returns with status = %llx\n", Status);
    DumpMap (Map, MapCount);
  }
}

int
main (
  VOID
  )
{
  UINT32  Count;
  UINTN   BufferSize;
  VOID    *Buffer;
  UINTN   PassCount;
  UINT64  CurrentTick;
  UINTN   Index;
  PAGING_MODE Mode[] = {
    // Paging4Level,
    Paging4Level1GB,
    // Paging5Level,
    Paging5Level1GB
  };

  char* PageModeStr[] = {
    // "Paging32bit",
    // "PagingPae",
    // "Paging4Level",
    "Paging4Level1GB",
    // "Paging5Level",
    "Paging5Level1GB"
  };
  UINT64  Tick[ARRAY_SIZE(Mode)];

  srand ((unsigned int)time (NULL));
   
  BufferSize = SIZE_64MB;
  Buffer     = _aligned_malloc ((size_t)BufferSize, SIZE_4KB);
  Count      = 0;
  PassCount  = 0;
  memset(Tick, 0, sizeof(Tick));

 #ifdef FUZZY
  while (Count < 1000) {
    printf("\nFuzzyTest :%d\n", Count);
    for (Index = 0; Index < ARRAY_SIZE(Mode); Index++) {
      CurrentTick = clock();
      if (FuzzyTest(Mode[Index], Buffer, BufferSize)) {
        PassCount++;
      }
      Tick[Index] += clock() - CurrentTick;
      printf(
        "============ %s: %.2f ==================\n",
        PageModeStr[Index],
        (double)Tick[Index] * ARRAY_SIZE(Mode) / Count
        );
    }

    Count += ARRAY_SIZE(Mode);
    printf(
      "=========== Pass Rate = %.2f%% (%d / %d) 4L/5L ============================\n",
      (double)PassCount * 100 / Count, (UINT32)PassCount, (UINT32)Count
    );
  }

 #else
  StaticTest (Buffer, BufferSize);
 #endif

  return 0;
}
