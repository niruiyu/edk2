/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PciBus.h"
#include "PcieFeatureSupport.h"

EFI_STATUS
MaxPayloadSizeScan (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  )
{
  UINT8                           *MaxPayloadSize;
  PCI_REG_PCIE_DEVICE_CAPABILITY  DeviceCapability;

  ///// TEST Begin
  {
    if (PciDevice->BusNumber == 0 && PciDevice->DeviceNumber == 4) {
      PciDevice->PciExpressCapability.DeviceCapability.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_4096B;
    }

    if (PciDevice->BusNumber == 2) {
      if (PciDevice->FunctionNumber == 0) {
        PciDevice->PciExpressCapability.DeviceCapability.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_2048B;
      }
      if (PciDevice->FunctionNumber == 1) {
        PciDevice->PciExpressCapability.DeviceCapability.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_4096B;
      }
    }

    if (PciDevice->BusNumber == 3) {
      PciDevice->PciExpressCapability.DeviceCapability.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_1024B;
    }

    if (PciDevice->BusNumber == 4) {
      PciDevice->PciExpressCapability.DeviceCapability.Bits.MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_2048B;
    }
  }
  ///// TEST End

  DEBUG ((
    DEBUG_INFO, "  %a [%02d|%02d|%02d]: Capability = %x\n",
    __FUNCTION__, PciDevice->BusNumber, PciDevice->DeviceNumber, PciDevice->FunctionNumber,
    PciDevice->PciExpressCapability.DeviceCapability.Bits.MaxPayloadSize
    ));
  DeviceCapability.Uint32 = PciDevice->PciExpressCapability.DeviceCapability.Uint32;

  if ((PciDevice->DeviceState.MaxPayloadSize != EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) &&
      (PciDevice->DeviceState.MaxPayloadSize != EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE)) {
    DeviceCapability.Bits.MaxPayloadSize =
      MIN (PciDevice->DeviceState.MaxPayloadSize, DeviceCapability.Bits.MaxPayloadSize);
  }

  MaxPayloadSize = *Context;
  if (MaxPayloadSize == NULL) {
    //
    // Initialize the Context
    //
    MaxPayloadSize  = AllocatePool (sizeof (*MaxPayloadSize));
    *MaxPayloadSize = (UINT8) DeviceCapability.Bits.MaxPayloadSize;
    *Context        = MaxPayloadSize;
  } else {
    //
    // Set the Context to the minimum Max Payload Size in the heirarchy.
    //
    *MaxPayloadSize = MIN (*MaxPayloadSize, (UINT8) DeviceCapability.Bits.MaxPayloadSize);
  }
  return EFI_SUCCESS;
}

EFI_STATUS
MaxPayloadSizeProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  )
{
  UINT8 *MaxPayloadSize;

  ASSERT (Context != NULL);
  ASSERT (*Context != NULL);

  if (PciDevice->DeviceState.MaxPayloadSize == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE) {
    //
    // NOT_APPLICABLE means platform requests PciBus doesn't change the setting.
    // But the capability of this device is still honored when calculating the aligned value.
    //
    return EFI_SUCCESS;
  }

  MaxPayloadSize                        = (UINT8 *) *Context;
  PciDevice->DeviceState.MaxPayloadSize = *MaxPayloadSize;

  if (PciDevice->PciExpressCapability.DeviceControl.Bits.MaxPayloadSize != PciDevice->DeviceState.MaxPayloadSize) {
    DEBUG ((
      DEBUG_INFO, "  %a [%02d|%02d|%02d]: %x -> %x\n",
      __FUNCTION__, PciDevice->BusNumber, PciDevice->DeviceNumber, PciDevice->FunctionNumber,
      PciDevice->PciExpressCapability.DeviceControl.Bits.MaxPayloadSize,
      PciDevice->DeviceState.MaxPayloadSize
      ));
    PciDevice->PciExpressCapability.DeviceControl.Bits.MaxPayloadSize = PciDevice->DeviceState.MaxPayloadSize;

    return PciDevice->PciIo.Pci.Write (
                                  &PciDevice->PciIo,
                                  EfiPciIoWidthUint16,
                                  PciDevice->PciExpressCapabilityOffset
                                  + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl),
                                  1,
                                  &PciDevice->PciExpressCapability.DeviceControl.Uint16
                                  );
  }
  return EFI_SUCCESS;
}

