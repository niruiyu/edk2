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


STATIC
VOID
LtrOr (
  BOOLEAN   *Result,
  BOOLEAN   Ltr
)
{
  ASSERT (Result != NULL);
  ASSERT (*Result == 0xFF || *Result == TRUE || *Result == FALSE);
  if (*Result == 0xFF) {
    //
    // Initialize Result when meeting the first device in the Level.
    //
    *Result = Ltr;
  } else {
    //
    // Save the "OR" result of LTR of all devices in this Level.
    //
    *Result = (*Result || Ltr);
  }
}

/**
  Scan the devices to finalize the LTR settings of each device.

  The scan needs to be done in post-order.

  @param PciIoDevice  A pointer to the PCI_IO_DEVICE.
  @param Context      Pointer to feature specific context.

  @retval EFI_SUCCESS setup of PCI feature LTR is successful.
**/
EFI_STATUS
LtrScan (
  IN  PCI_IO_DEVICE *PciIoDevice,
  IN  UINTN         Level,
  IN  VOID          **Context
  )
{
  BOOLEAN           *Ltr;
  ASSERT (Level <= PCI_MAX_BUS);
  ASSERT (Context != NULL);

  // LTR of a parent (certain bridge) in level N is enabled when any child in
  // level N + 1 enables LTR.
  //
  // Because the devices are enumerated in post-order (children-then-parent),
  // we could allocate one BOOLEAN in Context to save the "OR" result of LTR
  // enable status of all children in a certain level. LTR of parent is set
  // when the "OR" result is TRUE.
  //
  // Because the max level cannot exceed the max PCI bus number 256, allocating
  // a BOOLEAN array of 256 elements should be enough.
  //
  Ltr = (BOOLEAN *) (*Context);
  if (Ltr == NULL) {
    Ltr = AllocatePool (sizeof (BOOLEAN) * (PCI_MAX_BUS + 1));
    SetMem (Ltr, sizeof (BOOLEAN) * (PCI_MAX_BUS + 1), 0xFF);
    *Context = Ltr;
  }
  /// UNIT TEST
  {
    /*
For below device heirarchy with LTR enabled in 5/0/0 and 11/0/0, disabled in 9/0/0
The LTR setting for upstream bridges should be as following.
                    0/4/0[1]
  2/0/0[1]  |       2/1/0[1]         | 2/2/0[0]
  3/0/0[1]  |       7/0/0[1]
  4/0/0[1]  |       8/0/0[1]
  5/0/0[1]  | 9/0/0[0]   9/1/0[1]
                         10/0/0[1]
                         11/0/0[1]
               
1. CMD line to launch QEMU
setlocal
SET RP1=-device pcie-root-port,id=root_port1,chassis=1 -device qemu-xhci,bus=root_port1
SET RP2=-device pcie-root-port,id=rp2,chassis=2
@REM Level 2
SET RP2_2=-device pcie-root-port,id=rp2.1,bus=rp2 -device pcie-root-port,id=rp2.2,bus=rp2,chassis=3 -device qemu-xhci,bus=rp2
@REM Level 3
SET RP2_3=-device pcie-root-port,id=rp2.1.1,bus=rp2.1,chassis=4 -device pcie-root-port,id=rp2.2.1,bus=rp2.2,chassis=5
SET RP2_4=-device pcie-root-port,id=rp2.1.1.1,bus=rp2.1.1,chassis=6 -device pcie-root-port,id=rp2.2.1.1,bus=rp2.2.1,chassis=7
SET RP2_5=-device pcie-root-port,id=rp2.1.1.1.1,bus=rp2.1.1.1,chassis=8 -device qemu-xhci,bus=rp2.2.1.1 -device pcie-root-port,id=rp2.2.1.1.2,bus=rp2.2.1.1,chassis=9
SET RP2_6=-device pcie-root-port,id=rp2.2.1.1.2.1,bus=rp2.2.1.1.2,chassis=10
SET RP2_7=-device qemu-xhci,bus=rp2.2.1.1.2.1

qemu-system-x86_64.exe -machine pc-q35-2.8 -drive if=pflash,format=raw,unit=0,file=OVMF_CODE.fd,readonly=on -drive if=pflash,format=raw,unit=1,file=OVMF_VARS.fd -serial COM6 %RP1% %RP2% %RP2_2% %RP2_3% %RP2_4% %RP2_5% %RP2_6% %RP2_7% 

2. Platform device policy for LTR
  switch (PciAddress.Bus) {
    case 5:
    case 11:
    PciePolicy->Ltr = 1;
    break;

    case 9:
    if (PciAddress.Device == 0) {
      PciePolicy->Ltr = 0;
    }
    break;
  }

3. Below code to claim every device supports LTR.
    */
    PciIoDevice->PciExpressCapability.DeviceCapability2.Bits.LtrMechanism = 1;
  }
  /// END of UNIT TEST

  DEBUG ((
    DEBUG_INFO, "  %a [%02d|%02d|%02d]: Capability = %x.\n",
    __FUNCTION__, PciIoDevice->BusNumber, PciIoDevice->DeviceNumber, PciIoDevice->FunctionNumber,
    PciIoDevice->PciExpressCapability.DeviceCapability2.Bits.LtrMechanism
    ));
  //
  // Disable LTR if the device doesn't support.
  //
  if (!PciIoDevice->PciExpressCapability.DeviceCapability2.Bits.LtrMechanism) {
    PciIoDevice->DeviceState.Ltr = FALSE;
  }

  //
  // If the policy is AUTO or NOT_APPLICABLE for a certain device, only enable LTR
  // when any of its children's LTR is enabled.
  // Note:
  //  It's platform's responsibility to make sure consistent policy is returned.
  //  Inconsistent policy means Bridge's LTR is set to FALSE while child device's LTR is
  //  set to TRUE in platform policy.
  //
  if ((PciIoDevice->DeviceState.Ltr != TRUE) && (PciIoDevice->DeviceState.Ltr != FALSE)) {
    ASSERT (PciIoDevice->DeviceState.Ltr == EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO ||
            PciIoDevice->DeviceState.Ltr == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);

    if ((Level < PCI_MAX_BUS) && (Ltr[Level + 1] != 0xFF)) {
      //
      // LTR of a parent is the "OR" result of LTR of all children.
      //
      PciIoDevice->DeviceState.Ltr = Ltr[Level + 1];

      //
      // Reset the LTR status of Level + 1 because Ltr[Level + 1] will be used by another sub-tree.
      //
      Ltr[Level + 1] = 0xFF;
    }
  }

  if ((PciIoDevice->DeviceState.Ltr == TRUE) || (PciIoDevice->DeviceState.Ltr == FALSE)) {
    LtrOr (&Ltr[Level], PciIoDevice->DeviceState.Ltr);
  }

  return EFI_SUCCESS;
}

/**
  Program the LTR settings of each device.

  The program needs to be done in pre-order per the PCIE spec requirement

  @param PciIoDevice  A pointer to the PCI_IO_DEVICE.
  @param Context      Pointer to feature specific context.

  @retval EFI_SUCCESS setup of PCI feature LTR is successful.
**/
EFI_STATUS
LtrProgram (
  IN  PCI_IO_DEVICE *PciIoDevice,
  IN  UINTN         Level,
  IN  VOID          **Context
  )
{
  if ((PciIoDevice->DeviceState.Ltr == TRUE) || (PciIoDevice->DeviceState.Ltr == FALSE)) {
    if (PciIoDevice->DeviceState.Ltr != PciIoDevice->PciExpressCapability.DeviceControl2.Bits.LtrMechanism) {
      
      DEBUG ((
        DEBUG_INFO, "  %a [%02d|%02d|%02d]: %x -> %x.\n",
        __FUNCTION__, PciIoDevice->BusNumber, PciIoDevice->DeviceNumber, PciIoDevice->FunctionNumber,
        PciIoDevice->PciExpressCapability.DeviceControl2.Bits.LtrMechanism,
        PciIoDevice->DeviceState.Ltr
        ));

      return PciIoDevice->PciIo.Pci.Write (
                                    &PciIoDevice->PciIo,
                                    EfiPciIoWidthUint16,
                                    PciIoDevice->PciExpressCapabilityOffset
                                    + OFFSET_OF (PCI_CAPABILITY_PCIEXP, DeviceControl2),
                                    1,
                                    &PciIoDevice->PciExpressCapability.DeviceControl2.Uint16
                                    );
    }
  }

  return EFI_SUCCESS;
}

/**
  Program AtomicOp.

  @param PciIoDevice  A pointer to the PCI_IO_DEVICE.
  @param Level        The level of the PCI device in the heirarchy.
                      Level of root ports is 0.
  @param Context      Pointer to feature specific context.

  @retval EFI_SUCCESS setup of PCI feature LTR is successful.
**/
EFI_STATUS
AtomicOpProgram (
  IN  PCI_IO_DEVICE *PciIoDevice,
  IN  UINTN         Level,
  IN  VOID          **Context
  )
{
  if (PciIoDevice->DeviceState.AtomicOp == EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO ||
      PciIoDevice->DeviceState.AtomicOp == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE) {
    return EFI_SUCCESS;
  }

  //
  // BIT0 of the policy value is for AtomicOp Requester Enable (BIT6)
  // BIT1 of the policy value is for AtomicOp Egress Blocking (BIT7)
  //
  if ((PciIoDevice->DeviceState.AtomicOp >> 2) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (!PciIoDevice->PciExpressCapability.DeviceCapability2.Bits.AtomicOpRouting) {
    PciIoDevice->DeviceState.AtomicOp &= ~BIT1;
  }
  if (PciIoDevice->DeviceState.AtomicOp != 
      BitFieldRead16 (PciIoDevice->PciExpressCapability.DeviceControl2.Uint16, 6, 7)) {

      DEBUG ((
        DEBUG_INFO, "  %a [%02d|%02d|%02d]: %x -> %x.\n",
        __FUNCTION__, PciIoDevice->BusNumber, PciIoDevice->DeviceNumber, PciIoDevice->FunctionNumber,
        BitFieldRead16 (PciIoDevice->PciExpressCapability.DeviceControl2.Uint16, 6, 7),
        PciIoDevice->DeviceState.AtomicOp
        ));
      BitFieldWrite16 (
        PciIoDevice->PciExpressCapability.DeviceControl2.Uint16, 6, 7,
        PciIoDevice->DeviceState.AtomicOp
        );
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