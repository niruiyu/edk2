/** @file
  This library implements CpuPageTableLib that are generic for IA32 family CPU.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuPageTable.h"

/**
  Set the IA32_PTE_4K.

  @param[in] Pte4K     Pointer to IA32_PTE_4K.
  @param[in] Offset    The offset within the linear address range.
  @param[in] Attribute The attribute of the linear address range.
                       All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                       Page table entry is reset to 0 before set to the new attribute when a new physical base address is set.
  @param[in] Mask      The mask used for attribute. The corresponding field in Attribute is ignored if that in Mask is 0.
**/
VOID
PageTableLibSetPte4K (
  IN IA32_PTE_4K         *Pte4K,
  IN UINT64              Offset,
  IN IA32_MAP_ATTRIBUTE  *Attribute,
  IN IA32_MAP_ATTRIBUTE  *Mask
  )
{
  if (Mask->Bits.PageTableBaseAddress) {
    //
    // Reset all attributes when the physical address is changed.
    //
    Pte4K->Uint64 = IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (Attribute) + Offset;
  }

  if (Mask->Bits.Present) {
    Pte4K->Bits.Present = Attribute->Bits.Present;
  }

  if (Mask->Bits.ReadWrite) {
    Pte4K->Bits.ReadWrite = Attribute->Bits.ReadWrite;
  }

  if (Mask->Bits.UserSupervisor) {
    Pte4K->Bits.UserSupervisor = Attribute->Bits.UserSupervisor;
  }

  if (Mask->Bits.WriteThrough) {
    Pte4K->Bits.WriteThrough = Attribute->Bits.WriteThrough;
  }

  if (Mask->Bits.CacheDisabled) {
    Pte4K->Bits.CacheDisabled = Attribute->Bits.CacheDisabled;
  }

  if (Mask->Bits.Accessed) {
    Pte4K->Bits.Accessed = Attribute->Bits.Accessed;
  }

  if (Mask->Bits.Dirty) {
    Pte4K->Bits.Dirty = Attribute->Bits.Dirty;
  }

  if (Mask->Bits.Pat) {
    Pte4K->Bits.Pat = Attribute->Bits.Pat;
  }

  if (Mask->Bits.Global) {
    Pte4K->Bits.Global = Attribute->Bits.Global;
  }

  if (Mask->Bits.ProtectionKey) {
    Pte4K->Bits.ProtectionKey = Attribute->Bits.ProtectionKey;
  }

  if (Mask->Bits.Nx) {
    Pte4K->Bits.Nx = Attribute->Bits.Nx;
  }
}

/**
  Set the IA32_PDPTE_1G or IA32_PDE_2M.

  @param[in] PleB      Pointer to PDPTE_1G or PDE_2M. Both share the same structure definition.
  @param[in] Offset    The offset within the linear address range.
  @param[in] Attribute The attribute of the linear address range.
                       All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                       Page table entry is reset to 0 before set to the new attribute when a new physical base address is set.
  @param[in] Mask      The mask used for attribute. The corresponding field in Attribute is ignored if that in Mask is 0.
**/
VOID
PageTableLibSetPleB (
  IN IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE  *PleB,
  IN UINT64                             Offset,
  IN IA32_MAP_ATTRIBUTE                 *Attribute,
  IN IA32_MAP_ATTRIBUTE                 *Mask
  )
{
  if (Mask->Bits.PageTableBaseAddress) {
    //
    // Reset all attributes when the physical address is changed.
    //
    PleB->Uint64 = IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (Attribute) + Offset;
  }

  PleB->Bits.MustBeOne = 1;

  if (Mask->Bits.Present) {
    PleB->Bits.Present = Attribute->Bits.Present;
  }

  if (Mask->Bits.ReadWrite) {
    PleB->Bits.ReadWrite = Attribute->Bits.ReadWrite;
  }

  if (Mask->Bits.UserSupervisor) {
    PleB->Bits.UserSupervisor = Attribute->Bits.UserSupervisor;
  }

  if (Mask->Bits.WriteThrough) {
    PleB->Bits.WriteThrough = Attribute->Bits.WriteThrough;
  }

  if (Mask->Bits.CacheDisabled) {
    PleB->Bits.CacheDisabled = Attribute->Bits.CacheDisabled;
  }

  if (Mask->Bits.Accessed) {
    PleB->Bits.Accessed = Attribute->Bits.Accessed;
  }

  if (Mask->Bits.Dirty) {
    PleB->Bits.Dirty = Attribute->Bits.Dirty;
  }

  if (Mask->Bits.Pat) {
    PleB->Bits.Pat = Attribute->Bits.Pat;
  }

  if (Mask->Bits.Global) {
    PleB->Bits.Global = Attribute->Bits.Global;
  }

  if (Mask->Bits.ProtectionKey) {
    PleB->Bits.ProtectionKey = Attribute->Bits.ProtectionKey;
  }

  if (Mask->Bits.Nx) {
    PleB->Bits.Nx = Attribute->Bits.Nx;
  }
}

/**
  Set the IA32_PDPTE_1G, IA32_PDE_2M or IA32_PTE_4K.

  @param[in] Level     3, 2 or 1.
  @param[in] Ple       Pointer to PDPTE_1G, PDE_2M or IA32_PTE_4K, depending on the Level.
  @param[in] Offset    The offset within the linear address range.
  @param[in] Attribute The attribute of the linear address range.
                       All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                       Page table entry is reset to 0 before set to the new attribute when a new physical base address is set.
  @param[in] Mask      The mask used for attribute. The corresponding field in Attribute is ignored if that in Mask is 0.
**/
VOID
PageTableLibSetPle (
  IN UINTN               Level,
  IN IA32_PAGING_ENTRY   *Ple,
  IN UINT64              Offset,
  IN IA32_MAP_ATTRIBUTE  *Attribute,
  IN IA32_MAP_ATTRIBUTE  *Mask
  )
{
  if (Level == 1) {
    PageTableLibSetPte4K (&Ple->Pte4K, Offset, Attribute, Mask);
  } else {
    ASSERT (Level == 2 || Level == 3);
    PageTableLibSetPleB (&Ple->PleB, Offset, Attribute, Mask);
  }
}

/**
  Set the IA32_PML5, IA32_PML4, IA32_PDPTE or IA32_PDE.

  @param[in] Pnle      Pointer to IA32_PML5, IA32_PML4, IA32_PDPTE or IA32_PDE. All share the same structure definition.
  @param[in] Attribute The attribute of the page directory referenced by the non-leaf.
**/
VOID
PageTableLibSetPnle (
  IN IA32_PAGE_NON_LEAF_ENTRY  *Pnle,
  IN IA32_MAP_ATTRIBUTE        *Attribute
  )
{
  Pnle->Bits.Present        = Attribute->Bits.Present;
  Pnle->Bits.ReadWrite      = Attribute->Bits.ReadWrite;
  Pnle->Bits.UserSupervisor = Attribute->Bits.UserSupervisor;
  Pnle->Bits.Nx             = Attribute->Bits.Nx;
  Pnle->Bits.Accessed       = 0;

  //
  // Set the attributes (WT, CD, A) to 0.
  // WT and CD determin the memory type used to access the 4K page directory referenced by this entry.
  // So, it implictly requires PAT[0] is Write Back.
  // Create a new parameter if caller requires to use a different memory type for accessing page directories.
  //
  Pnle->Bits.WriteThrough  = 0;
  Pnle->Bits.CacheDisabled = 0;
}

/**
  Update page table to map [LinearAddress, LinearAddress + Length) with specified attribute in the specified level.

  @param[in]      ParentPagingEntry The pointer to the page table entry to update.
  @param[in]      Modify            FALSE to indicate Buffer is not used and BufferSize is increased by the required buffer size.
  @param[in]      Buffer            The free buffer to be used for page table creation/updating.
                                    When Modify is TRUE, it's used from the end.
                                    When Modify is FALSE, it's ignored.
  @param[in, out] BufferSize        The available buffer size.
                                    Return the remaining buffer size.
  @param[in]      Level             Page table level. Could be 5, 4, 3, 2, or 1.
  @param[in]      MaxLeafLevel      Maximum level that can be a leaf entry. Could be 1, 2 or 3 (if Page 1G is supported).
  @param[in]      LinearAddress     The start of the linear address range.
  @param[in]      Length            The length of the linear address range.
  @param[in]      Offset            The offset within the linear address range.
  @param[in]      Attribute         The attribute of the linear address range.
                                    All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                                    Page table entries that map the linear address range are reset to 0 before set to the new attribute
                                    when a new physical base address is set.
  @param[in]      Mask              The mask used for attribute. The corresponding field in Attribute is ignored if that in Mask is 0.

  @retval RETURN_SUCCESS            PageTable is created/updated successfully.
**/
RETURN_STATUS
PageTableLibMapInLevel (
  IN     IA32_PAGING_ENTRY   *ParentPagingEntry,
  IN     BOOLEAN             Modify,
  IN     VOID                *Buffer,
  IN OUT INTN                *BufferSize,
  IN     UINTN               Level,
  IN     UINTN               MaxLeafLevel,
  IN     UINT64              LinearAddress,
  IN     UINT64              Length,
  IN     UINT64              Offset,
  IN     IA32_MAP_ATTRIBUTE  *Attribute,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  )
{
  RETURN_STATUS       Status;
  UINTN               BitStart;
  UINTN               Index;
  IA32_PAGING_ENTRY   *PagingEntry;
  UINT64              RegionLength;
  UINT64              SubLength;
  UINT64              SubOffset;
  UINT64              RegionMask;
  UINT64              RegionStart;
  IA32_MAP_ATTRIBUTE  AllOneMask;
  IA32_MAP_ATTRIBUTE  PleBAttribute;
  IA32_MAP_ATTRIBUTE  NopAttribute;
  BOOLEAN             CreateNew;
  IA32_PAGING_ENTRY   OneOfPagingEntry;

  ASSERT (Level != 0);
  ASSERT ((Attribute != NULL) && (Mask != NULL));

  CreateNew         = FALSE;
  AllOneMask.Uint64 = ~0ull;

  NopAttribute.Uint64              = 0;
  NopAttribute.Bits.Present        = 1;
  NopAttribute.Bits.ReadWrite      = 1;
  NopAttribute.Bits.UserSupervisor = 1;

  //
  // ParentPagingEntry ONLY is deferenced for checking Present and MustBeOne bits
  // when Modify is FALSE.
  //

  if (ParentPagingEntry->Pce.Present == 0) {
    //
    // The parent entry is CR3 or PML5E/PML4E/PDPTE/PDE.
    // It does NOT point to an existing page directory.
    //
    ASSERT (Buffer == NULL || *BufferSize >= SIZE_4KB);
    CreateNew    = TRUE;
    *BufferSize -= SIZE_4KB;

    if (Modify) {
      ParentPagingEntry->Uintn = (UINTN)Buffer + *BufferSize;
      ZeroMem ((VOID *)ParentPagingEntry->Uintn, SIZE_4KB);
      //
      // Set default attribute bits for PML5E/PML4E/PDPTE/PDE.
      //
      PageTableLibSetPnle (&ParentPagingEntry->Pnle, &NopAttribute);
    } else {
      //
      // Just make sure Present and MustBeZero (PageSize) bits are accurate.
      //
      OneOfPagingEntry.Pnle.Uint64 = 0;
    }
  } else if (IsPle (ParentPagingEntry, Level + 1)) {
    ASSERT (Buffer == NULL || *BufferSize >= SIZE_4KB);
    CreateNew    = TRUE;
    *BufferSize -= SIZE_4KB;
    //
    // The parent entry is a PDPTE_1G or PDE_2M. Split to 2M or 4K pages.
    // Note: it's impossible the parent entry is a PTE_4K.
    //
    if (Modify) {
      //
      // Use NOP attributes as the attribute of grand-parents because CPU will consider
      // the actual attributes of grand-parents when determing the memory type.
      //
      PleBAttribute.Uint64 = PageTableLibGetPleBMapAttribute (&ParentPagingEntry->PleB, &NopAttribute);

      //
      // Create 512 child-level entries that map to 2M/4K.
      //
      ParentPagingEntry->Uintn = (UINTN)Buffer + *BufferSize;
      ZeroMem ((VOID *)ParentPagingEntry->Uintn, SIZE_4KB);

      //
      // Set NOP attributes
      // Note: Should NOT inherit the attributes from the original entry because a zero RW bit
      //       will make the entire region read-only even the child entries set the RW bit.
      //
      PageTableLibSetPnle (&ParentPagingEntry->Pnle, &NopAttribute);

      RegionStart  = IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS (&PleBAttribute);
      RegionLength = LShiftU64 (1, 12 + (Level - 1) * 9);
      PagingEntry  = (IA32_PAGING_ENTRY *)(UINTN)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (&ParentPagingEntry->Pnle);
      for (SubOffset = 0, Index = 0; Index < 512; Index++) {
        PageTableLibSetPle (Level, &PagingEntry[Index], SubOffset, &PleBAttribute, &AllOneMask);
        SubOffset += RegionLength;
      }
    } else {
      //
      // Just make sure Present and MustBeZero (PageSize) bits are accurate.
      //
      PageTableLibSetPle (Level, &OneOfPagingEntry, 0, &NopAttribute, &AllOneMask);
    }
  }

  //
  // RegionLength: 256T (1 << 48) 512G (1 << 39), 1G (1 << 30), 2M (1 << 21) or 4K (1 << 12).
  // RegionStart:  points to the linear address that's aligned on RegionLength and lower than (LinearAddress + Offset).
  //
  BitStart     = 12 + (Level - 1) * 9;
  Index        = BitFieldRead64 (LinearAddress + Offset, BitStart, BitStart + 9 - 1);
  RegionLength = LShiftU64 (1, BitStart);
  RegionMask   = RegionLength - 1;
  RegionStart  = (LinearAddress + Offset) & ~RegionMask;

  //
  // Apply the attribute.
  //
  PagingEntry = (IA32_PAGING_ENTRY *)(UINTN)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (&ParentPagingEntry->Pnle);
  while (Offset < Length && Index < 512) {
    SubLength = MIN (Length - Offset, RegionStart + RegionLength - (LinearAddress + Offset));
    if ((Level <= MaxLeafLevel) && (LinearAddress + Offset == RegionStart) && (SubLength == RegionLength)) {
      //
      // Create one entry mapping the entire region (1G, 2M or 4K).
      //
      if (Modify) {
        PageTableLibSetPle (Level, &PagingEntry[Index], Offset, Attribute, Mask);
      }
    } else {
      //
      // Recursively call to create page table.
      // There are 3 cases:
      //   a. Level cannot be a leaf entry which points to physical memory.
      //   a. Level can be a leaf entry but (LinearAddress + Offset) is NOT aligned on the RegionStart.
      //   b. Level can be a leaf entry and (LinearAddress + Offset) is aligned on RegionStart,
      //      but the length is SMALLER than the RegionLength.
      //
      Status = PageTableLibMapInLevel (
                 (!Modify && CreateNew) ? &OneOfPagingEntry : &PagingEntry[Index],
                 Modify,
                 Buffer,
                 BufferSize,
                 Level - 1,
                 MaxLeafLevel,
                 LinearAddress,
                 Length,
                 Offset,
                 Attribute,
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
  Create or update page table to map [LinearAddress, LinearAddress + Length) with specified attribute.

  @param[in, out] PageTable      The pointer to the page table to update, or pointer to NULL if a new page table is to be created.
  @param[in]      PagingMode     The paging mode.
  @param[in]      Buffer         The free buffer to be used for page table creation/updating.
  @param[in, out] BufferSize     The buffer size.
                                 On return, the remaining buffer size.
                                 The free buffer is used from the end so caller can supply the same Buffer pointer with an updated
                                 BufferSize in the second call to this API.
  @param[in]      LinearAddress  The start of the linear address range.
  @param[in]      Length         The length of the linear address range.
  @param[in]      Attribute      The attribute of the linear address range.
                                 All non-reserved fields in IA32_MAP_ATTRIBUTE are supported to set in the page table.
                                 Page table entries that map the linear address range are reset to 0 before set to the new attribute
                                 when a new physical base address is set.
  @param[in]      Mask           The mask used for attribute. The corresponding field in Attribute is ignored if that in Mask is 0.

  @retval RETURN_UNSUPPORTED        PagingMode is not supported.
  @retval RETURN_INVALID_PARAMETER  PageTable, BufferSize, Attribute or Mask is NULL.
  @retval RETURN_INVALID_PARAMETER  *BufferSize is not multiple of 4KB.
  @retval RETURN_BUFFER_TOO_SMALL   The buffer is too small for page table creation/updating.
                                    BufferSize is updated to indicate the expected buffer size.
                                    Caller may still get RETURN_BUFFER_TOO_SMALL with the new BufferSize.
  @retval RETURN_SUCCESS            PageTable is created/updated successfully.
**/
RETURN_STATUS
EFIAPI
PageTableMap (
  IN OUT UINTN               *PageTable  OPTIONAL,
  IN     PAGING_MODE         PagingMode,
  IN     VOID                *Buffer,
  IN OUT UINTN               *BufferSize,
  IN     UINT64              LinearAddress,
  IN     UINT64              Length,
  IN     IA32_MAP_ATTRIBUTE  *Attribute,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  )
{
  RETURN_STATUS      Status;
  IA32_PAGING_ENTRY  TopPagingEntry;
  INTN               RequiredSize;
  UINT64             MaxLinearAddress;
  UINTN              MaxLevel;
  BOOLEAN            MaxLeafLevel;

  if ((PagingMode == Paging32bit) || (PagingMode == PagingPae) || (PagingMode >= PagingModeMax)) {
    //
    // 32bit paging is never supported.
    // PAE paging will be supported later.
    //
    return RETURN_UNSUPPORTED;
  }

  if ((PageTable == NULL) || (BufferSize == NULL) || (Attribute == NULL) || (Mask == NULL)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (*BufferSize % SIZE_4KB != 0) {
    //
    // BufferSize should be multiple of 4K.
    //
    return RETURN_INVALID_PARAMETER;
  }

  if ((*BufferSize != 0) && (Buffer == NULL)) {
    return RETURN_INVALID_PARAMETER;
  }

  MaxLeafLevel     = (UINT8)PagingMode;
  MaxLevel         = (UINT8)(PagingMode >> 8);
  MaxLinearAddress = LShiftU64 (1, 12 + MaxLevel * 9);

  if ((LinearAddress > MaxLinearAddress) || (Length > MaxLinearAddress - LinearAddress)) {
    //
    // Maximum linear address is (1 << 48) or (1 << 57)
    //
    return RETURN_INVALID_PARAMETER;
  }

  TopPagingEntry.Uintn = *PageTable;
  if (TopPagingEntry.Uintn != 0) {
    TopPagingEntry.Pce.Present = 1;
  }

  //
  // Query the required buffer size without modifying the page table.
  // Assume 4GB buffer is enough for any page table.
  //
  RequiredSize = 0;
  Status       = PageTableLibMapInLevel (
                   &TopPagingEntry,
                   FALSE,
                   NULL,
                   &RequiredSize,
                   MaxLevel,
                   MaxLeafLevel,
                   LinearAddress,
                   Length,
                   0,
                   Attribute,
                   Mask
                   );
  if (RETURN_ERROR (Status)) {
    return Status;
  }

  RequiredSize = -RequiredSize;

  if ((UINTN)RequiredSize > *BufferSize) {
    *BufferSize = RequiredSize;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if ((RequiredSize != 0) && (Buffer == NULL)) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // Update the page table when the supplied buffer is sufficient.
  //
  Status = PageTableLibMapInLevel (
             &TopPagingEntry,
             TRUE,
             Buffer,
             BufferSize,
             MaxLevel,
             MaxLeafLevel,
             LinearAddress,
             Length,
             0,
             Attribute,
             Mask
             );
  if (!RETURN_ERROR (Status)) {
    *PageTable = (UINTN)(TopPagingEntry.Uintn & IA32_PE_BASE_ADDRESS_MASK_40);
  }

  return Status;
}
