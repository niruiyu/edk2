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
  OUT IA32_MAP_ENTRY      *Map,
  OUT IA32_MAP_ATTRIBUTE  *MapMasks,
  IN  UINTN               *MapCount
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
    Map[Index].Size          = Random64 (SIZE_4KB, AvgRangeSize) & ~0xFFFull;
    if (Map[Index].Size > MaxAddress - Map[Index].LinearAddress) {
      Map[Index].Size = MaxAddress - Map[Index].LinearAddress;
    }

    LinearAddress = Map[Index].LinearAddress + Map[Index].Size;

    if (RandomBoolean ()) {
      //
      // Randomly skip mapping certain range.
      //
      continue;
    }

    Map[Index].Attribute.Uint64               = Map[Index].LinearAddress;
    MapMasks[Index].Bits.PageTableBaseAddress = 1;

    Map[Index].Attribute.Bits.Present = 1;
    MapMasks[Index].Bits.Present      = 1;

    Map[Index].Attribute.Bits.ReadWrite = RandomBoolean ();
    MapMasks[Index].Bits.ReadWrite      = RandomBoolean ();
    if (MapMasks[Index].Bits.ReadWrite == 0) {
      Map[Index].Attribute.Bits.ReadWrite = 0;
    }

    Map[Index].Attribute.Bits.WriteThrough = RandomBoolean ();
    MapMasks[Index].Bits.WriteThrough      = RandomBoolean ();
    if (MapMasks[Index].Bits.WriteThrough == 0) {
      Map[Index].Attribute.Bits.WriteThrough = 0;
    }

    Map[Index].Attribute.Bits.CacheDisabled = RandomBoolean ();
    MapMasks[Index].Bits.CacheDisabled      = RandomBoolean ();
    if (MapMasks[Index].Bits.CacheDisabled == 0) {
      Map[Index].Attribute.Bits.CacheDisabled = 0;
    }

    Map[Index].Attribute.Bits.Pat = RandomBoolean ();
    MapMasks[Index].Bits.Pat      = RandomBoolean ();
    if (MapMasks[Index].Bits.Pat == 0) {
      Map[Index].Attribute.Bits.Pat = 0;
    }

    Map[Index].Attribute.Bits.Nx = RandomBoolean ();
    MapMasks[Index].Bits.Nx      = RandomBoolean ();
    if (MapMasks[Index].Bits.Nx == 0) {
      Map[Index].Attribute.Bits.Nx = 0;
    }

    Index++;
  }

  //
  // Combine adjacent ranges
  //
  for (NewIndex = 0, Index = 1; Index < *MapCount; Index++) {
    if ((Map[Index].LinearAddress == Map[NewIndex].LinearAddress + Map[NewIndex].Size) &&
        (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[NewIndex].Attribute)) &&
        (IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[NewIndex].Attribute) + Map[NewIndex].Size)
        )
    {
      Map[NewIndex].Size += Map[Index].Size;
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
DumpMap (
  IA32_MAP_ENTRY  *Map,
  UINTN           MapCount
  )
{
  for (int i = 0; i < MapCount; i++) {
    printf (
      "[%016llx - %016llx]: %016llx\n",
      Map[i].LinearAddress,
      Map[i].LinearAddress + Map[i].Size,
      Map[i].Attribute.Uint64
      );
  }
}

VOID
FuzzyTest (
  VOID   *Buffer,
  UINTN  BufferSize
  )
{
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      SrcMap[50];
  IA32_MAP_ATTRIBUTE  SrcMapMasks[ARRAY_SIZE (SrcMap)];
  UINTN               SrcMapCount;
  IA32_MAP_ENTRY      Map[100];
  UINTN               MapCount;
  UINTN               Index;
  UINTN               PageTable;

  SrcMapCount = Random32 (1, ARRAY_SIZE (SrcMap));
  GenerateRandomMapEntry (SrcMap, SrcMapMasks, &SrcMapCount);

  PageTable = 0;
  for (Index = 0; Index < SrcMapCount; Index++) {
    Status = PageTableSetMap (
               &PageTable,
               Buffer,
               &BufferSize,
               TRUE,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Size,
               &SrcMap[Index].Attribute,
               &SrcMapMasks[Index]
               );
    assert (Status == RETURN_SUCCESS);
  }

  MapCount = ARRAY_SIZE (Map);
  Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
  assert (Status == RETURN_SUCCESS);
  if ((SrcMapCount != MapCount) || (memcmp (SrcMap, Map, MapCount * sizeof (IA32_MAP_ENTRY)) != 0)) {
    printf ("FAIL!!!!!!\n");
    DumpMap (SrcMap, SrcMapCount);
    printf ("--------------------------\n");
    DumpMap (Map, MapCount);
  } else {
    printf ("PASS!!!\n");
  }
}

VOID
StaticTest (
  VOID
  )
{
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      SrcMap[20];
  IA32_MAP_ENTRY      Map[100];
  IA32_MAP_ATTRIBUTE  MapAttribute, MapMask;
  UINTN               MapCount;
  UINTN               Index;

  UINTN  BufferSize = SIZE_32MB;
  void   *Buffer    = _aligned_malloc ((size_t)BufferSize, SIZE_4KB);

  printf ("Buffer = %p\n", Buffer);

  UINTN  PageTable = 0;

  SrcMap[0].LinearAddress    = 0;
  SrcMap[0].Size             = 0x00005e9194b76000;
  SrcMap[0].Attribute.Uint64 = 0x8000000000000001;
  SrcMap[1].LinearAddress    = 0x00005e9194b76000;
  SrcMap[1].Size             = 0x0000cabc87ffc000;
  SrcMap[1].Attribute.Uint64 = 0x80005e9194b76011;
  MapMask.Uint64             = MAX_UINT64;

  for (Index = 0; Index < 2; Index++) {
    Status = PageTableSetMap (
               &PageTable,
               Buffer,
               &BufferSize,
               TRUE,
               SrcMap[Index].LinearAddress,
               SrcMap[Index].Size,
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

  Status = PageTableSetMap (&PageTable, Buffer, &BufferSize, TRUE, 0, SIZE_4GB, &MapAttribute, &MapMask);
  assert (Status == 0);

  MapCount = ARRAY_SIZE (Map);
  Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
  printf ("PageTableParse returns with status = %llx\n", Status);
  DumpMap (Map, MapCount);
  assert (MapCount == 1);
  assert (Map[0].LinearAddress == 0);
  assert (Map[0].Size == SIZE_4GB);
  IA32_MAP_ATTRIBUTE  MapAttribute4G;

  MapAttribute4G.Uint64         = 0;
  MapAttribute4G.Bits.Present   = 1;
  MapAttribute4G.Bits.ReadWrite = 1;
  assert (MapAttribute.Uint64 == Map[0].Attribute.Uint64);

  MapAttribute.Bits.Present = 0;
  Status                    = PageTableSetMap (&PageTable, Buffer, &BufferSize, TRUE, 0x60000, 0xA0000 - 0x60000, &MapAttribute, &MapMask);
  assert (Status == 0);

  MapAttribute.Bits.ReadWrite = 0;
  MapAttribute.Bits.Nx        = 1;
  Status                      = PageTableSetMap (&PageTable, Buffer, &BufferSize, TRUE, 0x17ca00000, 0x7ca0000000 - 0x17ca00000, &MapAttribute, &MapMask);
  assert (Status == 0);
  MapCount = ARRAY_SIZE (Map);
  Status   = PageTableParse (PageTable, TRUE, Map, &MapCount);
  printf ("PageTableParse returns with status = %llx\n", Status);
  DumpMap (Map, MapCount);
}

int
main (
  VOID
  )
{
  UINT32  Index;
  UINTN   BufferSize;
  void    *Buffer;

  srand ((unsigned int)time (NULL));

  BufferSize = SIZE_32MB;
  Buffer     = _aligned_malloc ((size_t)BufferSize, SIZE_4KB);
  Index      = 0;

  while (Index++ < 1000) {
    printf ("FuzzyTest:%d\n", Index);
    FuzzyTest (Buffer, BufferSize);
  }

  return 0;
}
