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
#endif
