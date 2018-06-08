#ifndef _NVDIMM_NAMESPACE_BLK_H_
#define _NVDIMM_NAMESPACE_BLK_H_

#include "NvdimmBlockIoDxe.h"

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
    UINT32   UncorrectableError : 1;
    UINT32   ReadMisMatch : 1;
    UINT32   Reserved_3 : 1;
    UINT32   DpaRangeLocked : 1;
    UINT32   BwDisabled : 1;
    UINT32   Reserved_6 : 25;
    UINT32   Pending : 1;
  } Bits;
  UINT32     Uint32;
} BW_STATUS_REGISTER;

typedef struct {
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *ControlSpa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *ControlMap;
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *ControlInterleave;
  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *Control;
  volatile BW_COMMAND_REGISTER                                          *ControlCommand;
  volatile BW_STATUS_REGISTER                                           *ControlStatus;

  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             *DataWindowSpa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *DataWindowMap;
  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *DataWindowInterleave;
  EFI_ACPI_6_0_NFIT_NVDIMM_BLOCK_DATA_WINDOW_REGION_STRUCTURE           *DataWindow;
  UINT32                                                                NumberOfSegments;
  UINT8                                                                 **DataWindowAperture;
} BLK;

EFI_STATUS
InitializeBlkParameters (
  OUT BLK                                                                   *Blk,
  IN  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE *Map,
  IN  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     *Control,
  IN  EFI_ACPI_6_0_NFIT_INTERLEAVE_STRUCTURE                                *Interleave
);

typedef struct _NVDIMM_NAMESPACE NVDIMM_NAMESPACE;

EFI_STATUS
NvdimmBlkReadWriteBytes (
  IN NVDIMM_NAMESPACE               *Namespace,
  IN BOOLEAN                        Write,
  IN UINT64                         Offset,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
);
#endif