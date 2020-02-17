/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PciBus.h"
#include "PcieFeatureSupport.h"


UINT8
MaxPayloadSizePcieToEfi (
  IN UINT8              PcieMaxPayloadSize
  )
{
  switch (PcieMaxPayloadSize) {
    case PCIE_MAX_PAYLOAD_SIZE_128B:
      return EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_128B;
    case PCIE_MAX_PAYLOAD_SIZE_256B:
      return EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_256B;
    case PCIE_MAX_PAYLOAD_SIZE_512B:
      return EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_512B;
    case PCIE_MAX_PAYLOAD_SIZE_1024B:
      return EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_1024B;
    case PCIE_MAX_PAYLOAD_SIZE_2048B:
      return EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_2048B;
    case PCIE_MAX_PAYLOAD_SIZE_4096B:
      return EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_4096B;
    default:
      ASSERT (FALSE);
      return EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE;
  }
}

/**
  Routine to translate the given device-specific platform policy from type
  EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE to HW-specific value, as per PCI Base Specification
  Revision 4.0; for the PCI feature Max_Payload_Size.

  @param  MPS     Input device-specific policy should be in terms of type
                  EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE

  @retval         Range values for the Max_Payload_Size as defined in the PCI
                  Base Specification 4.0
**/
UINT8
MaxPayloadSizeEfiToPcie (
  IN  UINT8                   EfiMaxPayloadSize
)
{
  switch (EfiMaxPayloadSize) {
    case EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_128B:
      return PCIE_MAX_PAYLOAD_SIZE_128B;
    case EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_256B:
      return PCIE_MAX_PAYLOAD_SIZE_256B;
    case EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_512B:
      return PCIE_MAX_PAYLOAD_SIZE_512B;
    case EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_1024B:
      return PCIE_MAX_PAYLOAD_SIZE_1024B;
    case EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_2048B:
      return PCIE_MAX_PAYLOAD_SIZE_2048B;
    case EFI_PCI_EXPRESS_MAX_PAYLOAD_SIZE_4096B:
      return PCIE_MAX_PAYLOAD_SIZE_4096B;
    default:
      return PCIE_MAX_PAYLOAD_SIZE_128B;
  }
}

EFI_STATUS
MaxPayloadScan (
  IN PCI_IO_DEVICE *PciDevice,
  IN VOID          **Context
  )
{
  UINT8                           *MaxPayloadSize;
  PCI_REG_PCIE_DEVICE_CAPABILITY  DeviceCapability;

  DeviceCapability.Uint32 = PciDevice->PciExpressCapabilityStructure.DeviceCapability.Uint32;
  if (PciDevice->DeviceState.MaxPayloadSize == EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) {
    //
    // no change to PCI Root ports without any endpoint device
    //
    if (IS_PCI_BRIDGE (&PciDevice->Pci) && IsListEmpty  (&PciDevice->ChildList)) {
        //
        // No device on root bridge
        //
        DeviceCapability.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_128B;
      }
  } else {
    DeviceCapability.Bits.MaxPayloadSize = MIN (
      MaxPayloadSizeEfiToPcie (PciDevice->DeviceState.MaxPayloadSize), DeviceCapability.Bits.MaxPayloadSize
      );
  }

  MaxPayloadSize = *Context;
  if (MaxPayloadSize == NULL) {
    //
    // Initialize the Context
    //
    MaxPayloadSize = AllocatePool (sizeof (*MaxPayloadSize));
    *MaxPayloadSize = (UINT8) DeviceCapability.Bits.MaxPayloadSize;
    *Context = MaxPayloadSize;
  } else {
    //
    // Set the Context to the minimum Max Payload Size in the heirarchy.
    //
    *MaxPayloadSize = MIN (*MaxPayloadSize, (UINT8) DeviceCapability.Bits.MaxPayloadSize);
  }
  return EFI_SUCCESS;
}

EFI_STATUS
MaxPayloadProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN VOID          **Context
  )
{
  PCI_REG_PCIE_DEVICE_CONTROL DeviceControl;
  UINT32                      Offset;
  EFI_STATUS                  Status;
  UINT8                       *MaxPayloadSize;

  ASSERT (Context != NULL);
  ASSERT (*Context != NULL);

  MaxPayloadSize = (UINT8 *) *Context;
  PciDevice->DeviceState.MaxPayloadSize = MaxPayloadSizePcieToEfi (*MaxPayloadSize);

  Offset = PciDevice->PciExpressCapabilityOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl);
  Status = PciDevice->PciIo.Pci.Read (
                                  &PciDevice->PciIo,
                                  EfiPciIoWidthUint16,
                                  Offset,
                                  sizeof (DeviceControl),
                                  &DeviceControl.Uint16
                                  );
  if (!EFI_ERROR (Status)) {
    if (DeviceControl.Bits.MaxPayloadSize != *MaxPayloadSize) {
      DeviceControl.Bits.MaxPayloadSize = *MaxPayloadSize;
      DEBUG (( DEBUG_INFO, "MPS=%d,", *MaxPayloadSize));

      Status = PciDevice->PciIo.Pci.Write (
                                      &PciDevice->PciIo,
                                      EfiPciIoWidthUint16,
                                      Offset,
                                      1,
                                      &DeviceControl.Uint16
                                      );
    }
  }

  return Status;
}
