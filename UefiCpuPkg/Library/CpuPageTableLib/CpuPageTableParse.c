/** @file
  This library implements CpuPageTableLib that are generic for IA32 family CPU.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuPageTable.h"

/**
  Return the attribute of a 2M/1G page table entry.

  @param[in] PleB               Pointer to a 2M/1G page table entry.
  @param[in] ParentMapAttribute Pointer to the parent attribute.

  @return Attribute of the 2M/1G page table entry.
**/
UINT64
PageTableLibGetPleBMapAttribute (
  IN IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE  *PleB,
  IN IA32_MAP_ATTRIBUTE                 *ParentMapAttribute
  )
{
  IA32_MAP_ATTRIBUTE  MapAttribute;

  //
  // PageTableBaseAddress cannot be assigned field to field
  // because their bit positions are different in IA32_MAP_ATTRIBUTE and IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE.
  //
  MapAttribute.Uint64 = IA32_PLEB_PAGE_TABLE_BASE_ADDRESS (PleB);

  MapAttribute.Bits.Present        = ParentMapAttribute->Bits.Present & PleB->Bits.Present;
  MapAttribute.Bits.ReadWrite      = ParentMapAttribute->Bits.ReadWrite & PleB->Bits.ReadWrite;
  MapAttribute.Bits.UserSupervisor = ParentMapAttribute->Bits.UserSupervisor & PleB->Bits.UserSupervisor;
  MapAttribute.Bits.Nx             = ParentMapAttribute->Bits.Nx | PleB->Bits.Nx;
  MapAttribute.Bits.WriteThrough   = PleB->Bits.WriteThrough;
  MapAttribute.Bits.CacheDisabled  = PleB->Bits.CacheDisabled;
  MapAttribute.Bits.Accessed       = PleB->Bits.Accessed;

  MapAttribute.Bits.Pat           = PleB->Bits.Pat;
  MapAttribute.Bits.Dirty         = PleB->Bits.Dirty;
  MapAttribute.Bits.Global        = PleB->Bits.Global;
  MapAttribute.Bits.ProtectionKey = PleB->Bits.ProtectionKey;

  return MapAttribute.Uint64;
}

/**
  Return the attribute of a 4K page table entry.

  @param[in] Pte4K              Pointer to a 4K page table entry.
  @param[in] ParentMapAttribute Pointer to the parent attribute.

  @return Attribute of the 4K page table entry.
**/
UINT64
PageTableLibGetPte4KMapAttribute (
  IN IA32_PTE_4K         *Pte4K,
  IN IA32_MAP_ATTRIBUTE  *ParentMapAttribute
  )
{
  IA32_MAP_ATTRIBUTE  MapAttribute;

  MapAttribute.Uint64 = IA32_PTE4K_PAGE_TABLE_BASE_ADDRESS (Pte4K);

  MapAttribute.Bits.Present        = ParentMapAttribute->Bits.Present & Pte4K->Bits.Present;
  MapAttribute.Bits.ReadWrite      = ParentMapAttribute->Bits.ReadWrite & Pte4K->Bits.ReadWrite;
  MapAttribute.Bits.UserSupervisor = ParentMapAttribute->Bits.UserSupervisor & Pte4K->Bits.UserSupervisor;
  MapAttribute.Bits.Nx             = ParentMapAttribute->Bits.Nx | Pte4K->Bits.Nx;
  MapAttribute.Bits.WriteThrough   = Pte4K->Bits.WriteThrough;
  MapAttribute.Bits.CacheDisabled  = Pte4K->Bits.CacheDisabled;
  MapAttribute.Bits.Accessed       = Pte4K->Bits.Accessed;

  MapAttribute.Bits.Pat           = Pte4K->Bits.Pat;
  MapAttribute.Bits.Dirty         = Pte4K->Bits.Dirty;
  MapAttribute.Bits.Global        = Pte4K->Bits.Global;
  MapAttribute.Bits.ProtectionKey = Pte4K->Bits.ProtectionKey;

  return MapAttribute.Uint64;
}

/**
  Return the attribute of a non-leaf page table entry.

  @param[in] Pnle               Pointer to a non-leaf page table entry.
  @param[in] ParentMapAttribute Pointer to the parent attribute.

  @return Attribute of the non-leaf page table entry.
**/
UINT64
PageTableLibGetPnleMapAttribute (
  IN IA32_PAGE_NON_LEAF_ENTRY  *Pnle,
  IN IA32_MAP_ATTRIBUTE        *ParentMapAttribute
  )
{
  IA32_MAP_ATTRIBUTE  MapAttribute;

  MapAttribute.Uint64 = Pnle->Uint64;

  MapAttribute.Bits.Present        = ParentMapAttribute->Bits.Present & Pnle->Bits.Present;
  MapAttribute.Bits.ReadWrite      = ParentMapAttribute->Bits.ReadWrite & Pnle->Bits.ReadWrite;
  MapAttribute.Bits.UserSupervisor = ParentMapAttribute->Bits.UserSupervisor & Pnle->Bits.UserSupervisor;
  MapAttribute.Bits.Nx             = ParentMapAttribute->Bits.Nx | Pnle->Bits.Nx;
  MapAttribute.Bits.WriteThrough   = Pnle->Bits.WriteThrough;
  MapAttribute.Bits.CacheDisabled  = Pnle->Bits.CacheDisabled;
  MapAttribute.Bits.Accessed       = Pnle->Bits.Accessed;
  return MapAttribute.Uint64;
}

/**
  Add the linear address mapping to Map.

  @param[in]      PagingEntry        Pointer to a leaf page table entry that maps to physical address memory.
  @param[in]      Level              Page table level.
  @param[in]      RegionStart        The base linear address of the region covered by the leaf page table entry.
  @param[in]      ParentMapAttribute The mapping attribute of the parent entries.
  @param[in, out] Map                Pointer to an array that describes multiple linear address ranges.
  @param[in, out] MapCount           Pointer to a UINTN that hold the actual number of entries in the Map.
  @param[in]      MapCapacity        The maximum number of entries the Map can hold.

  @retval RETURN_BUFFER_TOO_SMALL  Capacity is too small. *Count is updated to indicate the required capacity.
  @retval RETURN_SUCCESS           Page table is parsed successfully.
**/
RETURN_STATUS
PageTableLibAddMap (
  IN     IA32_PAGING_ENTRY   *PagingEntry,
  IN     UINTN               Level,
  IN     UINT64              RegionStart,
  IN     IA32_MAP_ATTRIBUTE  *ParentMapAttribute,
  IN OUT IA32_MAP_ENTRY      *Map,
  IN OUT UINTN               *Count,
  IN     UINTN               Capacity
  )
{
  UINTN               Index;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  UINT64              Length;

  Length = 0;

  switch (Level) {
    case 3:
      Length              = SIZE_1GB;
      MapAttribute.Uint64 = PageTableLibGetPleBMapAttribute (&PagingEntry->PleB, ParentMapAttribute);
      break;

    case 2:
      Length              = SIZE_2MB;
      MapAttribute.Uint64 = PageTableLibGetPleBMapAttribute (&PagingEntry->PleB, ParentMapAttribute);
      break;

    case 1:
      Length              = SIZE_4KB;
      MapAttribute.Uint64 = PageTableLibGetPte4KMapAttribute (&PagingEntry->Pte4K, ParentMapAttribute);
      break;

    default:
      ASSERT (Level == 1 || Level == 2 || Level == 3);
      break;
  }

  //
  // Merge with existing Map entry.
  //
  for (Index = 0; Index < *Count; Index++) {
    if ((Map[Index].LinearAddress + Map[Index].Length == RegionStart) &&
        (IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[Index].Attribute) + Map[Index].Length
         == IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&MapAttribute)) &&
        (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute))
        )
    {
      //
      // Enlarge to right side. (Right side == Bigger address)
      //
      Map[Index].Length += Length;
      return RETURN_SUCCESS;
    }

    if ((Map[Index].LinearAddress == RegionStart + Length) &&
        (IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[Index].Attribute)
         == IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&MapAttribute) + Length) &&
        (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute))
        )
    {
      //
      // Enlarge to left side. (Right side == Bigger address)
      //
      Map[Index].LinearAddress                       = RegionStart;
      Map[Index].Attribute.Bits.PageTableBaseAddress = MapAttribute.Bits.PageTableBaseAddress;
      return RETURN_SUCCESS;
    }
  }

  ASSERT (Index == *Count);
  if (*Count < Capacity) {
    Map[*Count].LinearAddress    = RegionStart;
    Map[*Count].Length           = Length;
    Map[*Count].Attribute.Uint64 = MapAttribute.Uint64;
    *Count                       = *Count + 1;
    return RETURN_SUCCESS;
  } else {
    ASSERT (*Count < Capacity);
    //
    // Report the required space with BufferTooSmall error status.
    // Note: BufferTooSmall might be returned again even with the updated Count.
    //
    *Count = *Count + 1;
    return RETURN_BUFFER_TOO_SMALL;
  }
}

/**
  Return TRUE when the page table entry is a leaf entry that points to the physical address memory.
  Return FALSE when the page table entry is a non-leaf entry that points to the page table entries.

  @param[in] PagingEntry Pointer to the page table entry.
  @param[in] Level       Page level where the page table entry resides in.

  @retval TRUE  It's a leaf entry.
  @retval FALSE It's a non-leaf entry.
**/
BOOLEAN
IsPle (
  IN IA32_PAGING_ENTRY  *PagingEntry,
  IN UINTN              Level
  )
{
  //
  // PML5E and PML4E are always non-leaf entries.
  //
  if (Level == 1) {
    return TRUE;
  }

  if (((Level == 3) || (Level == 2))) {
    if (PagingEntry->PleB.Bits.MustBeOne == 1) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Recursively parse the non-leaf page table entries.

  @param[in]      PageTableBaseAddress The base address of the 512 non-leaf page table entries in the specified level.
  @param[in]      Level                Page level. Could be 5, 4, 3, 2, 1.
  @param[in]      RegionStart          The base linear address of the region covered by the non-leaf page table entries.
  @param[in]      ParentMapAttribute   The mapping attribute of the parent entries.
  @param[in, out] Map                  Pointer to an array that describes multiple linear address ranges.
  @param[in, out] MapCount             Pointer to a UINTN that hold the actual number of entries in the Map.
  @param[in]      MapCapacity          The maximum number of entries the Map can hold.

  @retval RETURN_INVALID_PARAMETER MapCount is NULL.
  @retval RETURN_INVALID_PARAMETER *MapCount is not 0 but Map is NULL.
  @retval RETURN_BUFFER_TOO_SMALL  Capacity is too small. *Count is updated to indicate the required capacity.
  @retval RETURN_SUCCESS           Page table is parsed successfully.
**/
RETURN_STATUS
PageTableLibParsePnle (
  IN     UINT64              PageTableBaseAddress,
  IN     UINTN               Level,
  IN     UINT64              RegionStart,
  IN     IA32_MAP_ATTRIBUTE  *ParentMapAttribute,
  IN OUT IA32_MAP_ENTRY      *Map,
  IN OUT UINTN               *MapCount,
  IN     UINTN               MapCapacity
  )
{
  RETURN_STATUS       Status;
  IA32_PAGING_ENTRY   *PagingEntry;
  UINTN               Index;
  UINTN               LeftShift;
  IA32_MAP_ATTRIBUTE  MapAttribute;

  PagingEntry = (IA32_PAGING_ENTRY *)(UINTN)PageTableBaseAddress;
  LeftShift   = 12 + 9 * (Level - 1);

  for (Index = 0; Index < 512; Index++) {
    if (PagingEntry[Index].Pce.Present == 0) {
      continue;
    }

    if (IsPle (&PagingEntry[Index], Level)) {
      Status = PageTableLibAddMap (
                 &PagingEntry[Index],
                 Level,
                 RegionStart + LShiftU64 (Index, LeftShift),
                 ParentMapAttribute,
                 Map,
                 MapCount,
                 MapCapacity
                 );
    } else {
      MapAttribute.Uint64 = PageTableLibGetPnleMapAttribute (&PagingEntry[Index].Pnle, ParentMapAttribute);
      Status              = PageTableLibParsePnle (
                              IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (&PagingEntry[Index].Pnle),
                              Level - 1,
                              RegionStart + LShiftU64 (Index, LeftShift),
                              &MapAttribute,
                              Map,
                              MapCount,
                              MapCapacity
                              );
    }

    if (RETURN_ERROR (Status)) {
      return Status;
    }
  }

  return RETURN_SUCCESS;
}

/**
  Parse page table.

  @param[in]      PageTable Pointer to the page table.
  @param[in]      Paging5L  TRUE when the PageTable points to 5-level page table.
  @param[out]     Map       Return an array that describes multiple linear address ranges.
  @param[in, out] MapCount  On input, the maximum number of entries that Map can hold.
                            On output, the number of entries in Map.

  @retval RETURN_INVALID_PARAMETER MapCount is NULL.
  @retval RETURN_INVALID_PARAMETER *MapCount is not 0 but Map is NULL.
  @retval RETURN_BUFFER_TOO_SMALL  *MapCount is too small.
  @retval RETURN_SUCCESS           Page table is parsed successfully.
**/
RETURN_STATUS
EFIAPI
PageTableParse (
  IN     UINTN           PageTable,
  IN     BOOLEAN         Paging5L,
  OUT    IA32_MAP_ENTRY  *Map,
  IN OUT UINTN           *MapCount
  )
{
  UINTN               MapCapacity;
  IA32_MAP_ATTRIBUTE  MapAttribute;

  if (MapCount == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((*MapCount != 0) && (Map == NULL)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (PageTable == 0) {
    *MapCount = 0;
    return RETURN_SUCCESS;
  }

  //
  // Page table layout is as below:
  //
  // [IA32_CR3]
  //     |
  //     |
  //     V
  // [IA32_PML5E]
  // ...
  // [IA32_PML5E] --> [IA32_PML4E]
  //                  ...
  //                  [IA32_PML4E] --> [IA32_PDPTE_1G] --> 1G aligned physical address
  //                                   ...
  //                                   [IA32_PDPTE] --> [IA32_PDE_2M] --> 2M aligned physical address
  //                                                    ...
  //                                                    [IA32_PDE] --> [IA32_PTE_4K]  --> 4K aligned physical address
  //                                                                   ...
  //                                                                   [IA32_PTE_4K]  --> 4K aligned physical address
  //
  ZeroMem (&MapAttribute, sizeof (MapAttribute));

  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 1;

  MapCapacity = *MapCount;
  *MapCount   = 0;
  return PageTableLibParsePnle ((UINT64)PageTable, Paging5L ? 5 : 4, 0, &MapAttribute, Map, MapCount, MapCapacity);
}
