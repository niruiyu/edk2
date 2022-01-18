/** @file
  This library defines some routines that are generic for IA32 family CPU.

  The library routines are UEFI specification compliant.

  Copyright (c) 2020, AMD Inc. All rights reserved.<BR>
  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Register/Intel/Cpuid.h>

#include <Library/BaseLib.h>
#include <Library/UefiCpuLib.h>

typedef union {
  struct {
    UINT32    Pcid : 12;
  } Bits;
  UINT64    Uint64;
} IA32_CR3;

///
/// Format of a PML5 Entry (PML5E) that References a PML4 Table
///
typedef struct {
  UINT64    Present              : 1; // 0 = Not present in memory, 1 = Present in memory
  UINT64    ReadWrite            : 1; // 0 = Read-Only, 1= Read/Write
  UINT64    UserSupervisor       : 1; // 0 = Supervisor, 1=User
  UINT64    WriteThrough         : 1; // 0 = Write-Back caching, 1=Write-Through caching
  UINT64    CacheDisabled        : 1; // 0 = Cached, 1=Non-Cached
  UINT64    Accessed             : 1; // 0 = Not accessed, 1 = Accessed (set by CPU)
  UINT64    Available0           : 1; // Ignored
  UINT64    MustBeZero           : 1; // Must Be Zero

  UINT64    Available2           : 4; // Ignored

  UINT64    PageTableBaseAddress : 40; // Page Table Base Address
  UINT64    Available3           : 11; // Ignored
  UINT64    Nx                   : 1;  // No Execute bit
} IA32_PML5E;

///
/// Format of a PML4 Entry (PML4E) that References a Page-Directory-Pointer Table
///
typedef IA32_PML5E IA32_PML4E;

///
/// Format of a Page-Directory-Pointer-Table Entry (PDPTE) that References a Page Directory
///
typedef IA32_PML5E IA32_PDPTE;

///
/// Format of a Page-Directory Entry that References a Page Table
///
typedef IA32_PML5E IA32_PDE;

///
/// Format of a Page-Directory Entry that Maps a 2-MByte Page
///
typedef struct {
  UINT64    Present              : 1; // 0 = Not present in memory, 1 = Present in memory
  UINT64    ReadWrite            : 1; // 0 = Read-Only, 1= Read/Write
  UINT64    UserSupervisor       : 1; // 0 = Supervisor, 1=User
  UINT64    WriteThrough         : 1; // 0 = Write-Back caching, 1=Write-Through caching
  UINT64    CacheDisabled        : 1; // 0 = Cached, 1=Non-Cached
  UINT64    Accessed             : 1; // 0 = Not accessed, 1 = Accessed (set by CPU)
  UINT64    Dirty                : 1; // 0 = Not dirty, 1 = Dirty (set by CPU)
  UINT64    MustBeOne            : 1; // Page Size. Must Be One

  UINT64    Global               : 1; // 0 = Not global, 1 = Global (if CR4.PGE = 1)
  UINT64    Available1           : 3; // Ignored
  UINT64    Pat                  : 1; // PAT

  UINT64    PageTableBaseAddress : 39; // Page Table Base Address
  UINT64    Available3           : 7;  // Ignored
  UINT64    ProtectionKey        : 4;  // Protection key
  UINT64    Nx                   : 1;  // No Execute bit
} IA32_PDE_2M;

///
/// Format of a Page-Directory-Pointer-Table Entry (PDPTE) that Maps a 1-GByte Page
///
typedef IA32_PDE_2M IA32_PDPTE_1G;

///
/// Format of a Page-Table Entry that Maps a 4-KByte Page
///
typedef struct {
  UINT64    Present              : 1; // 0 = Not present in memory, 1 = Present in memory
  UINT64    ReadWrite            : 1; // 0 = Read-Only, 1= Read/Write
  UINT64    UserSupervisor       : 1; // 0 = Supervisor, 1=User
  UINT64    WriteThrough         : 1; // 0 = Write-Back caching, 1=Write-Through caching
  UINT64    CacheDisabled        : 1; // 0 = Cached, 1=Non-Cached
  UINT64    Accessed             : 1; // 0 = Not accessed, 1 = Accessed (set by CPU)
  UINT64    Dirty                : 1; // 0 = Not dirty, 1 = Dirty (set by CPU)
  UINT64    Pat                  : 1; // PAT

  UINT64    Global               : 1; // 0 = Not global, 1 = Global (if CR4.PGE = 1)
  UINT64    Available1           : 3; // Ignored

  UINT64    PageTableBaseAddress : 40; // Page Table Base Address
  UINT64    Available3           : 7;  // Ignored
  UINT64    ProtectionKey        : 4;  // Protection key
  UINT64    Nx                   : 1;  // No Execute bit
} IA32_PTE_4K;

typedef union {
  IA32_PML5E       Pml5;
  IA32_PML4E       Pml4;
  IA32_PDPTE       Pdpte;
  IA32_PDE         Pde;

  IA32_PDPTE_1G    Pdpte1G;
  IA32_PDE_2M      Pde2M;

  IA32_PTE_4K      Pte4K;

  UINT64           Uint64;
} IA32_PAGING_ENTRY;

typedef union {
  struct {
    UINT16    Present        : 1;   // 0 = Not present in memory, 1 = Present in memory
    UINT16    ReadWrite      : 1;   // 0 = Read-Only, 1= Read/Write
    UINT16    UserSupervisor : 1;   // 0 = Supervisor, 1=User
    UINT16    Pat            : 3;   // PAT << 2 + PCD << 1 + PWT
    UINT16    Accessed       : 1;   // 0 = Not accessed, 1 = Accessed (set by CPU)
    UINT16    Dirty          : 1;   // 0 = Not dirty, 1 = Dirty (set by CPU)

    UINT16    Global         : 1;
    UINT16    ProtectionKey  : 4;
    UINT16    Nx             : 1;
  } Bits;
  UINT16    Uint16;
} IA32_MAP_ATTRIBUTE;

typedef struct {
  UINT64                LinearAddress;
  UINT64                PhysicalAddress;
  UINT64                Size;
  IA32_MAP_ATTRIBUTE    Attribute;
} IA32_MAP_ENTRY;

UINT16
GetPdeMapAttribute (
  IA32_PDE  *Pde
  )
{
  IA32_MAP_ATTRIBUTE  MapAttribute;

  MapAttribute.Uint16 = 0;

  MapAttribute.Present        = Pde->Present;
  MapAttribute.ReadWrite      = Pde->ReadWrite;
  MapAttribute.UserSupervisor = Pde->UserSupervisor;
  MapAttribute.Pat            = Pde->CacheDisabled * 2 + Pde->WriteThrough;
  MapAttribute.Accessed       = Pde->Accessed;
  MapAttribute.Dirty          = Pde->Dirty;
  MapAttribute.Global         = Pde->Global;
  MapAttribute.ProtectionKey  = Pde->ProtectionKey;
  MapAttribute.Nx             = Pde->Nx;

  return MapAttribute.Uint16;
}

UINT16
GetPdpte1GMapAttribute (
  IA32_PDPTE_1G       *Pdpte1G,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute
  )
{
  IA32_MAP_ATTRIBUTE  MapAttribute;

  MapAttribute.Uint16 = 0;

  MapAttribute.Present        = ParentMapAttribute->Present & Pdpte1G->Present;
  MapAttribute.ReadWrite      = ParentMapAttribute->ReadWrite & Pdpte1G->ReadWrite;
  MapAttribute.UserSupervisor = ParentMapAttribute->UserSuperVisor & Pdpte1G->UserSupervisor;
  MapAttribute.Nx             = ParentMapAttribute->Nx | Pdpte1G->Nx;
  MapAttribute.Pat            = Pdpte1G->Pat * 4 + Pdpte1G->CacheDisabled * 2 + Pdpte1G->WriteThrough;
  MapAttribute.Accessed       = Pdpte1G->Accessed;
  MapAttribute.Dirty          = Pdpte1G->Dirty;
  MapAttribute.Global         = Pdpte1G->Global;
  MapAttribute.ProtectionKey  = Pdpte1G->ProtectionKey;

  return MapAttribute.Uint16;
}

UINT16
GetPde2MMapAttribute (
  IA32_PDE_2M         *Pde2M,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute
  )
{
  return GetPdpte1GMapAttribute ((IA32_PDPTE_1G *)Pde2M, ParentMapAttribute);
}

UINT16
GetPte4KMapAttribute (
  IA32_PTE_4K         *Pte4K,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute
  )
{
  IA32_MAP_ATTRIBUTE  MapAttribute;

  MapAttribute.Uint16 = 0;

  MapAttribute.Present        = ParentMapAttribute->Present & Pte4K->Present;
  MapAttribute.ReadWrite      = ParentMapAttribute->ReadWrite & Pte4K->ReadWrite;
  MapAttribute.UserSupervisor = ParentMapAttribute->UserSuperVisor & Pte4K->UserSupervisor;
  MapAttribute.Nx             = ParentMapAttribute->Nx | Pte4K->Nx;
  MapAttribute.Pat            = Pte4K->Pat * 4 + Pte4K->CacheDisabled * 2 + Pte4K->WriteThrough;
  MapAttribute.Accessed       = Pte4K->Accessed;
  MapAttribute.Dirty          = Pte4K->Dirty;
  MapAttribute.Global         = Pte4K->Global;
  MapAttribute.ProtectionKey  = Pte4K->ProtectionKey;

  return MapAttribute.Uint16;
}

VOID
AddMap (
  IA32_PAGING_ENTRY   *PagingEntry,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute,
  UINT64              LinearAddress,
  UINTN               Level,
  IA32_MAP_ENTRY      *Map,
  UINTN               *Count,
  UINTN               Capacity
  )
{
  UINTN               Index;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  UINT64              PhysicalAddress;
  UINT64              Size,

                      MapAttribute.Uint16 = 0;

  PhysicalAddress = 0;
  Size            = 0;

  switch (Level) {
    case 3:
      PhysicalAddress     = PagingEntry->Pdpte1G.PageTableBaseAddress;
      Size                = SIZE_1G;
      MapAttribute.Uint16 = GetPdpte1GMapAttribute (PagingEntry->Pdpte1G, ParentMapAttribute);
      break;

    case 2:
      PhysicalAddress     = PagingEntry->Pde2M.PageTableBaseAddress;
      Size                = SIZE_2M;
      MapAttribute.Uint16 = GetPde2MMapAttribute (PagingEntry->Pde2M, ParentMapAttribute);
      break;

    case 1:
      PhysicalAddress     = PagingEntry->Pte4K.PageTableBaseAddress;
      Size                = SIZE_4K;
      MapAttribute.Uint16 = GetPte4KMapAttribute (PagingEntry->Pte4K, ParentMapAttribute);
      break;

    default:
      ASSERT (Level == 1 || Level == 2 || Level == 3);
      break;
  }

  for (Index = 0; Index < *Count; Index++) {
    if ((Map[Index].PhysicalAddress + Map[Index].Size == PhysicalAddress) &&
        (Map[Index].Attribute.Uint16 == MapAttribute.Uint16)
        )
    {
      Map[Index].Size += Size;
      break;
    }

    if ((Map[Index].PhysicalAddress == PhysicalAddress + Size) &&
        (Map[Index].Attribute.Uint16 == MapAttribute.Uint16))
    {
      Map[Index].PhysicalAddress = PhysicalAddress;
      break;
    }
  }

  if (Index == *Count) {
    ASSERT (*Count < Capacity);
    if (*Count < Capacity) {
      Map[*Count].LinearAddress    = LinearAddress;
      Map[*Count].PhysicalAddress  = PhysicalAddress;
      Map[*Count].Size             = Size;
      Map[*Count].Attribute.Uint16 = MapAttribute.Uint16;
      *Count                       = *Count + 1;
    }
  }
}

typedef enum {
  CacheUncacheable    = 0,
  CacheWriteCombining = 1,
  CacheWriteThrough   = 4,
  CacheWriteProtected = 5,
  CacheWriteBack      = 6
} IA32_MEMORY_TYPE;

VOID
ParsePde (
  UINT64              PageTableBaseAddress,
  UINTN               Level,
  UINT64              LinearAddress,
  IA32_MAP_ATTRIBUTE  *ParentMapAttribute,
  IA32_MAP_ENTRY      *Map,
  UINTN               *MapCount,
  UINTN               MapCapacity
  )
{
  IA32_PAGING_ENTRY  *PagingEntry;
  UINTN              Index;
  UINTN              LeftShift;

  PagingEntry = (IA32_PAGING_ENTRY *)PageTableBaseAddress;
  LeftShift   = 12 + 9 * (Level - 1);

  for (Index = 0; Index < 512; Index++) {
    if (Level == 1) {
      AddMap (&PagingEntry[Index], Level, LinearAddress + LShiftU64 (Index, LeftShift), Map, &MapCount, MapCapacity);
    } else if ((Level == 2) && (PagingEntry[Index].Pde2M.MustBeOne == 1)) {
      AddMap (&PagingEntry[Index], Level, LinearAddress + LShiftU64 (Index, LeftShift), Map, &MapCount, MapCapacity);
    } else if ((Level == 3) && (PagingEntry[Index].Pdpte1G.MustBeOne == 1)) {
      AddMap (&PagingEntry[Index], Level, LinearAddress + LShiftU64 (Index, LeftShift), Map, &MapCount, MapCapacity);
    } else {
      ParsePde (PagingEntry[Index].Pde.PageTableBaseAddress, Level - 1, LinearAddress + LShiftU64 (Index, LeftShift), Map, &MapCount, MapCapacity);
    }
  }
}

/**
  Parse page table.

  @param  PageTableBase
**/
BOOLEAN
EFIAPI
PageTableParse (
  EFI_PHYSICAL_ADDRESS  PageTableBase,
  BOOLEAN               Paging5L,
  IA32_MEMORY_TYPE      *Pat,
  )
{
  CONST UINTN         MapCapacity = 256;
  IA32_MAP_ENTRY      Map[MapCapacity];
  UINTN               MapCount;
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
  MapAttribute.Uint16 = 0;

  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 1;

  MapCount = 0;
  ParsePde (PageTableBaseAddress, Paging5L ? 5 : 4, 0, &MapAttribute, Map, &MapCount, MapCapacity);
}

/**
  Create initial page table with 0 mapping.

  @param Buffer      Point to a buffer to hold the initial page table.
  @param BufferSize  The size of the buffer.
                     On return, the size of used buffer.
  @param Paging5L    To create a 5-level page table if it's TRUE.

  @retval RETURN_INVALID_PARAMETER
  @retval RETURN_BUFFER_TOO_SMALL
  @retval RETURN_SUCCESS
**/
RETURN_STATUS
EFIAPI
PageTableCreate (
  VOID     *Buffer,
  UINTN    *BufferSize,
  BOOLEAN  Paging5L
  )
{
  if (BufferSize == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (*BufferSize < SIZE_4K) {
    *BufferSize = SIZE_4K;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if (Buffer == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  *BufferSize = SIZE_4K;

  ZeroMem (Buffer, *BufferSize);
  return RETURN_SUCCESS;
}

/**
  It might create new page table entries that map LinearAddress to PhysicalAddress with specified MapAttribute.
  It might change existing page table entries to map LinearAddress to PhysicalAddress with specified MapAttribute.
  @param Buffer 
  @param BufferSize 
  @param Paging5L 
  @param LinearAddress 
  @param PhysicalAddress 
  @param Size 
  @param MapAttribute 
  @retval RETURN_BUFFER_TOO_SMALL
**/
RETURN_STATUS
EFIAPI
PageTableSetMap (
  VOID                *Buffer,
  UINTN               *BufferSize,
  BOOLEAN             Paging5L,
  UINT64              LinearAddress,
  UINT64              PhysicalAddress,
  UINT64              Size,
  IA32_MAP_ATTRIBUTE  *MapAttribute
  )
{
}
