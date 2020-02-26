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


EFI_STATUS
MaxReadRequestSizeProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  )
{
  ASSERT (*Context == NULL);

  if (PciDevice->DeviceState.MaxReadRequestSize == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE) {
    return EFI_SUCCESS;
  }
  if (PciDevice->DeviceState.MaxReadRequestSize == EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) {
    PciDevice->DeviceState.MaxReadRequestSize = (UINT8) PciDevice->PciExpressCapability.DeviceControl.Bits.MaxPayloadSize;
  }

  if (PciDevice->PciExpressCapability.DeviceControl.Bits.MaxReadRequestSize != PciDevice->DeviceState.MaxReadRequestSize) {
    DEBUG ((
      DEBUG_INFO, "  %a [%02d|%02d|%02d]: %x -> %x\n",
      __FUNCTION__, PciDevice->BusNumber, PciDevice->DeviceNumber, PciDevice->FunctionNumber,
      PciDevice->PciExpressCapability.DeviceControl.Bits.MaxReadRequestSize,
      PciDevice->DeviceState.MaxReadRequestSize
      ));
    PciDevice->PciExpressCapability.DeviceControl.Bits.MaxReadRequestSize = PciDevice->DeviceState.MaxReadRequestSize;

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


/**
  Program the PCIE Device Control register Relaxed Ordering field per platform policy.

  @param  PciDevice             A pointer to the PCI_IO_DEVICE instance.

  @retval EFI_SUCCESS           The data was read from or written to the PCI device.
  @retval EFI_UNSUPPORTED       The address range specified by Offset, Width, and Count is not
                                valid for the PCI configuration header of the PCI controller.
  @retval EFI_INVALID_PARAMETER Buffer is NULL or Width is invalid.
**/
EFI_STATUS
RelaxedOrderingProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  )
{
  ASSERT (*Context == NULL);
  
  if (PciDevice->DeviceState.RelaxedOrdering == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE ||
      PciDevice->DeviceState.RelaxedOrdering == EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) {
    return EFI_SUCCESS;
  }
  
  if (PciDevice->PciExpressCapability.DeviceControl.Bits.RelaxedOrdering != PciDevice->DeviceState.RelaxedOrdering) {
    DEBUG ((
      DEBUG_INFO, "  %a [%02d|%02d|%02d]: %x -> %x\n",
      __FUNCTION__, PciDevice->BusNumber, PciDevice->DeviceNumber, PciDevice->FunctionNumber,
      PciDevice->PciExpressCapability.DeviceControl.Bits.RelaxedOrdering,
      PciDevice->DeviceState.RelaxedOrdering
      ));
    PciDevice->PciExpressCapability.DeviceControl.Bits.RelaxedOrdering = PciDevice->DeviceState.RelaxedOrdering;

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


/**
  Overrides the PCI Device Control register No-Snoop register field; if
  the hardware value is different than the intended value.

  @param  PciDevice             A pointer to the PCI_IO_DEVICE instance.

  @retval EFI_SUCCESS           The data was read from or written to the PCI device.
  @retval EFI_UNSUPPORTED       The address range specified by Offset, Width, and Count is not
                                valid for the PCI configuration header of the PCI controller.
  @retval EFI_INVALID_PARAMETER Buffer is NULL or Width is invalid.

**/
EFI_STATUS
NoSnoopProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  )
{
  ASSERT (*Context == NULL);
  
  if (PciDevice->DeviceState.NoSnoop == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE ||
      PciDevice->DeviceState.NoSnoop == EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) {
    return EFI_SUCCESS;
  }

  
  if (PciDevice->PciExpressCapability.DeviceControl.Bits.NoSnoop != PciDevice->DeviceState.NoSnoop) {
    DEBUG ((
      DEBUG_INFO, "  %a [%02d|%02d|%02d]: %x -> %x\n",
      __FUNCTION__, PciDevice->BusNumber, PciDevice->DeviceNumber, PciDevice->FunctionNumber,
      PciDevice->PciExpressCapability.DeviceControl.Bits.NoSnoop,
      PciDevice->DeviceState.NoSnoop
      ));
    PciDevice->PciExpressCapability.DeviceControl.Bits.NoSnoop = PciDevice->DeviceState.NoSnoop;

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

/**
  Program PCIE feature Completion Timeout per the device-specific platform policy.

  @param PciIoDevice      A pointer to the PCI_IO_DEVICE.

  @retval EFI_SUCCESS           The feature is initialized successfully.
  @retval EFI_INVALID_PARAMETER The policy is not supported by the device.
**/
EFI_STATUS
CompletionTimeoutProgram (
  IN PCI_IO_DEVICE *PciIoDevice,
  IN UINTN         Level,
  IN VOID          **Context
  )
{
  PCI_REG_PCIE_DEVICE_CONTROL2    DevicePolicy;
  UINTN                           RangeIndex;
  UINT8                           SubRanges;

  if (PciIoDevice->DeviceState.CompletionTimeout == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE ||
      PciIoDevice->DeviceState.CompletionTimeout == EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) {
    return EFI_SUCCESS;
  }

  //
  // Interpret the policy value as BIT[0:4] in Device Control 2 Register
  //
  DevicePolicy.Uint16 = (UINT16) PciIoDevice->DeviceState.CompletionTimeout;

  //
  // Ignore when device doesn't support to disable Completion Timeout while the policy requests.
  //
  if (PciIoDevice->PciExpressCapability.DeviceCapability2.Bits.CompletionTimeoutDisable == 0 &&
      DevicePolicy.Bits.CompletionTimeoutDisable == 1) {
    return EFI_INVALID_PARAMETER;
  }

  if (DevicePolicy.Bits.CompletionTimeoutValue != 0) {
    //
    // Ignore when the policy requests to use a range that's not supported by the device.
    // RangeIndex is 0 ~ 3 for Range A ~ D.
    //
    RangeIndex = DevicePolicy.Bits.CompletionTimeoutValue >> 2;
    if ((PciIoDevice->PciExpressCapability.DeviceCapability2.Bits.CompletionTimeoutRanges & (1 < RangeIndex)) == 0) {
      return EFI_INVALID_PARAMETER;
    }

    //
    // Ignore when the policy doesn't request one and only one sub-range for a certain range.
    //
    SubRanges = (UINT8) (DevicePolicy.Bits.CompletionTimeoutValue & (BIT0 | BIT1));
    if (SubRanges != BIT0 && SubRanges != BIT1) {
      return EFI_INVALID_PARAMETER;
    }
  }

  if ((PciIoDevice->PciExpressCapability.DeviceControl2.Bits.CompletionTimeoutDisable
       != DevicePolicy.Bits.CompletionTimeoutDisable) ||
      (PciIoDevice->PciExpressCapability.DeviceControl2.Bits.CompletionTimeoutValue
       != DevicePolicy.Bits.CompletionTimeoutValue)) {
    DEBUG ((
      DEBUG_INFO, "  %a [%02d|%02d|%02d]: Disable = %x -> %x, Timeout = %x -> %x.\n",
      __FUNCTION__, PciIoDevice->BusNumber, PciIoDevice->DeviceNumber, PciIoDevice->FunctionNumber,
      PciIoDevice->PciExpressCapability.DeviceControl2.Bits.CompletionTimeoutDisable,
      DevicePolicy.Bits.CompletionTimeoutDisable,
      PciIoDevice->PciExpressCapability.DeviceControl2.Bits.CompletionTimeoutValue,
      DevicePolicy.Bits.CompletionTimeoutValue
      ));
    PciIoDevice->PciExpressCapability.DeviceControl2.Bits.CompletionTimeoutDisable
                      = DevicePolicy.Bits.CompletionTimeoutDisable;
    PciIoDevice->PciExpressCapability.DeviceControl2.Bits.CompletionTimeoutValue
                      = DevicePolicy.Bits.CompletionTimeoutValue;

    return PciIoDevice->PciIo.Pci.Write (
                                  &PciIoDevice->PciIo,
                                  EfiPciIoWidthUint16,
                                  PciIoDevice->PciExpressCapabilityOffset
                                  + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl2),
                                  1,
                                  &PciIoDevice->PciExpressCapability.DeviceControl2.Uint16
                                  );
  }

  return EFI_SUCCESS;
}
