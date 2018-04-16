#ifndef _PMEM_BLOCK_IO_DXE_H_
#define _PMEM_BLOCK_IO_DXE_H_
#include <Uefi.h>
#include <Register/Cpuid.h>
#include <Protocol/NvdimmLabel.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Guid/Acpi.h>
#include <Guid/Btt.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/SortLib.h>
#include <Library/SafeIntLib.h>

#include "InternalBtt.h"

extern CHAR8 *gEfiCallerBaseName;
extern EFI_GUID  gNvdimmControlRegionGuid;
extern EFI_GUID  gNvdimmPersistentMemoryRegionGuid;
extern EFI_GUID  gNvdimmBlockDataWindowRegionGuid;

#define BW_APERTURE_LENGTH   8192
#define CACHE_LINE_SIZE      64

typedef
VOID
(*CACHE_LINE_FLUSH) (
  VOID *Address
  );

VOID
EFIAPI
AsmFlushCacheLineOpt (
  IN      VOID                      *LinearAddress
  );

VOID
EFIAPI
AsmStoreFence (
  VOID
);

VOID
InitializeCpuCommands (
  VOID
);
typedef union {
  struct {
    UINT32   DpaLo;
    UINT32   DpaHi : 5;
    UINT32   Reserved_37 : 11;
    UINT32   Size : 8;
    UINT32   Write : 1;
    UINT32   Reserved_57 : 7;
  } Bits;
  UINT64     Uint64;
} BW_COMMAND_REGISTER;

typedef union {
  struct {
    UINT32   InvalidAddr : 1;
    UINT32   UncorrectableError: 1;
    UINT32   ReadMisMatch : 1;
    UINT32   Reserved_3 : 1;
    UINT32   DpaRangeLocked : 1;
    UINT32   BwDisabled: 1;
    UINT32   Reserved_6 : 25;
    UINT32   Pending : 1;
  } Bits;
  UINT32     Uint32;
} BW_STATUS_REGISTER;

typedef struct _NVDIMM NVDIMM;

typedef struct {
  BOOLEAN                            Started;    ///< The driver is started or not.
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs[EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE_TYPE + 1];
  UINTN                              NfitStrucCount[EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE_TYPE + 1];
  LIST_ENTRY                         Nvdimms;    ///< list of NVDIMM.
  LIST_ENTRY                         Namespaces; ///< List of NVDIMM_NAMESPACE.
} PMEM;

typedef struct {
  NVDIMM                          *Nvdimm;    ///< Point to the NVDIMM the label is in.
  EFI_NVDIMM_LABEL                *Label;
} NVDIMM_LABEL;

typedef enum {
  NamespaceTypeBlock,
  NamespaceTypePmem
} NAMESPACE_TYPE;

#pragma pack (1)
typedef struct {
  NVDIMM_NAMESPACE_DEVICE_PATH    NvdimmNamespace;
  EFI_DEVICE_PATH_PROTOCOL        End;
} NVDIMM_NAMESPACE_FULL_DEVICE_PATH;
#pragma pack()

typedef struct {
  UINT32                            Signature;
  LIST_ENTRY                        Link;
  EFI_GUID                          Uuid;
  NAMESPACE_TYPE                    Type;
  BTT_HANDLE                        BttHandle;
  BOOLEAN                           ReadOnly;
  CHAR8                             Name[EFI_NVDIMM_LABEL_NAME_LEN];
  EFI_GUID                          AddressAbstractionGuid;
  UINT8                             *PmSpaBase;
  NVDIMM_LABEL                      *Labels;        ///< Array of labels which assembly the namespace.
  UINTN                             LabelCount;     ///< Count of labels which assembly the namespace.
  UINTN                             LabelCapacity;  ///< Capacity of labels which holds the labels.
                                                    ///< Useful for Local namespaces for which LabelCount is unknown until assembling is completed.
  UINT64                            SetCookie;
  UINT32                            BlockSize;
  UINT64                            TotalSize;
  EFI_HANDLE                        Handle;
  EFI_BLOCK_IO_MEDIA                Media;
  EFI_BLOCK_IO_PROTOCOL             BlockIo;
  NVDIMM_NAMESPACE_FULL_DEVICE_PATH DevicePath;
} NVDIMM_NAMESPACE;
#define NVDIMM_NAMESPACE_SIGNATURE        SIGNATURE_32 ('n', 'd', 'n', 's')
#define NVDIMM_NAMESPACE_FROM_LINK(l)     CR (l, NVDIMM_NAMESPACE, Link,    NVDIMM_NAMESPACE_SIGNATURE)
#define NVDIMM_NAMESPACE_FROM_BLOCK_IO(b) CR (b, NVDIMM_NAMESPACE, BlockIo, NVDIMM_NAMESPACE_SIGNATURE)

typedef struct _NVDIMM {
  UINT32                                         Signature;
  LIST_ENTRY                                     Link;
  EFI_ACPI_6_0_NFIT_DEVICE_HANDLE                DeviceHandle;

  EFI_HANDLE                                     Handle;
  UINT8                                          *LabelStorageData;
  EFI_NVDIMM_LABEL_INDEX_BLOCK                   *LabelIndexBlock;
  EFI_NVDIMM_LABEL                               *Labels;

  EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE *FlushHintAddress;

  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *PmSpa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *PmMap;
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *PmInterleave;
  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *PmControl;

  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *BlockControlSpa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *BlockControlMap;
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *BlockControlInterleave;
  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *BlockControl;
  volatile BW_COMMAND_REGISTER                                          *BlockControlCommand;
  volatile BW_STATUS_REGISTER                                           *BlockControlStatus;

  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *BlockDataWindowSpa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *BlockDataWindowMap;
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *BlockDataWindowInterleave;
  EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE           *BlockDataWindow;
  UINT32                                                                NumberOfSegments;
  UINT8                                                                 **BlockDataWindowAperture;

} NVDIMM;
#define NVDIMM_SIGNATURE                 SIGNATURE_32 ('_', 'n', 'v', 'd')
#define NVDIMM_FROM_LINK(Link)           CR (Link, NVDIMM, Link, NVDIMM_SIGNATURE)

typedef struct {
  UINT32                                                    Signature;
  LIST_ENTRY                                                Link;
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE Spa;
} SPA;
#define SPA_SIGNATURE                 SIGNATURE_32 ('_', 's', 'p', 'a')
#define SPA_FROM_LINK(Link)           CR (Link, SPA, Link, SPA_SIGNATURE)


VOID
InitializeBlockIo (
  IN OUT NVDIMM_NAMESPACE   *Namespace
);
NVDIMM *
LocateNvdimm (
  LIST_ENTRY                       *List,
  EFI_ACPI_6_0_NFIT_DEVICE_HANDLE  *DeviceHandle,
  BOOLEAN                          Create
);


extern PMEM mPmem;

EFI_STATUS
ParseNfit (
  VOID
);

VOID
FreeNvdimms (
  LIST_ENTRY    *List
);
RETURN_STATUS
DeviceRegionOffsetToSpa (
  UINT64                                                                RegionOffset,
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *Spa,
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map,
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave, OPTIONAL
  UINT8                                                                 **Address
);
EFI_STATUS
NvdimmBlockIoReadWriteBytes (
  IN NVDIMM_NAMESPACE               *Namespace,
  IN BOOLEAN                        Write,
  IN UINT64                         Offset,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
);
#endif
