/** @file
  This library defines some routines that are generic for IA32 family CPU.

  The library routines are UEFI specification compliant.

  Copyright (c) 2020, AMD Inc. All rights reserved.<BR>
  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuPageTable.h"

/**
  Create initial page table with 0 mapping.
  TODO: Another way is to remove this API. NULL/0 means 0-mapping page table. But 0 cannot be set to CR3.
        Question is: do we need to create a 0-mapping page table and set it to CR3?

  @param PageTable            Return the zero mapping page table.
  @param Buffer               Point to a free buffer for building the page table.
  @param BufferSize           The size of the free buffer.
                              On return, the size of the remaining free buffer.
                              Return the required size if the input BufferSize is too small.

  @retval RETURN_INVALID_PARAMETER
  @retval RETURN_BUFFER_TOO_SMALL
  @retval RETURN_SUCCESS
**/
RETURN_STATUS
PageTableLibCreateZeroMapping (
  OUT    UINTN  *PageTable,
  IN     VOID   *Buffer,
  IN OUT UINTN  *BufferSize
  )
{
  if (BufferSize == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (*BufferSize % SIZE_4KB != 0) {
    //
    // BufferSize should be multiple of 4K.
    //
    return RETURN_INVALID_PARAMETER;
  }

  if (*BufferSize < SIZE_4KB) {
    *BufferSize = SIZE_4KB;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if ((Buffer == NULL) || (((UINTN)Buffer & 0xFFF) != 0)) {
    return RETURN_INVALID_PARAMETER;
  }

  *PageTable = (UINTN)Buffer + *BufferSize - SIZE_4KB;
  ZeroMem ((VOID *)*PageTable, SIZE_4KB);
  *BufferSize -= SIZE_4KB;

  return RETURN_SUCCESS;
}

VOID
PageTableLibSetPte4K (
  IN IA32_PTE_4K         *Pte4K,
  IN UINT64              Offset,
  IN IA32_MAP_ATTRIBUTE  *Setting,
  IN IA32_MAP_ATTRIBUTE  *Mask
  )
{
  if (Mask->Bits.PageTableBaseAddress) {
    //
    // Reset all attributes when the physical address is changed.
    //
    Pte4K->Uint64 = IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (Setting) + Offset;
  }

  if (Mask->Bits.Present) {
    Pte4K->Bits.Present = Setting->Bits.Present;
  }

  if (Mask->Bits.ReadWrite) {
    Pte4K->Bits.ReadWrite = Setting->Bits.ReadWrite;
  }

  if (Mask->Bits.UserSupervisor) {
    Pte4K->Bits.UserSupervisor = Setting->Bits.UserSupervisor;
  }

  if (Mask->Bits.WriteThrough) {
    Pte4K->Bits.WriteThrough = Setting->Bits.WriteThrough;
  }

  if (Mask->Bits.CacheDisabled) {
    Pte4K->Bits.CacheDisabled = Setting->Bits.CacheDisabled;
  }

  if (Mask->Bits.Accessed) {
    Pte4K->Bits.Accessed = Setting->Bits.Accessed;
  }

  if (Mask->Bits.Dirty) {
    Pte4K->Bits.Dirty = Setting->Bits.Dirty;
  }

  if (Mask->Bits.Pat) {
    Pte4K->Bits.Pat = Setting->Bits.Pat;
  }

  if (Mask->Bits.Global) {
    Pte4K->Bits.Global = Setting->Bits.Global;
  }

  if (Mask->Bits.ProtectionKey) {
    Pte4K->Bits.ProtectionKey = Setting->Bits.ProtectionKey;
  }

  if (Mask->Bits.Nx) {
    Pte4K->Bits.Nx = Setting->Bits.Nx;
  }
}

VOID
PageTableLibSetPleB (
  IN IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE  *PleB,
  IN UINT64                             Offset,
  IN IA32_MAP_ATTRIBUTE                 *Setting,
  IN IA32_MAP_ATTRIBUTE                 *Mask
  )
{
  if (Mask->Bits.PageTableBaseAddress) {
    //
    // Reset all attributes when the physical address is changed.
    //
    PleB->Uint64 = IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (Setting) + Offset;
  }

  if (Mask->Bits.Present) {
    PleB->Bits.Present = Setting->Bits.Present;
  }

  if (Mask->Bits.ReadWrite) {
    PleB->Bits.ReadWrite = Setting->Bits.ReadWrite;
  }

  if (Mask->Bits.UserSupervisor) {
    PleB->Bits.UserSupervisor = Setting->Bits.UserSupervisor;
  }

  if (Mask->Bits.WriteThrough) {
    PleB->Bits.WriteThrough = Setting->Bits.WriteThrough;
  }

  if (Mask->Bits.CacheDisabled) {
    PleB->Bits.CacheDisabled = Setting->Bits.CacheDisabled;
  }

  if (Mask->Bits.Accessed) {
    PleB->Bits.Accessed = Setting->Bits.Accessed;
  }

  if (Mask->Bits.Dirty) {
    PleB->Bits.Dirty = Setting->Bits.Dirty;
  }

  if (Mask->Bits.Pat) {
    PleB->Bits.Pat = Setting->Bits.Pat;
  }

  if (Mask->Bits.Global) {
    PleB->Bits.Global = Setting->Bits.Global;
  }

  if (Mask->Bits.ProtectionKey) {
    PleB->Bits.ProtectionKey = Setting->Bits.ProtectionKey;
  }

  if (Mask->Bits.Nx) {
    PleB->Bits.Nx = Setting->Bits.Nx;
  }
}

RETURN_STATUS
PageTableLibSetMapInLevel (
  IN     IA32_PAGING_ENTRY   *ParentPagingEntry,
  IN     VOID                *Buffer,
  IN OUT UINTN               *BufferSize,
  IN     UINTN               Level,
  IN     UINT64              LinearAddress,
  IN     UINT64              Size,
  IN     UINT64              Offset,
  IN     IA32_MAP_ATTRIBUTE  *Setting,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  )
{
  RETURN_STATUS                      Status;
  UINTN                              BitStart;
  UINTN                              Index;
  IA32_PAGING_ENTRY                  *PagingEntry;
  UINT64                             RegionSize;
  UINTN                              ParentLevel;
  UINT64                             SubSize;
  UINT64                             SubOffset;
  UINT64                             RegionMask;
  UINT64                             RegionStart;
  IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE  PleB;

  ASSERT (Level != 0);

  //
  // Split the page.
  // Either create zero mapping if the parent is a zero mapping.
  // Or create mapping that inherits the parent attributes.
  //
  ParentLevel = Level + 1;
  if (
      //
      // CR3 is 0. Will create 512 PML5/PML4 entries (4KB).
      //
      ((ParentLevel == 6) && (ParentPagingEntry->Uintn == 0)) ||
      //
      // PML5E/PML4E/PDPTE/PDE does NOT point to an existing page table entry.
      // Will create 512 child-level entries (PML4E, PDPTE, PDE, PTE_4K).
      //
      ((ParentLevel <= 5) && (ParentPagingEntry->Pce.Present == 0))
      )
  {
    Status = PageTableLibCreateZeroMapping (&ParentPagingEntry->Uintn, Buffer, BufferSize);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    if (ParentLevel != 6) {
      //
      // CR3 doesn't have Present or ReadWrite bit.
      //
      ParentPagingEntry->Pnle.Bits.Present   = 1;
      ParentPagingEntry->Pnle.Bits.ReadWrite = 1;
    }
  } else if (IsPle (ParentPagingEntry, ParentLevel)) {
    //
    // The parent entry is a PDPTE_1G or PDE_2M. Split to 2M or 4K pages.
    // Note: it's impossible the parent entry is a PTE_4K.
    //
    PleB.Uint64 = ParentPagingEntry->PleB.Uint64;
    Status      = PageTableLibCreateZeroMapping (&ParentPagingEntry->Uintn, Buffer, BufferSize);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    ParentPagingEntry->Pnle.Bits.Present        = PleB.Bits.Present;
    ParentPagingEntry->Pnle.Bits.ReadWrite      = PleB.Bits.ReadWrite;
    ParentPagingEntry->Pnle.Bits.UserSupervisor = PleB.Bits.UserSupervisor;
    ParentPagingEntry->Pnle.Bits.Nx             = PleB.Bits.Nx;

    ParentPagingEntry->Pnle.Bits.WriteThrough  = PleB.Bits.WriteThrough;
    ParentPagingEntry->Pnle.Bits.CacheDisabled = PleB.Bits.CacheDisabled;
    ParentPagingEntry->Pnle.Bits.Accessed      = PleB.Bits.Accessed;
    RegionStart                                = IA32_PLEB_PAGE_TABLE_BASE_ADDRESS (&PleB);
    PagingEntry                                = (IA32_PAGING_ENTRY *)(UINTN)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (&ParentPagingEntry->Pnle);
    for (SubOffset = 0, Index = 0; Index < 512; Index++) {
      if (Level == 2) {
        PagingEntry[Index].Pde2M.Uint64 = RegionStart + SubOffset;
        SubOffset                      += SIZE_2MB;

        PagingEntry[Index].Pde2M.Bits.Present        = PleB.Bits.Present;
        PagingEntry[Index].Pde2M.Bits.ReadWrite      = PleB.Bits.ReadWrite;
        PagingEntry[Index].Pde2M.Bits.UserSupervisor = PleB.Bits.UserSupervisor;
        PagingEntry[Index].Pde2M.Bits.WriteThrough   = PleB.Bits.WriteThrough;
        PagingEntry[Index].Pde2M.Bits.CacheDisabled  = PleB.Bits.CacheDisabled;
        PagingEntry[Index].Pde2M.Bits.Accessed       = PleB.Bits.Accessed;
        PagingEntry[Index].Pde2M.Bits.Dirty          = PleB.Bits.Dirty;
        PagingEntry[Index].Pde2M.Bits.MustBeOne      = 1;

        PagingEntry[Index].Pde2M.Bits.Global        = PleB.Bits.Global;
        PagingEntry[Index].Pde2M.Bits.Pat           = PleB.Bits.Pat;
        PagingEntry[Index].Pde2M.Bits.ProtectionKey = PleB.Bits.ProtectionKey;
        PagingEntry[Index].Pde2M.Bits.Nx            = PleB.Bits.Nx;
      } else {
        ASSERT (Level == 1);
        PagingEntry[Index].Pte4K.Uint64 = RegionStart + SubOffset;
        SubOffset                      += SIZE_4KB;

        PagingEntry[Index].Pte4K.Bits.Present        = PleB.Bits.Present;
        PagingEntry[Index].Pte4K.Bits.ReadWrite      = PleB.Bits.ReadWrite;
        PagingEntry[Index].Pte4K.Bits.UserSupervisor = PleB.Bits.UserSupervisor;
        PagingEntry[Index].Pte4K.Bits.WriteThrough   = PleB.Bits.WriteThrough;
        PagingEntry[Index].Pte4K.Bits.CacheDisabled  = PleB.Bits.CacheDisabled;
        PagingEntry[Index].Pte4K.Bits.Accessed       = PleB.Bits.Accessed;
        PagingEntry[Index].Pte4K.Bits.Dirty          = PleB.Bits.Dirty;
        PagingEntry[Index].Pte4K.Bits.Pat            = PleB.Bits.Pat;

        PagingEntry[Index].Pte4K.Bits.Global        = PleB.Bits.Global;
        PagingEntry[Index].Pte4K.Bits.ProtectionKey = PleB.Bits.ProtectionKey;
        PagingEntry[Index].Pte4K.Bits.Nx            = PleB.Bits.Nx;
      }
    }
  }

  //
  // Apply the setting.
  //
  PagingEntry = (IA32_PAGING_ENTRY *)(UINTN)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (&ParentPagingEntry->Pnle);
  BitStart    = 12 + (Level - 1) * 9;
  Index       = BitFieldRead64 (LinearAddress + Offset, BitStart, BitStart + 9 - 1);
  RegionSize  = LShiftU64 (1, BitStart);
  RegionMask  = RegionSize - 1;
  RegionStart = (LinearAddress + Offset) & ~RegionMask;
  while (Offset < Size && Index < 512) {
    SubSize = MIN (Size - Offset, RegionStart + RegionSize - (LinearAddress + Offset));
    if ((Level <= 3) && (LinearAddress + Offset == RegionStart) && (SubSize == RegionSize)) {
      //
      // Do not split the page table.
      // Set one entry to map to entire (1 << BitStart) (1G, 2M, 4K) region of linear address space.
      //
      if (Level == 1) {
        PageTableLibSetPte4K (&PagingEntry[Index].Pte4K, Offset, Setting, Mask);
      } else {
        PageTableLibSetPleB (&PagingEntry[Index].PleB, Offset, Setting, Mask);
        PagingEntry[Index].PleB.Bits.MustBeOne = 1;
      }
    } else {
      Status = PageTableLibSetMapInLevel (
                 &PagingEntry[Index],
                 Buffer,
                 BufferSize,
                 Level - 1,
                 LinearAddress,
                 Size,
                 Offset,
                 Setting,
                 Mask
                 );
      if (RETURN_ERROR (Status)) {
        return Status;
      }
    }

    Offset      += SubSize;
    RegionStart += RegionSize;
    Index++;
  }

  return RETURN_SUCCESS;
}

/**
  It might create new page table entries that map LinearAddress to PhysicalAddress with specified MapAttribute.
  It might change existing page table entries to map LinearAddress to PhysicalAddress with specified MapAttribute.

  @param PageTable
  @param Buffer
  @param BufferSize
  @param Paging5L
  @param LinearAddress
  @param PhysicalAddress
  @param Size
  @param Setting
  @param Mask
  @return RETURN_STATUS
**/
RETURN_STATUS
EFIAPI
PageTableSetMap (
  OUT    UINTN               *PageTable  OPTIONAL,
  IN     VOID                *Buffer,
  IN OUT UINTN               *BufferSize,
  IN     BOOLEAN             Paging5L,
  IN     UINT64              LinearAddress,
  IN     UINT64              Size,
  IN     IA32_MAP_ATTRIBUTE  *Setting,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  )
{
  if (PageTable == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  return PageTableLibSetMapInLevel (
           (IA32_PAGING_ENTRY *)PageTable,
           Buffer,
           BufferSize,
           Paging5L ? 5 : 4,
           LinearAddress,
           Size,
           0,
           Setting,
           Mask
           );
}
