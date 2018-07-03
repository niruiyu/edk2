#include <Uefi.h>
#include <IndustryStandard/Acpi.h>
#include <Guid/Acpi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

typedef struct {
  EFI_ACPI_6_0_NVDIMM_FIRMWARE_INTERFACE_TABLE                          Header;
  EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE             Spa;
  EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE Map;
  EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE                     Control;
} NVDIMM_NFIT;

NVDIMM_NFIT mNvdimmNfit = {
  { // Header
    { // Header
      SIGNATURE_32 ('N', 'F', 'I', 'T'),
      sizeof (NVDIMM_NFIT),
      1,
      0, // Checksum, TBF
      0, 0, 0, // OEMID, OEM Table ID, OEM Revision
      0, 0 // Creator ID, Revision
    },
    0 // Reserved
  },
  { // Spa
    EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE_TYPE, // Type
    sizeof (EFI_ACPI_6_0_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE), // Length
    1, // Index
    0, // Flags
    0, // Reserved
    0, // Proximity DOmain
    EFI_ACPI_6_0_NFIT_GUID_BYTE_ADDRESSABLE_PERSISTENT_MEMORY_REGION, // Address Range Type GUID
    0, SIZE_16MB, // Base, Length, TBF
    EFI_MEMORY_WB
  },
  { // Map
    EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE,
    sizeof (EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE),
    {0x8, 0x7, 0x6, 0x5, 0x234, 0x1},// Device Handle
    0, // Physical ID
    0, // Region ID
    1, // SPA Index
    1, // Control Index
    SIZE_16MB, // Region Size
    0, // Region Offset
    0, // Physical Region Base
    0, // Interleave Index
    1 // Interleave ways
  },
  { // Control
    EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE_TYPE,
    sizeof (EFI_ACPI_6_0_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE),
    1, // Index
    0x8086, 0x1234, // Vendor ID, Device ID
    0, // Revision ID
    0x8088, 0x8888, // Subsystem Vendor ID, Device ID
    111, // Subsystem Revision ID
    1,    8,    0x3434, // Valid Fields, Location, Date
    { 0 }, // Reserved[2],
    0x76541234, // Serial Number
    0, // Region Format Interface Code
    0, // Number of Block Control Windows
    0, // Size of Block Control Window
    0, 0, 0, 0, // Command Register Offset/Size, Status Register Offset/Size
    0, // Flag
    0, // Reserved
  }
};

#pragma pack(1)
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER Header;
  UINT64                      Pointer[1];
} RSDT;
#pragma pack()

RSDT mRsdt = {
  { 0, sizeof (RSDT)},
  (UINT64)(UINTN)(VOID *)&mNvdimmNfit
};

EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER mRsdp = {
  0, 0, { 0 },
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION, // Revision
  0, // Rsdt
  sizeof (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER),
  (UINT64) (UINTN) (VOID *) &mRsdt,
  0,
  {0}
};

VOID
InstallNvdimmNfit ()
{
  EFI_STATUS Status;
  Status = gBS->InstallConfigurationTable (&gEfiAcpiTableGuid, &mRsdp);
  ASSERT_EFI_ERROR (Status);
}