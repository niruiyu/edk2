/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EFI_PCI_EXPRESS_FEATURES_H_
#define _EFI_PCI_EXPRESS_FEATURES_H_


EFI_STATUS
MaxPayloadScan (
  IN PCI_IO_DEVICE *PciDevice,
  IN VOID          **Context
  );

EFI_STATUS
MaxPayloadProgram (
  IN PCI_IO_DEVICE *PciDevice,
  IN VOID          **Context
  );

#endif
