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
#include "NvdimmNamespaceBlk.h"


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


typedef struct _NVDIMM NVDIMM;

typedef struct {
  BOOLEAN                            Initialized; ///< Whether NFIT and NVDIMM are initialized.
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs[EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE_TYPE + 1];
  UINTN                              NfitStrucCount[EFI_ACPI_6_0_NFIT_FLUSH_HINT_ADDRESS_STRUCTURE_TYPE + 1];
  LIST_ENTRY                         Nvdimms;     ///< list of NVDIMM.
  LIST_ENTRY                         Namespaces;  ///< List of namespaces.
} PMEM;

typedef struct {
  NVDIMM                          *Nvdimm;    ///< Point to the NVDIMM the label is in.
  EFI_NVDIMM_LABEL                *Label;
} NVDIMM_LABEL;

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
  BLK                                                                   Blk;
} NVDIMM;
#define NVDIMM_SIGNATURE                 SIGNATURE_32 ('_', 'n', 'v', 'd')
#define NVDIMM_FROM_LINK(Link)           CR (Link, NVDIMM, Link, NVDIMM_SIGNATURE)

typedef enum {
  NamespaceTypeBlock,
  NamespaceTypePmem
} NAMESPACE_TYPE;

typedef struct _NVDIMM_NAMESPACE {
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
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
} NVDIMM_NAMESPACE;
#define NVDIMM_NAMESPACE_SIGNATURE        SIGNATURE_32 ('n', 'd', 'n', 's')
#define NVDIMM_NAMESPACE_FROM_LINK(l)     CR (l, NVDIMM_NAMESPACE, Link,    NVDIMM_NAMESPACE_SIGNATURE)
#define NVDIMM_NAMESPACE_FROM_BLOCK_IO(b) CR (b, NVDIMM_NAMESPACE, BlockIo, NVDIMM_NAMESPACE_SIGNATURE)


/**

**/
VOID
OpenNvdimmLabelsByChild (
  NVDIMM_NAMESPACE          *Namespace
);

/**
  Initialize EFI_BLOCK_IO_PROTOCOL instance for the namespace.

  @param  Namespace   Pointer to NVDIMM_NAMESPACE.
**/
VOID
InitializeBlockIo (
  IN OUT NVDIMM_NAMESPACE   *Namespace
);

/**
  Flush the content update to NVDIMM.

  @param Nvdimm    Pointer to the NVDIMM instance.
**/
VOID
WpqFlush (
  IN CONST  NVDIMM    *Nvdimm
);

/**
  Locate or create the NVDIMM instance in the list.

  @param List         The NVDIMM instance list.
  @param DeviceHandle The unique device handle to identify the NVDIMM instance.
  @param Create       TRUE to create one if the specified NVDIMM instance cannot be found in list.

  @return  The found NVDIMM instance or newly created one.
**/
NVDIMM *
LocateNvdimm (
  LIST_ENTRY                       *List,
  EFI_ACPI_6_0_NFIT_DEVICE_HANDLE  *DeviceHandle,
  BOOLEAN                          Create
);

/**
  Free the NVDIMM instance.

  @param Nvdimm    Pointer to the NVDIMM instance.
**/
VOID
FreeNvdimm (
  NVDIMM        *Nvdimm
);

/**
  Free the NVDIMM instance list.

  @param List         The NVDIMM instance list.
**/
VOID
FreeNvdimms (
  LIST_ENTRY    *List
);

/**
  Perform byte-level of read or write operation on the specified namespace.

  @param Namespace   Pointer to NVDIMM_NAMESPACE.
  @param Write       TRUE indicates write operation; FALSE indicates read operation.
  @param Offset      The offset within the namespace.
  @param BufferSize  The size of the data to read or write.
  @param Buffer      Receive the data to read, or supply the data to write.

  @retval EFI_SUCCESS  The data is successfully read or written.
  @retval 
**/
EFI_STATUS
NvdimmBlockIoReadWriteBytes (
  IN NVDIMM_NAMESPACE               *Namespace,
  IN BOOLEAN                        Write,
  IN UINT64                         Offset,
  IN UINTN                          BufferSize,
  IN OUT VOID                       *Buffer
);
EFI_STATUS
ParseNvdimmLabels (
  EFI_HANDLE                  *Handles,
  UINTN                       HandleNum
);
VOID
FreeNamespace (
  NVDIMM_NAMESPACE                 *Namespace
);

/////////////////////////////////////////////////////
/// NFIT functions
/////////////////////////////////////////////////////
EFI_STATUS
ParseNfit (
  VOID
);

EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *
LocateNfitStrucByIndex (
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs,
  UINTN                              NfitStrucCount,
  UINTN                              StructureIndexOffset,
  UINT16                             SructureIndex
);
EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *
LocateNfitStrucByDeviceHandle (
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs,
  UINTN                              NfitStrucCount,
  UINTN                              DeviceHandleOffset,
  EFI_ACPI_6_0_NFIT_DEVICE_HANDLE    *DeviceHandle
);

RETURN_STATUS
DeviceRegionOffsetToSpa (
  UINT64                                                                RegionOffset,
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *Spa,
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map,
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave, OPTIONAL
  UINT8                                                                 **Address
);

VOID
FreeNfitStructs (
  VOID
);




extern PMEM mPmem;
extern CHAR8 *gEfiCallerBaseName;
extern EFI_GUID  gNvdimmControlRegionGuid;
extern EFI_GUID  gNvdimmPersistentMemoryRegionGuid;
extern EFI_GUID  gNvdimmBlockDataWindowRegionGuid;

#endif
