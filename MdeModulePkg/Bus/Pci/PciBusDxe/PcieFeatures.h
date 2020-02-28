/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EFI_PCI_EXPRESS_FEATURES_H_
#define _EFI_PCI_EXPRESS_FEATURES_H_


EFI_STATUS
MaxPayloadSizeScan (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  );

EFI_STATUS
MaxPayloadSizeProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  );

EFI_STATUS
MaxReadRequestSizeProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  );

EFI_STATUS
RelaxedOrderingProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  );

EFI_STATUS
NoSnoopProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  );

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
  );

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
  );

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
  );

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
  );
#endif
