/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_FEATURE_H__
#define __PCIE_FEATURE_H__

typedef
EFI_STATUS
(* PCIE_FEATURE_CONFIGURE) (
  IN PCI_IO_DEVICE *PciDevice,
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
  PCIE_FEATURE_CONFIGURE Configure[PcieFeatureConfigurationPhaseMax];
} PCIE_FEATURE_ENTRY;

extern PCIE_FEATURE_ENTRY                mPcieFeatures[];
extern EFI_PCI_EXPRESS_DEVICE_POLICY     mPcieDevicePolicy;
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
  This function gets the platform requirement to initialize the list of PCI Express
  features from the protocol definition supported.
  This function should be called after the LocatePciPlatformProtocol.
  @retval EFI_SUCCESS           return by platform to acknowledge the list of
                                PCI Express feature to be configured
                                (in mPciePlatformPolicy)
  @retval EFI_INVALID_PARAMETER platform does not support the protocol arguements
                                passed
  @retval EFI_UNSUPPORTED       platform did not published the protocol
**/
EFI_STATUS
PciExpressPlatformGetPolicy (
  );

/**
  Gets the PCI device-specific platform policy from the PCI Platform Protocol.
  If no PCI Platform protocol is published than setup the PCI feature to predetermined
  defaults, in order to align all the PCI devices in the PCI hierarchy, as applicable.

  @param  PciDevice     A pointer to PCI_IO_DEVICE

  @retval EFI_STATUS    The direct status from the PCI Platform Protocol
  @retval EFI_SUCCESS   On return of predetermined PCI features defaults, for
                        the case when protocol returns as EFI_UNSUPPORTED to
                        indicate PCI device exist and it has no platform policy
                        defined. Also, on returns when no PCI Platform Protocol
                        exist.
**/
EFI_STATUS
PciExpressPlatformGetDevicePolicy (
  IN PCI_IO_DEVICE          *PciDevice
  );

/**
  Gets the PCI device-specific platform policy from the PCI Express Platform Protocol.
  If no PCI Platform protocol is published than setup the PCI feature to predetermined
  defaults, in order to align all the PCI devices in the PCI hierarchy, as applicable.

  @param  PciIoDevice     A pointer to PCI_IO_DEVICE

  @retval EFI_STATUS    The direct status from the PCI Platform Protocol
  @retval EFI_SUCCESS   On return of predetermined PCI features defaults, for
                        the case when protocol returns as EFI_UNSUPPORTED to
                        indicate PCI device exist and it has no platform policy
                        defined. Also, on returns when no PCI Platform Protocol
                        exist.
**/
EFI_STATUS
PcieGetDevicePolicy (
  IN PCI_IO_DEVICE          *PciIoDevice,
  IN VOID                   **Context
  );

/**
  Translate the EFI definition of Max Payload Size to HW-specific value,
  as per PCI Base Specification Revision 4.0; for the PCI feature Max_Payload_Size.

  @param  EfiMaxPayloadSize  EFI definition of Max Payload Size

  @retval         Range values for the Max_Payload_Size as defined in the PCI
                  Base Specification 4.0
**/
UINT8
MaxPayloadSizeEfiToPcie (
  IN  UINT8                   EfiMaxPayloadSize
);

/**
  Enumerate all the nodes of the specified root bridge or PCI-PCI Bridge, to
  configure the other PCI features.

  @param RootBridge          A pointer to the PCI_IO_DEVICE.

  @retval EFI_SUCCESS           The other PCI features configuration during enumeration
                                of all the nodes of the PCI root bridge instance were
                                programmed in PCI-compliance pattern along with the
                                device-specific policy, as applicable.
  @retval EFI_UNSUPPORTED       One of the override operation maong the nodes of
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
