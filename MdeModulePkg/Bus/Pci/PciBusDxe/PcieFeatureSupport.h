/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_FEATURE_SUPPORT_H__
#define __PCIE_FEATURE_SUPPORT_H__

typedef
EFI_STATUS
(* PCIE_FEATURE_CONFIGURE) (
  IN PCI_IO_DEVICE *PciDevice,
  IN UINTN         Level,
  IN VOID          **Context
  );

typedef enum {
  PcieFeatureScan,
  PcieFeatureProgram,
  PcieFeatureConfigurationPhaseMax
} PCIE_FEATURE_CONFIGURATION_PHASE;

typedef struct {
  UINTN                  PlatformPolicyOffset;
  BOOLEAN                Enable;
  BOOLEAN                PreOrder[PcieFeatureConfigurationPhaseMax];
  PCIE_FEATURE_CONFIGURE Configure[PcieFeatureConfigurationPhaseMax];
} PCIE_FEATURE_ENTRY;

extern EFI_PCI_EXPRESS_PLATFORM_PROTOCOL *mPciePlatformProtocol;
/**
  This function retrieves the PCI Express Platform Protocols published by platform
  @retval EFI_STATUS          direct return status from the LocateProtocol ()
                              boot service for the PCI Express Override Protocol
          EFI_SUCCESS         The PCI Express Platform Protocol is found
**/
EFI_STATUS
InitializePciExpressProtocols (
  VOID
  );


/**
  The routine calls EFI_PCI_EXPRESS_PLATFORM_PROTOCOL::GetPolicy() to get platform policy
  regarding which PCI-E features are required to initialize by PCI core (PCI BUS) driver.
**/
EFI_STATUS
PcieGetPolicy (
  VOID
  );

/**
  Enumerate all the nodes of the specified root bridge or PCI-PCI Bridge, to
  configure the other PCI features.

  @param RootBridge          A pointer to the PCI_IO_DEVICE.

  @retval EFI_SUCCESS           The other PCI features configuration during enumeration
                                of all the nodes of the PCI root bridge instance were
                                programmed in PCI-compliance pattern along with the
                                device-specific policy, as applicable.
  @retval EFI_UNSUPPORTED       One of the override operation among the nodes of
                                the PCI hierarchy resulted in a incompatible address
                                range.
  @retval EFI_INVALID_PARAMETER The override operation is performed with invalid input
                                parameters.
**/
EFI_STATUS
EnumerateRootBridgePcieFeatures (
  IN PCI_IO_DEVICE          *RootBridge
  );

#endif
