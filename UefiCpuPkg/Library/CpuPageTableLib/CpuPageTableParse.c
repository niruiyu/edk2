/** @file
  This library defines some routines that are generic for IA32 family CPU.

  The library routines are UEFI specification compliant.

  Copyright (c) 2020, AMD Inc. All rights reserved.<BR>
  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuPageTable.h"

UINT64
PageTableLibGetPleBMapAttribute (
  IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE  *PleB,
  IA32_MAP_ATTRIBUTE                 *ParentMapAttribute
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

  MapAttribute.Bits.Pat                  = PleB->Bits.Pat;
  MapAttribute.Bits.Dirty                = PleB->Bits.Dirty;
  MapAttribute.Bits.Global               = PleB->Bits.Global;
  MapAttribute.Bits.ProtectionKey        = PleB->Bits.ProtectionKey;

  return MapAttribute.Uint64;
}

UINT64
PageTableLibGetPte4KMapAttribute (
  IA32_PTE_4K         *Pte4K,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute
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

  MapAttribute.Bits.Pat                  = Pte4K->Bits.Pat;
  MapAttribute.Bits.Dirty                = Pte4K->Bits.Dirty;
  MapAttribute.Bits.Global               = Pte4K->Bits.Global;
  MapAttribute.Bits.ProtectionKey        = Pte4K->Bits.ProtectionKey;

  return MapAttribute.Uint64;
}

UINT64
PageTableLibGetPnleMapAttribute (
  IA32_PAGE_NON_LEAF_ENTRY  *Pnle,
  IA32_MAP_ATTRIBUTE        *ParentMapAttribute
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

RETURN_STATUS
PageTableLibAddMap (
  IA32_PAGING_ENTRY   *PagingEntry,
  UINTN               Level,
  UINT64              LinearAddress,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute,
  IA32_MAP_ENTRY      *Map,
  UINTN               *Count,
  UINTN               Capacity
  )
{
  UINTN               Index;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  UINT64              Size;

  Size            = 0;

  switch (Level) {
    case 3:
      Size                = SIZE_1GB;
      MapAttribute.Uint64 = PageTableLibGetPleBMapAttribute (&PagingEntry->PleB, ParentMapAttribute);
      break;

    case 2:
      Size                = SIZE_2MB;
      MapAttribute.Uint64 = PageTableLibGetPleBMapAttribute (&PagingEntry->PleB, ParentMapAttribute);
      break;

    case 1:
      Size                = SIZE_4KB;
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
    if ((Map[Index].LinearAddress + Map[Index].Size == LinearAddress) &&
        (IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[Index].Attribute) + Map[Index].Size == IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&MapAttribute)) &&
        (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute))
        )
    {
      //
      // Enlarge to right side. (Right side == Bigger address)
      //
      Map[Index].Size += Size;
      return RETURN_SUCCESS;
    }

    if ((Map[Index].LinearAddress == LinearAddress + Size) &&
        (IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&MapAttribute) + Size) &&
        (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[Index].Attribute) == IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute))
        )
    {
      //
      // Enlarge to left side. (Right side == Bigger address)
      //
      Map[Index].LinearAddress = LinearAddress;
      Map[Index].Attribute.Bits.PageTableBaseAddress = MapAttribute.Bits.PageTableBaseAddress;
      return RETURN_SUCCESS;
    }
  }

  ASSERT (Index == *Count);
  if (*Count < Capacity) {
    Map[*Count].LinearAddress    = LinearAddress;
    Map[*Count].Size             = Size;
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

BOOLEAN
IsPle (
  IN     IA32_PAGING_ENTRY  *PagingEntry,
  IN     UINTN              Level
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

RETURN_STATUS
PageTableLibParsePnle (
  UINT64              PageTableBaseAddress,
  UINTN               Level,
  UINT64              LinearAddress,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute,
  IA32_MAP_ENTRY      *Map,
  UINTN               *MapCount,
  UINTN               MapCapacity
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
                 LinearAddress + LShiftU64 (Index, LeftShift),
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
                              LinearAddress + LShiftU64 (Index, LeftShift),
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

  @param PageTable
  @param Paging5L
  @param Map
  @param MapCount
  @return RETURN_STATUS
**/
RETURN_STATUS
EFIAPI
PageTableParse (
  UINTN           PageTable,
  BOOLEAN         Paging5L,
  IA32_MAP_ENTRY  *Map,
  UINTN           *MapCount
  )
{
  UINTN               MapCapacity;
  IA32_MAP_ATTRIBUTE  MapAttribute;

  /*
  [IA32_CR3]
      |
      |
      V
  [IA32_PML5E]
  ...
  [IA32_PML5E] --> [IA32_PML4E]
                   ...
                   [IA32_PML4E] --> [IA32_PDPTE_1G] --> 1G aligned physical address
                                    ...
                                    [IA32_PDPTE] --> [IA32_PDE_2M] --> 2M aligned physical address
                                                     ...
                                                     [IA32_PDE] --> [IA32_PTE_4K]  --> 4K aligned physical address
                                                                    ...
                                                                    [IA32_PTE_4K]

  */
  ZeroMem (&MapAttribute, sizeof (MapAttribute));

  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 1;

  MapCapacity = *MapCount;
  *MapCount   = 0;
  return PageTableLibParsePnle ((UINT64)PageTable, Paging5L ? 5 : 4, 0, &MapAttribute, Map, MapCount, MapCapacity);
}
