/** @file
  Public include file for PageTableLib library.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PAGE_TABLE_LIB_H_
#define PAGE_TABLE_LIB_H_

typedef union {
  struct {
    UINT64    Present              : 1; // 0 = Not present in memory, 1 = Present in memory
    UINT64    ReadWrite            : 1; // 0 = Read-Only, 1= Read/Write
    UINT64    UserSupervisor       : 1; // 0 = Supervisor, 1=User
    UINT64    WriteThrough         : 1; // 0 = Write-Back caching, 1=Write-Through caching
    UINT64    CacheDisabled        : 1; // 0 = Cached, 1=Non-Cached
    UINT64    Accessed             : 1; // 0 = Not accessed, 1 = Accessed (set by CPU)
    UINT64    Dirty                : 1; // 0 = Not dirty, 1 = Dirty (set by CPU)
    UINT64    Pat                  : 1; // PAT

    UINT64    Global               : 1; // 0 = Not global, 1 = Global (if CR4.PGE = 1)
    UINT64    Reserved1            : 3; // Ignored

    UINT64    PageTableBaseAddress : 40; // Page Table Base Address
    UINT64    Reserved2            : 7;  // Ignored
    UINT64    ProtectionKey        : 4;  // Protection key
    UINT64    Nx                   : 1;  // No Execute bit
  } Bits;
  UINT64    Uint64;
} IA32_MAP_ATTRIBUTE;

#define IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS_MASK 0xFFFFFFFFFF000ull
#define IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS(pa) ((pa)->Uint64 & IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS_MASK)
#define IA32_MAP_ATTRIBUTE_ATTRIBUTES(pa)              ((pa)->Uint64 & ~IA32_MAP_ATTRIBUTE_PAGE_TABLE_BASE_ADDRESS_MASK)

typedef struct {
  UINT64                LinearAddress;
  UINT64                Length;
  IA32_MAP_ATTRIBUTE    Attribute;
} IA32_MAP_ENTRY;


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
  OUT    UINTN               *PageTable  OPTIONAL,
  IN     VOID                *Buffer,
  IN OUT UINTN               *BufferSize,
  IN     BOOLEAN             Paging5L,
  IN     UINT64              LinearAddress,
  IN     UINT64              Size,
  IN     IA32_MAP_ATTRIBUTE  *Setting,
  IN     IA32_MAP_ATTRIBUTE  *Mask
  );

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
  UINTN           PageTable,
  BOOLEAN         Paging5L,
  IA32_MAP_ENTRY  *Map,
  UINTN           *MapCount
  );

#endif
