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

  @param PageTableBuffer      Point to a buffer to hold the initial page table.
                              On return, the initial page table.
  @param BufferSize           The size of the buffer.
                              On return, the size of used buffer.
  @param Paging5L             To create a 5-level page table if it's TRUE.

  @retval RETURN_INVALID_PARAMETER
  @retval RETURN_BUFFER_TOO_SMALL
  @retval RETURN_SUCCESS
**/
RETURN_STATUS
PageTableCreateZeroMappingPageTable (
  OUT    UINT64  *PageTable,
  IN     VOID    *Buffer,
  IN OUT UINTN   *BufferSize
  )
{
  if (BufferSize == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (*BufferSize < SIZE_4KB) {
    *BufferSize = SIZE_4KB;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if ((Buffer == NULL) || (((UINTN)Buffer & 0xFFF) != 0)) {
    return RETURN_INVALID_PARAMETER;
  }

  *PageTable = (UINT64)(UINTN)Buffer;
  ZeroMem (Buffer, SIZE_4KB);
  *BufferSize = SIZE_4KB;

  return RETURN_SUCCESS;
}

/**
  Create initial page table with 0 mapping.
  TODO: Another way is to remove this API. NULL/0 means 0-mapping page table. But 0 cannot be set to CR3.
        Question is: do we need to create a 0-mapping page table and set it to CR3?

  @param PageTableBuffer      Point to a buffer to hold the initial page table.
                              On return, the initial page table.
  @param BufferSize           The size of the buffer.
                              On return, the size of used buffer.
  @param Paging5L             To create a 5-level page table if it's TRUE.

  @retval RETURN_INVALID_PARAMETER
  @retval RETURN_BUFFER_TOO_SMALL
  @retval RETURN_SUCCESS
**/
RETURN_STATUS
PageTableCreateZeroMappingPnle (
  OUT    IA32_PAGE_NON_LEAF_ENTRY  *Pnle,
  IN     VOID                      *Buffer,
  IN OUT UINTN                     *BufferSize
  )
{
  if (BufferSize == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (*BufferSize < SIZE_4KB) {
    *BufferSize = SIZE_4KB;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if ((Buffer == NULL) || (((UINTN)Buffer & 0xFFF) != 0)) {
    return RETURN_INVALID_PARAMETER;
  }

  ZeroMem (Buffer, SIZE_4KB);
  *BufferSize = SIZE_4KB;

  ZeroMem (Pnle, sizeof (*Pnle));
  Pnle->Present              = 1;
  Pnle->ReadWrite            = 1;
  Pnle->PageTableBaseAddress = (UINT64)(UINTN)Buffer;
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
  if (Mask->Bits.Present) {
    Pte4K->Present = Setting->Bits.Present;
  }

  if (Mask->Bits.ReadWrite) {
    Pte4K->ReadWrite = Setting->Bits.ReadWrite;
  }

  if (Mask->Bits.UserSupervisor) {
    Pte4K->UserSupervisor = Setting->Bits.UserSupervisor;
  }

  if (Mask->Bits.WriteThrough) {
    Pte4K->WriteThrough = Setting->Bits.WriteThrough;
  }

  if (Mask->Bits.CacheDisabled) {
    Pte4K->CacheDisabled = Setting->Bits.CacheDisabled;
  }

  if (Mask->Bits.Accessed) {
    Pte4K->Accessed = Setting->Bits.Accessed;
  }

  if (Mask->Bits.Dirty) {
    Pte4K->Dirty = Setting->Bits.Dirty;
  }

  if (Mask->Bits.Pat) {
    Pte4K->Pat = Setting->Bits.Pat;
  }

  if (Mask->Bits.Global) {
    Pte4K->Global = Setting->Bits.Global;
  }

  if (Mask->Bits.PageTableBaseAddress) {
    Pte4K->PageTableBaseAddress = Setting->Bits.PageTableBaseAddress + Offset;
  }

  if (Mask->Bits.ProtectionKey) {
    Pte4K->ProtectionKey = Setting->Bits.ProtectionKey;
  }

  if (Mask->Bits.Nx) {
    Pte4K->Nx = Setting->Bits.Nx;
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
  if (Mask->Bits.Present) {
    PleB->Present = Setting->Bits.Present;
  }

  if (Mask->Bits.ReadWrite) {
    PleB->ReadWrite = Setting->Bits.ReadWrite;
  }

  if (Mask->Bits.UserSupervisor) {
    PleB->UserSupervisor = Setting->Bits.UserSupervisor;
  }

  if (Mask->Bits.WriteThrough) {
    PleB->WriteThrough = Setting->Bits.WriteThrough;
  }

  if (Mask->Bits.CacheDisabled) {
    PleB->CacheDisabled = Setting->Bits.CacheDisabled;
  }

  if (Mask->Bits.Accessed) {
    PleB->Accessed = Setting->Bits.Accessed;
  }

  if (Mask->Bits.Dirty) {
    PleB->Dirty = Setting->Bits.Dirty;
  }

  if (Mask->Bits.Pat) {
    PleB->Pat = Setting->Bits.Pat;
  }

  if (Mask->Bits.Global) {
    PleB->Global = Setting->Bits.Global;
  }

  if (Mask->Bits.PageTableBaseAddress) {
    PleB->PageTableBaseAddress = Setting->Bits.PageTableBaseAddress + Offset;
  }

  if (Mask->Bits.ProtectionKey) {
    PleB->ProtectionKey = Setting->Bits.ProtectionKey;
  }

  if (Mask->Bits.Nx) {
    PleB->Nx = Setting->Bits.Nx;
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
  IN     IA32_MAP_ATTRIBUTE  *Setting,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  )
{
  RETURN_STATUS      Status;
  UINTN              TotalBufferSize;
  UINTN              BitStart;
  UINTN              Index;
  IA32_PAGING_ENTRY  *PagingEntry;
  UINT64             Offset;

  if (((Level == 5) && (ParentPagingEntry->Uintn == 0)) ||
      ((Level < 5) && (ParentPagingEntry->Pce.Present == 0))
      )
  {
    ASSERT (Level > 1);
    TotalBufferSize = *BufferSize;
    if (Level == 5) {
      Status = PageTableCreateZeroMappingPageTable (&ParentPagingEntry->Uintn, Buffer, BufferSize);
    } else {
      Status = PageTableCreateZeroMappingPnle (&ParentPagingEntry->Pnle, Buffer, BufferSize);
    }

    if (RETURN_ERROR (Status)) {
      return Status;
    }

    ASSERT (*BufferSize == SIZE_4KB);
    Buffer      = (VOID *)((UINTN)Buffer + SIZE_4KB);
    *BufferSize = TotalBufferSize - SIZE_4KB;
  }

  PagingEntry = (IA32_PAGING_ENTRY *)(UINTN)ParentPagingEntry->Pnle.PageTableBaseAddress;
  BitStart    = 12 + (Level - 1) * 9;
  Index       = BitFieldRead64 (LinearAddress, BitStart, BitStart + 9 - 1);
  Offset      = 0;
  while (Offset < Size) {
    if ((Level <= 3) && (Size - Offset > LShiftU64 (1, BitStart))) {
      //
      // Do not split the page table.
      // Set one entry to map to entire (1 << BitStart) (1G, 2M, 4K) region of linear address space.
      //
      if (Level == 1) {
        PageTableLibSetPte4K (&PagingEntry[Index].Pte4K, Offset, Setting, Mask);
      } else {
        PageTableLibSetPleB (&PagingEntry[Index].PleB, Offset, Setting, Mask);
      }

      Offset += LShiftU64 (1, BitStart);
    } else {
      return PageTableLibSetMapInLevel (
               &PagingEntry[Index],
               Buffer,
               BufferSize,
               Level - 1,
               LinearAddress + Offset,
               Size - Offset,
               Setting,
               Mask
               );
    }
  }

  ASSERT (Offset == Size);
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
           Setting,
           Mask
           );
}
