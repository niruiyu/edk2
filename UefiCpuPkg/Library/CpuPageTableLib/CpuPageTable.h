#ifndef CPU_PAGE_TABLE_H__
#define CPU_PAGE_TABLE_H__

#include <Base.h>
#ifdef UNIT_TEST
  #include <assert.h>
  #include <string.h>
#define ASSERT  assert
#define ZeroMem(p, l)                              memset(p, 0, l)
#define LShiftU64(Operand, Count)                  (((UINT64)Operand) << (Count))
#define RShiftU64(Operand, Count)                  (((UINT64)Operand) >> (Count))
#define BitFieldRead64(Operand, StartBit, EndBit)  RShiftU64(Operand & ~LShiftU64((UINT64) -2, EndBit), StartBit);

#else
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/DebugLib.h>
#endif

#include <Library/CpuPageTableLib.h>

typedef struct {
  UINT64    Present        : 1;       // 0 = Not present in memory, 1 = Present in memory
  UINT64    ReadWrite      : 1;       // 0 = Read-Only, 1= Read/Write
  UINT64    UserSupervisor : 1;       // 0 = Supervisor, 1=User
  UINT64    WriteThrough   : 1;       // 0 = Write-Back caching, 1=Write-Through caching
  UINT64    CacheDisabled  : 1;       // 0 = Cached, 1=Non-Cached
  UINT64    Accessed       : 1;       // 0 = Not accessed, 1 = Accessed (set by CPU)

  UINT64    DontCare       : 57;
  UINT64    Nx             : 1;        // No Execute bit
} IA32_PAGE_COMMON_ENTRY;

///
/// Format of a non-leaf entry that references a page table entry
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
} IA32_PAGE_NON_LEAF_ENTRY;

///
/// Format of a PML5 Entry (PML5E) that References a PML4 Table
///
typedef IA32_PAGE_NON_LEAF_ENTRY IA32_PML5E;

///
/// Format of a PML4 Entry (PML4E) that References a Page-Directory-Pointer Table
///
typedef IA32_PAGE_NON_LEAF_ENTRY IA32_PML4E;

///
/// Format of a Page-Directory-Pointer-Table Entry (PDPTE) that References a Page Directory
///
typedef IA32_PAGE_NON_LEAF_ENTRY IA32_PDPTE;

///
/// Format of a Page-Directory Entry that References a Page Table
///
typedef IA32_PAGE_NON_LEAF_ENTRY IA32_PDE;

///
/// Format of a leaf entry that Maps a 1-Gbyte or 2-MByte Page
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
} IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE;

///
/// Format of a Page-Directory Entry that Maps a 2-MByte Page
///
typedef IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE IA32_PDE_2M;

///
/// Format of a Page-Directory-Pointer-Table Entry (PDPTE) that Maps a 1-GByte Page
///
typedef IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE IA32_PDPTE_1G;

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
  IA32_PAGE_NON_LEAF_ENTRY             Pnle; // To access Pml5, Pml4, Pdpte and Pde.
  IA32_PML5E                           Pml5;
  IA32_PML4E                           Pml4;
  IA32_PDPTE                           Pdpte;
  IA32_PDE                             Pde;

  IA32_PAGE_LEAF_ENTRY_BIG_PAGESIZE    PleB; // to access Pdpte1G and Pde2M.
  IA32_PDPTE_1G                        Pdpte1G;
  IA32_PDE_2M                          Pde2M;

  IA32_PTE_4K                          Pte4K;

  IA32_PAGE_COMMON_ENTRY               Pce; // To access all common bits in above entries.

  UINTN                                Uintn;
} IA32_PAGING_ENTRY;


#endif
