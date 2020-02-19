/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PciBus.h"
#include "PcieFeatureSupport.h"

EFI_PCI_EXPRESS_PLATFORM_PROTOCOL *mPciePlatformProtocol;

/**
  This function retrieves the PCI Express Platform Protocols published by platform
  @retval EFI_STATUS          direct return status from the LocateProtocol ()
                              boot service for the PCI Express Override Protocol
          EFI_SUCCESS         The PCI Express Platform Protocol is found
**/
EFI_STATUS
InitializePciExpressProtocols (
  VOID
  )
{
  EFI_STATUS                      Status;

  Status = gBS->LocateProtocol (
                  &gEfiPciExpressPlatformProtocolGuid,
                  NULL,
                  (VOID **) &mPciePlatformProtocol
                  );
  if (EFI_ERROR (Status)) {
    //
    // If PCI Express Platform protocol doesn't exist, try to get the Pci Express
    // Override Protocol and treat it as PCI Express Platform protocol.
    //
    Status = gBS->LocateProtocol (
                    &gEfiPciExpressOverrideProtocolGuid,
                    NULL,
                    (VOID **) &mPciePlatformProtocol
                    );
  }
  return Status;
}
