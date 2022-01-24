/** @file
  This library implements CpuPageTableLib that are generic for IA32 family CPU.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuPageTable.h"

/**
  Create zero mapping 512 entries by using the last 4KB memory from the free buffer.

  @param[out]     PageTable            Return the zero mapping page table.
  @param[in]      Buffer               Point to a free buffer for building the page table.
  @param[in, out] BufferSize           The size of the free buffer.
                                       On return, the size of the remaining free buffer.
                                       Return the required size if the input BufferSize is too small.

  @retval RETURN_INVALID_PARAMETER BufferSize is NULL, or *BufferSize is not multiple of 4KB.
  @retval RETURN_BUFFER_TOO_SMALL  *BufferSize < 4KB.
  @retval RETURN_INVALID_PARAMETER Buffer is NULL or is NOT 4KB aligned when *BufferSize is sufficient.
  @retval RETURN_SUCCESS           Zero mapping 512 entries are successfully created.
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

/**
  Set the IA32_PTE_4K.

  @param[in] Pte4K   Pointer to IA32_PTE_4K.
  @param[in] Offset  The offset within the linear address range.
  @param[in] Setting The setting of the linear address range.
                     All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                     Page table entry is reset to 0 before set to the new setting when a new physical base address is set.
  @param[in] Mask    The mask used for setting. The corresponding field in Setting is ignored if that in Mask is 0.
**/
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

/**
  Set the IA32_PDPTE_1G or IA32_PDE_2M.

  @param[in] PleB    Pointer to PDPTE_1G or PDE_2M. Both share the same structure definition.
  @param[in] Offset  The offset within the linear address range.
  @param[in] Setting The setting of the linear address range.
                     All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                     Page table entry is reset to 0 before set to the new setting when a new physical base address is set.
  @param[in] Mask    The mask used for setting. The corresponding field in Setting is ignored if that in Mask is 0.
**/
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

/**
  Update page table to map [LinearAddress, LinearAddress + Length) with specified setting in the specified level.

  @param[in]      ParentPagingEntry The pointer to the page table entry to update.
  @param[in]      Buffer            The free buffer to be used for page table creation/updating.
  @param[in, out] BufferSize        The buffer size.
                                    On return, the remaining buffer size.
                                    The free buffer is used from the end so caller can supply the same Buffer pointer with an updated
                                    BufferSize in the second call to this API.
  @param[in]      Level             Page table level. Could be 5, 4, 3, 2, or 1.
  @param[in]      LinearAddress     The start of the linear address range.
  @param[in]      Length            The length of the linear address range.
  @param[in]      Offset            The offset within the linear address range.
  @param[in]      Setting           The setting of the linear address range.
                                    All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                                    Page table entries that map the linear address range are reset to 0 before set to the new setting
                                    when a new physical base address is set.
  @param[in]      Mask              The mask used for setting. The corresponding field in Setting is ignored if that in Mask is 0.

  @retval RETURN_BUFFER_TOO_SMALL   The buffer is too small for page table creation/updating.
                                    BufferSize is updated to indicate the expected buffer size.
                                    Caller may still get RETURN_BUFFER_TOO_SMALL with the new BufferSize.
  @retval RETURN_SUCCESS            PageTable is created/updated successfully.
**/
RETURN_STATUS
PageTableLibMapInLevel (
  IN     IA32_PAGING_ENTRY   *ParentPagingEntry,
  IN     VOID                *Buffer,
  IN OUT UINTN               *BufferSize,
  IN     UINTN               Level,
  IN     UINT64              LinearAddress,
  IN     UINT64              Length,
  IN     UINT64              Offset,
  IN     IA32_MAP_ATTRIBUTE  *Setting,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  )
{
  RETURN_STATUS                      Status;
  UINTN                              BitStart;
  UINTN                              Index;
  IA32_PAGING_ENTRY                  *PagingEntry;
  UINT64                             RegionLength;
  UINTN                              ParentLevel;
  UINT64                             SubLength;
  UINT64                             SubOffset;
  UINT64                             RegionMask;
  UINT64                             RegionStart;
  IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE  PleB;

  ASSERT (Level != 0);
  ASSERT (ParentPagingEntry != NULL);
  ASSERT ((Setting != NULL) && (Mask != NULL));

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
  //
  // RegionStart: points to the linear address that's aligned on 1G, 2M or 4K and lower than LinearAddress + Offset
  // RegionLength:  1G, 2M or 4K.
  //
  RegionLength = LShiftU64 (1, BitStart);
  RegionMask   = RegionLength - 1;
  RegionStart  = (LinearAddress + Offset) & ~RegionMask;
  while (Offset < Length && Index < 512) {
    SubLength = MIN (Length - Offset, RegionStart + RegionLength - (LinearAddress + Offset));
    if ((Level <= 3) && (LinearAddress + Offset == RegionStart) && (SubLength == RegionLength)) {
      //
      // Create one entry mapping the entire region (1G, 2M or 4K).
      //
      if (Level == 1) {
        PageTableLibSetPte4K (&PagingEntry[Index].Pte4K, Offset, Setting, Mask);
      } else {
        PageTableLibSetPleB (&PagingEntry[Index].PleB, Offset, Setting, Mask);
        PagingEntry[Index].PleB.Bits.MustBeOne = 1;
      }
    } else {
      //
      // Recursively call to create page table mapping the remaining region.
      // There are 3 cases:
      //   a. Level is 5 or 4
      //   a. #a is TRUE but (LinearAddress + Offset) is NOT aligned on the RegionStart
      //   b. #a is TRUE and (LinearAddress + Offset) is aligned on RegionStart,
      //      but the length is SMALLER than the RegionLength
      //
      //
      Status = PageTableLibMapInLevel (
                 &PagingEntry[Index],
                 Buffer,
                 BufferSize,
                 Level - 1,
                 LinearAddress,
                 Length,
                 Offset,
                 Setting,
                 Mask
                 );
      if (RETURN_ERROR (Status)) {
        return Status;
      }
    }

    Offset      += SubLength;
    RegionStart += RegionLength;
    Index++;
  }

  return RETURN_SUCCESS;
}

/**
  Create or update page table to map [LinearAddress, LinearAddress + Length) with specified setting.

  @param[in, out] PageTable      The pointer to the page table to update, or pointer to NULL if a new page table is to be created.
  @param[in]      Buffer         The free buffer to be used for page table creation/updating.
  @param[in, out] BufferSize     The buffer size.
                                 On return, the remaining buffer size.
                                 The free buffer is used from the end so caller can supply the same Buffer pointer with an updated
                                 BufferSize in the second call to this API.
  @param[in]      Paging5L       TRUE when the PageTable points to 5-level page table.
  @param[in]      LinearAddress  The start of the linear address range.
  @param[in]      Length         The length of the linear address range.
  @param[in]      Setting        The setting of the linear address range.
                        All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                        Page table entries that map the linear address range are reset to 0 before set to the new setting
                        when a new physical base address is set.
  @param[in]      Mask           The mask used for setting. The corresponding field in Setting is ignored if that in Mask is 0.

  @retval RETURN_INVALID_PARAMETER  PageTable, Setting or Mask is NULL.
  @retval RETURN_BUFFER_TOO_SMALL   The buffer is too small for page table creation/updating.
                                    BufferSize is updated to indicate the expected buffer size.
                                    Caller may still get RETURN_BUFFER_TOO_SMALL with the new BufferSize.
  @retval RETURN_SUCCESS            PageTable is created/updated successfully.
**/
RETURN_STATUS
EFIAPI
PageTableMap (
  IN OUT UINTN               *PageTable  OPTIONAL,
  IN     VOID                *Buffer,
  IN OUT UINTN               *BufferSize,
  IN     BOOLEAN             Paging5L,
  IN     UINT64              LinearAddress,
  IN     UINT64              Length,
  IN     IA32_MAP_ATTRIBUTE  *Setting,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  )
{
  if ((PageTable == NULL) || (Setting == NULL) || (Mask == NULL)) {
    return RETURN_INVALID_PARAMETER;
  }

  return PageTableLibMapInLevel (
           (IA32_PAGING_ENTRY *)PageTable,
           Buffer,
           BufferSize,
           Paging5L ? 5 : 4,
           LinearAddress,
           Length,
           0,
           Setting,
           Mask
           );
}
