#include <Uefi.h>
#include <Protocol/NvdimmLabel.h>
#include <Protocol/DevicePath.h>
#include <IndustryStandard/Acpi.h>
#include <Guid/Btt.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>

UINT64
CalculateFletcher64 (
  UINT32  *Data,
  UINTN   Count
)
{
  UINT32  LoSum;
  UINT32  HiSum;
  UINTN   Index;

  LoSum = 0;
  HiSum = 0;

  for (Index = 0; Index < Count; Index++) {
    LoSum += Data[Index];
    HiSum += LoSum;
  }

  return LShiftU64 (HiSum, 32) | LoSum;
};

typedef struct {
  ///
  /// Signature of the Index Block data structure. Must be "NAMESPACE_INDEX\0".
  ///
  CHAR8  Sig[EFI_NVDIMM_LABEL_INDEX_SIG_LEN];

  ///
  /// Attributes of this Label Storage Area.
  ///
  UINT8  Flags[3];

  ///
  /// Size of each label in bytes, 128 bytes << LabelSize.
  /// 1 means 256 bytes, 2 means 512 bytes, etc. Shall be 1 or greater.
  ///
  UINT8  LabelSize;

  ///
  /// Sequence number used to identify which of the two Index Blocks is current.
  ///
  UINT32 Seq;

  ///
  /// The offset of this Index Block in the Label Storage Area.
  ///
  UINT64 MyOff;

  ///
  /// The size of this Index Block in bytes.
  /// This field must be a multiple of the EFI_NVDIMM_LABEL_INDEX_ALIGN.
  ///
  UINT64 MySize;

  ///
  /// The offset of the other Index Block paired with this one.
  ///
  UINT64 OtherOff;

  ///
  /// The offset of the first slot where labels are stored in this Label Storage Area.
  ///
  UINT64 LabelOff;

  ///
  /// The total number of slots for storing labels in this Label Storage Area.
  ///
  UINT32 NSlot;

  ///
  /// Major version number. Value shall be 1.
  ///
  UINT16 Major;

  ///
  /// Minor version number. Value shall be 2.
  ///
  UINT16 Minor;

  ///
  /// 64-bit Fletcher64 checksum of all fields in this Index Block.
  ///
  UINT64 Checksum;

  ///
  /// Array of unsigned bytes implementing a bitmask that tracks which label slots are free.
  /// A bit value of 0 indicates in use, 1 indicates free.
  /// The size of this field is the number of bytes required to hold the bitmask with NSlot bits,
  /// padded with additional zero bytes to make the Index Block size a multiple of EFI_NVDIMM_LABEL_INDEX_ALIGN.
  /// Any bits allocated beyond NSlot bits must be zero.
  ///
  UINT8  Free[256-72];
} NVDIMM_LABEL_INDEX_BLOCK;

VERIFY_SIZE_OF (NVDIMM_LABEL_INDEX_BLOCK, 256);

typedef struct {
  NVDIMM_LABEL_INDEX_BLOCK  LabelIndexBlock[2];
  EFI_NVDIMM_LABEL          Label[1];
} NVDIMM_LABEL_STORAGE;



#pragma pack(1)
typedef struct {
  ACPI_ADR_DEVICE_PATH     DeviceHandle;
  EFI_DEVICE_PATH_PROTOCOL End;
} NVDIMM_LABEL_DEVICE_PATH;
#pragma pack()

NVDIMM_LABEL_DEVICE_PATH mLabelDevicePathTemplate = {
  { // DeviceHandle
    { ACPI_DEVICE_PATH, ACPI_ADR_DP,{ sizeof (ACPI_ADR_DEVICE_PATH), 0 } },
    0x12345678
  },
  { // End
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,{ sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
  }
};

typedef struct {
  NVDIMM_LABEL_STORAGE      LabelStorage;
  EFI_NVDIMM_LABEL_PROTOCOL LabelProtocol;
  NVDIMM_LABEL_DEVICE_PATH  LabelDevicePath;
} NVDIMM_INSTANCE;

#define NVDIMM_INSTANCE_FROM_THIS(x) BASE_CR(x, NVDIMM_INSTANCE, LabelProtocol)

NVDIMM_LABEL_STORAGE  mLabelStorageTemplate = {
  {
    {

      "NAMESPACE_INDEX",
      { 0 },
      1, // Label Size
      1, // Seq;
      0, // my offset
      EFI_NVDIMM_LABEL_INDEX_ALIGN, // size of index block
      EFI_NVDIMM_LABEL_INDEX_ALIGN, // other offset
      EFI_NVDIMM_LABEL_INDEX_ALIGN * 2, // label offset
      8, // slot number
      1, // major version
      2, // minor version
      0, // check sum
      { 0xFE } // Free[]
    },
    {
      "NAMESPACE_INDEX",
      { 0 },
      1, // Label Size
      2, // Seq;
      EFI_NVDIMM_LABEL_INDEX_ALIGN, // my offset
      EFI_NVDIMM_LABEL_INDEX_ALIGN, // size of index block
      0, // other offset
      EFI_NVDIMM_LABEL_INDEX_ALIGN * 2, // label offset
      8, // slot number
      1, // major version
      2, // minor version
      0, // check sum
      { 0xFE } // Free[]
    }
  },
  {
    { // Label[0]
      { 0x8b931bba, 0xf760, 0x47c5,{ 0xa7, 0x99, 0xf1, 0xb7, 0x36, 0x29, 0xb, 0x36 } }, // Label UUID
      { "Label #1" },
      0, // Flags
      2, // NLabel
      0, // Position (TBF)
      0, // SetCookie
      0, // Lba size
      0, // DPA
      SIZE_32MB, // RAW size
      0, // Slot

      16, // Alignment
      { 0 }, // Reserved[3]
      EFI_ACPI_6_0_NFIT_GUID_BYTE_ADDRESSABLE_PERSISTENT_MEMORY_REGION, // Type GUID
      EFI_BTT_ABSTRACTION_GUID, // address abstraction mechanism
      { 0 }, // Reserved1[88];
      0 // Checksum
    }
  }
};

/**
  Retrieves the Label Storage Area size and the maximum transfer size for the LabelStorageRead and
  LabelStorageWrite methods.

  @param  This                   A pointer to the EFI_NVDIMM_LABEL_PROTOCOL instance.
  @param  SizeOfLabelStorageArea The size of the Label Storage Area for the NVDIMM in bytes.
  @param  MaxTransferLength      The maximum number of bytes that can be transferred in a single call to
                                 LabelStorageRead or LabelStorageWrite.

  @retval EFI_SUCCESS            The size of theLabel Storage Area and maximum transfer size returned are valid.
  @retval EFI_ACCESS_DENIED      The Label Storage Area for the NVDIMM device is not currently accessible.
  @retval EFI_DEVICE_ERROR       A physical device error occurred and the data transfer failed to complete.
**/
EFI_STATUS
EFIAPI
LabelStorageInformation (
  IN  EFI_NVDIMM_LABEL_PROTOCOL *This,
  OUT UINT32                    *SizeOfLabelStorageArea,
  OUT UINT32                    *MaxTransferLength
)
{
  NVDIMM_INSTANCE               *Instance;
  Instance = NVDIMM_INSTANCE_FROM_THIS (This);
  *SizeOfLabelStorageArea = sizeof (Instance->LabelStorage);
  *MaxTransferLength = 23;
  return EFI_SUCCESS;
}


/**
  Retrieves the label data for the requested offset and length from within the Label Storage Area for
  the NVDIMM.

  @param  This                   A pointer to the EFI_NVDIMM_LABEL_PROTOCOL instance.
  @param  Offset                 The byte offset within the Label Storage Area to read from.
  @param  TransferLength         Number of bytes to read from the Label Storage Area beginning at the byte
                                 Offset specified. A TransferLength of 0 reads no data.
  @param  LabelData              The return label data read at the requested offset and length from within
                                 the Label Storage Area.

  @retval EFI_SUCCESS            The label data from the Label Storage Area for the NVDIMM was read successfully
                                 at the specified Offset and TransferLength and LabelData contains valid data.
  @retval EFI_INVALID_PARAMETER  Any of the following are true:
                                 - Offset > SizeOfLabelStorageArea reported in the LabelStorageInformation return data.
                                 - Offset + TransferLength is > SizeOfLabelStorageArea reported in the
                                   LabelStorageInformation return data.
                                 - TransferLength is > MaxTransferLength reported in the LabelStorageInformation return
                                   data.
  @retval EFI_ACCESS_DENIED      The Label Storage Area for the NVDIMM device is not currently accessible and labels
                                 cannot be read at this time.
  @retval EFI_DEVICE_ERROR       A physical device error occurred and the data transfer failed to complete.
**/
EFI_STATUS
EFIAPI
LabelStorageRead (
  IN CONST EFI_NVDIMM_LABEL_PROTOCOL *This,
  IN UINT32                          Offset,
  IN UINT32                          TransferLength,
  OUT UINT8                          *LabelData
)
{
  NVDIMM_INSTANCE               *Instance;
  Instance = NVDIMM_INSTANCE_FROM_THIS (This);
  if (Offset > sizeof (Instance->LabelStorage) || Offset + TransferLength > sizeof (Instance->LabelStorage)) {
    return EFI_INVALID_PARAMETER;
  }
  if (LabelData == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  CopyMem (LabelData, (UINT8 *)&Instance->LabelStorage + Offset, TransferLength);
  return EFI_SUCCESS;
}

/**
  Writes the label data for the requested offset and length in to the Label Storage Area for the NVDIMM.

  @param  This                   A pointer to the EFI_NVDIMM_LABEL_PROTOCOL instance.
  @param  Offset                 The byte offset within the Label Storage Area to write to.
  @param  TransferLength         Number of bytes to write to the Label Storage Area beginning at the byte
                                 Offset specified. A TransferLength of 0 writes no data.
  @param  LabelData              The return label data write at the requested offset and length from within
                                 the Label Storage Area.

  @retval EFI_SUCCESS            The label data from the Label Storage Area for the NVDIMM written read successfully
                                 at the specified Offset and TransferLength.
  @retval EFI_INVALID_PARAMETER  Any of the following are true:
                                 - Offset > SizeOfLabelStorageArea reported in the LabelStorageInformation return data.
                                 - Offset + TransferLength is > SizeOfLabelStorageArea reported in the
                                   LabelStorageInformation return data.
                                 - TransferLength is > MaxTransferLength reported in the LabelStorageInformation return
                                   data.
  @retval EFI_ACCESS_DENIED      The Label Storage Area for the NVDIMM device is not currently accessible and labels
                                 cannot be written at this time.
  @retval EFI_DEVICE_ERROR       A physical device error occurred and the data transfer failed to complete.
**/
EFI_STATUS
EFIAPI
LabelStorageWrite (
  IN CONST EFI_NVDIMM_LABEL_PROTOCOL *This,
  IN UINT32                          Offset,
  IN UINT32                          TransferLength,
  IN UINT8                           *LabelData
)
{
  return EFI_DEVICE_ERROR;
}

///
/// Provides services that allow management of labels contained in a Label Storage Area.
///
EFI_NVDIMM_LABEL_PROTOCOL mLabelProtocolTemplate = {
  LabelStorageInformation,
  LabelStorageRead,
  LabelStorageWrite
};

VOID InstallNvdimmNfit ();

NVDIMM_INSTANCE mNvdimm[2];

EFI_STATUS
EFIAPI
NfitTestConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  EFI_STATUS   Status;
  EFI_HANDLE   Handle;
  UINTN        Index;

  mLabelDevicePathTemplate.DeviceHandle.ADR = 0x12345678;
  mLabelStorageTemplate.LabelIndexBlock[0].Checksum = CalculateFletcher64 (
    (UINT32 *)&mLabelStorageTemplate.LabelIndexBlock[0], sizeof (mLabelStorageTemplate.LabelIndexBlock[0]) / sizeof (UINT32)
  );
  mLabelStorageTemplate.LabelIndexBlock[1].Checksum = CalculateFletcher64 (
    (UINT32 *)&mLabelStorageTemplate.LabelIndexBlock[1], sizeof (mLabelStorageTemplate.LabelIndexBlock[1]) / sizeof (UINT32)
  );
  for (Index = 0; Index < ARRAY_SIZE (mNvdimm); Index++) {
    CopyMem (&mNvdimm[Index].LabelDevicePath, &mLabelDevicePathTemplate, sizeof (mLabelDevicePathTemplate));
    CopyMem (&mNvdimm[Index].LabelProtocol, &mLabelProtocolTemplate, sizeof (mLabelProtocolTemplate));
    CopyMem (&mNvdimm[Index].LabelStorage, &mLabelStorageTemplate, sizeof (mLabelStorageTemplate));
  }
  mNvdimm[0].LabelDevicePath.DeviceHandle.ADR = 0x12345678;
  mNvdimm[1].LabelDevicePath.DeviceHandle.ADR = 0x22345678;

  mNvdimm[0].LabelStorage.Label[0].Position = 0;
  mNvdimm[0].LabelStorage.Label[0].Checksum = CalculateFletcher64 (
    (UINT32 *)&mNvdimm[0].LabelStorage.Label[0], sizeof (mNvdimm[0].LabelStorage.Label[0]) / sizeof (UINT32)
  );

  mNvdimm[1].LabelStorage.Label[0].Position = 1;
  mNvdimm[1].LabelStorage.Label[0].Checksum = CalculateFletcher64 (
    (UINT32 *)&mNvdimm[1].LabelStorage.Label[0], sizeof (mNvdimm[1].LabelStorage.Label[0]) / sizeof (UINT32)
  );

  for (Index = 0; Index < ARRAY_SIZE (mNvdimm); Index++) {
    Handle = NULL;
    Status = gBS->InstallMultipleProtocolInterfaces (
      &Handle,
      &gEfiDevicePathProtocolGuid, &mNvdimm[Index].LabelDevicePath,
      &gEfiNvdimmLabelProtocolGuid, &mNvdimm[Index].LabelProtocol,
      NULL
    );
    ASSERT_EFI_ERROR (Status);
  }

  InstallNvdimmNfit ();
  CpuBreakpoint ();
  return EFI_SUCCESS;
}