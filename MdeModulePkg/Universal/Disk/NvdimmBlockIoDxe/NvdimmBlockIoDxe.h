/** @file

  Master header file of the driver.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
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
#include <Library/PrintLib.h>

#include "InternalBtt.h"


#define CACHE_LINE_SIZE      64

typedef
VOID *
(EFIAPI *CACHE_LINE_FLUSH) (
  IN VOID *Address
  );

/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU using "clflushopt" instruction.

  Flushed the cache line specified by LinearAddress, and returns LinearAddress.
  This function is only available on IA-32 and x64.

  @param  LinearAddress The address of the cache line to flush. If the CPU is
                        in a physical addressing mode, then LinearAddress is a
                        physical address. If the CPU is in a virtual
                        addressing mode, then LinearAddress is a virtual
                        address.

  @return LinearAddress.
**/
VOID *
EFIAPI
AsmFlushCacheLineOpt (
  IN VOID *LinearAddress
  );

/**
  Call "sfense" instruction to serialize load and store operations.
**/
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

typedef struct _NVDIMM_REGION {
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *Spa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map;
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave;
  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *Control;
} NVDIMM_REGION;

typedef struct {
  NVDIMM                          *Nvdimm;    ///< Point to the NVDIMM the label is in.
  NVDIMM_REGION                   *Region;
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

  NVDIMM_REGION                                  *PmRegion;
  UINTN                                          PmRegionCount;
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
  UINT32                            LbaSize;
  UINT64                            TotalSize;
  UINT64                            RawSize;
  EFI_HANDLE                        Handle;
  EFI_BLOCK_IO_MEDIA                Media;
  EFI_BLOCK_IO_PROTOCOL             BlockIo;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  EFI_UNICODE_STRING_TABLE          *ControllerNameTable;
} NVDIMM_NAMESPACE;
#define NVDIMM_NAMESPACE_SIGNATURE        SIGNATURE_32 ('n', 'd', 'n', 's')
#define NVDIMM_NAMESPACE_FROM_LINK(l)     CR (l, NVDIMM_NAMESPACE, Link,    NVDIMM_NAMESPACE_SIGNATURE)
#define NVDIMM_NAMESPACE_FROM_BLOCK_IO(b) CR (b, NVDIMM_NAMESPACE, BlockIo, NVDIMM_NAMESPACE_SIGNATURE)

/**
  Build the parent-child open relation ship between the namespace blockio and the NVDIMMs.

  @param Namespace  The namespace where the blockio is populated.
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
**/
EFI_STATUS
NvdimmBlockIoReadWriteRawBytes (
  IN NVDIMM_NAMESPACE               *Namespace,
  IN BOOLEAN                        Write,
  IN UINT64                         Offset,
  IN UINTN                          BufferSize,
  IN OUT VOID                       *Buffer
  );

/**
  Enumerate all NVDIMM labels to create(assemble) the namespaces and populate the BlockIo for each namespace.

  @retval EFI_SUCCESS All NVDIMM labels are parsed successfully.
**/
EFI_STATUS
ParseNvdimmLabels (
  VOID
  );

/**
  Load the labels for all NVDIMMs identified by the handles array.

  @param Handles    NVDIMM handles array.
  @param HandleNum  Number of handles in the NVDIMM handles array.

  @retval EFI_SUCCESS All labels are loaded successfully.
**/
EFI_STATUS
LoadAllNvdimmLabels (
  IN EFI_HANDLE                  *Handles,
  IN UINTN                       HandleNum
  );

/**
  Free all resources occupied by a namespace.

  @param Namespace  The namespace to free.
**/
VOID
FreeNamespace (
  NVDIMM_NAMESPACE                 *Namespace
  );

/////////////////////////////////////////////////////
/// NFIT functions
/////////////////////////////////////////////////////
/**
  Parse the NFIT ACPI structures to create all the NVDIMM instances.

  @retval EFI_SUCCESS           The NFIT ACPI structures are valid and all NVDIMM instances are created.
  @retval EFI_NOT_FOUND         There is no NFIT ACPI structure.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource to parse the NFIT ACPI structures.
  @retval EFI_INVALID_PARAMETER The NFIT ACPI structures are invalid.
**/
EFI_STATUS
ParseNfit (
  VOID
  );

/**
  Locate the NFIT structure by the structure index.

  @param NfitStrucs           The NFIT structures array.
  @param NfitStrucCount       Number of the NFIT structures.
  @param StructureIndexOffset The structure index offset.
  @param SructureIndex        Structure index to look for.

  @return NULL or the NFIT structure with the specified structure index.
**/
EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *
LocateNfitStrucByIndex (
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs,
  UINTN                              NfitStrucCount,
  UINTN                              StructureIndexOffset,
  UINT16                             SructureIndex
  );

/**
  Locate the NFIT structure by the device handle.

  @param NfitStrucs           The NFIT structures array.
  @param NfitStrucCount       Number of the NFIT structures.
  @param DeviceHandleOffset   The device handle offset.
  @param DeviceHandle         The device handle.

  @return NULL or the NFIT structure with the specified device handle.
**/
EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER *
LocateNfitStrucByDeviceHandle (
  EFI_ACPI_6_0_NFIT_STRUCTURE_HEADER **NfitStrucs,
  UINTN                              NfitStrucCount,
  UINTN                              DeviceHandleOffset,
  EFI_ACPI_6_0_NFIT_DEVICE_HANDLE    *DeviceHandle
  );

/**
  Convert the region offset to system physical address.

   @param RegionOffset  The region offset.
   @param Spa           The system physical address range structure.
   @param Map           The map structure.
   @param Interleave    The optional interleave structure.
   @param Address       Return the system physical address.

   @retval RETURN_BUFFER_TOO_SMALL The conversion fails.
   @retval RETURN_SUCCESS          The conversion succeeds.
**/
RETURN_STATUS
DeviceRegionOffsetToSpa (
  UINT64                                                                RegionOffset,
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *Spa,
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map,
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave, OPTIONAL
  UINT8                                                                 **Address
  );

/**
  Free the NFIT structures.
**/
VOID
FreeNfitStructs (
  VOID
  );

/**
  Initialize the controller name table for ComponentName(2).

  @param Namespace The NVDIMM Namespace instance.

  @retval EFI_SUCCESS          The controller name table is initialized.
  @retval EFI_OUT_OF_RESOURCES There is not enough memory to initialize the controller name table.
**/
EFI_STATUS
InitializeComponentName (
  IN NVDIMM_NAMESPACE  *Namespace
  );

extern PMEM                         mPmem;
extern CHAR8                        *gEfiCallerBaseName;
extern EFI_GUID                     gNvdimmControlRegionGuid;
extern EFI_GUID                     gNvdimmPersistentMemoryRegionGuid;
extern EFI_GUID                     gNvdimmBlockDataWindowRegionGuid;

extern EFI_DRIVER_BINDING_PROTOCOL  gNvdimmBlockIoDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL  gNvdimmBlockIoComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL gNvdimmBlockIoComponentName2;
extern CACHE_LINE_FLUSH             CacheLineFlush;

#endif
