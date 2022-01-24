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
  It might create new page table entries that map LinearAddress with specified MapAttribute.
  It might change existing page table entries to map LinearAddress with specified MapAttribute.

  @param PageTable
  @param Buffer
  @param BufferSize
  @param Paging5L
  @param LinearAddress
  @param Size
  @param Setting
  @param Mask
  @return RETURN_STATUS
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
  );

#endif
