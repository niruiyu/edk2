/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PciBus.h"
#include "PcieFeatureSupport.h"
#include "PcieFeatures.h"

EFI_PCI_EXPRESS_PLATFORM_PROTOCOL *mPciePlatformProtocol;
CHAR16 *mPcieFeatureConfigurePhaseStr[] = { L"Scan", L"Program" };

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
  )
{
  EFI_STATUS                                  Status;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS PciAddress;
  EFI_HANDLE                                  RootBridgeHandle;
  
  ASSERT (Context != NULL);
  ASSERT (*Context != NULL);
  RootBridgeHandle            = (EFI_HANDLE) *Context;

  PciAddress.Bus              = PciIoDevice->BusNumber;
  PciAddress.Device           = PciIoDevice->DeviceNumber;
  PciAddress.Function         = PciIoDevice->FunctionNumber;
  PciAddress.Register         = 0;
  PciAddress.ExtendedRegister = 0;

  CopyMem (&PciIoDevice->DeviceState, &mPcieDevicePolicy, sizeof (mPcieDevicePolicy));
  Status = mPciePlatformProtocol->GetDevicePolicy (
                                    mPciePlatformProtocol,
                                    RootBridgeHandle,
                                    PciAddress,
                                    sizeof (PciIoDevice->DeviceState),
                                    &PciIoDevice->DeviceState
                                    );
  return Status;
}



/**
  Notifies the platform about the current PCI Express state of the device.

  @param  PciIoDevice                 A pointer to PCI_IO_DEVICE
  @param  PciExDeviceConfiguration  Pointer to EFI_PCI_EXPRESS_DEVICE_CONFIGURATION.
                                    Used to pass the current state of device to
                                    platform.

  @retval EFI_STATUS        The direct status from the PCI Express Platform Protocol
  @retval EFI_UNSUPPORTED   returns when the PCI Express Platform Protocol or its
                            alias PCI Express OVerride Protocol is not present.
**/
EFI_STATUS
PcieNotifyDeviceState (
  IN PCI_IO_DEVICE               *PciIoDevice,
  IN VOID                        **Context
  )
{
  return mPciePlatformProtocol->NotifyDeviceState (
                                  mPciePlatformProtocol,
                                  PciIoDevice->Handle,
                                  sizeof (PciIoDevice->DeviceState),
                                  &PciIoDevice->DeviceState
                                  );
}

PCIE_FEATURE_ENTRY  mPcieFeatures[] = {
  //
  // Below feature should be put in the first entry of the entry array
  // so that before processing any PCIE features, the device policy
  // can be retrieved from platform protocol first.
  //
  { MAX_UINTN, TRUE, { PcieGetDevicePolicy, NULL              } },

  //
  // Individual PCIE features
  //
  { OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, MaxPayloadSize),
               TRUE, { MaxPayloadScan,      MaxPayloadProgram } },

  //
  // Below feature should be put in the last entry of the entry array
  // so that after all PCIE features are processed, the device state
  // can be reported to platform at last.
  //
  { MAX_UINTN, TRUE, { NULL,                PcieNotifyDeviceState} }
};

EFI_PCI_EXPRESS_DEVICE_POLICY  mPcieDevicePolicy = {
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  { 0 },
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
  { 0 }
};

VOID
EnablePcieFeature (
  UINTN   PlatformPolicyOffset,
  BOOLEAN Enable
) 
{
  UINTN   Index;

  for (Index = 0; Index < ARRAY_SIZE (mPcieFeatures); Index++) {
    if (mPcieFeatures[Index].PlatformPolicyOffset == PlatformPolicyOffset) {
      mPcieFeatures[Index].Enable = Enable;
      break;
    }
  }

  if (Enable) {
    return;
  }

  switch (PlatformPolicyOffset) {
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, MaxPayloadSize):
    mPcieDevicePolicy.MaxPayloadSize = EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE;
    break;
  }
}

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
  EFI_PCI_EXPRESS_PLATFORM_POLICY PciePlatformPolicy;

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
  if (!EFI_ERROR (Status)) {
    // put it later, better to just before pcie feature init
    Status = mPciePlatformProtocol->GetPolicy (
                                      mPciePlatformProtocol,
                                      sizeof (PciePlatformPolicy),
                                      &PciePlatformPolicy
                                      );
    if (!EFI_ERROR (Status)) {
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, MaxPayloadSize),           PciePlatformPolicy.MaxPayloadSize);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, MaxReadRequestSize),       PciePlatformPolicy.MaxReadRequestSize);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, ExtendedTag),              PciePlatformPolicy.ExtendedTag);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, RelaxedOrdering),          PciePlatformPolicy.RelaxedOrdering);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, NoSnoop),                  PciePlatformPolicy.NoSnoop);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, Aspm),                     PciePlatformPolicy.Aspm);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, CommonClockConfiguration), PciePlatformPolicy.CommonClockConfiguration);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, ExtendedSynch),            PciePlatformPolicy.ExtendedSynch);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, AtomicOp),                 PciePlatformPolicy.AtomicOp);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, Ltr),                      PciePlatformPolicy.Ltr);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, Ptm),                      PciePlatformPolicy.Ptm);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, CompletionTimeout),        PciePlatformPolicy.CompletionTimeout);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, ClockPowerManagement),     PciePlatformPolicy.ClockPowerManagement);
      EnablePcieFeature (OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, L1PmSubstates),            PciePlatformPolicy.L1PmSubstates);
    }
  }
  return Status;
}


VOID
DevicePolicyInit (
  EFI_PCI_EXPRESS_DEVICE_POLICY  *DevicePolicy
  )
{
  DevicePolicy->MaxPayloadSize           = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->MaxReadRequestSize       = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->ExtendedTag              = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->RelaxedOrdering          = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->NoSnoop                  = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->AspmControl              = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->CommonClockConfiguration = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->ExtendedSynch            = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->AtomicOp.Override        = 0;
  DevicePolicy->Ltr                      = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->Ptm                      = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->CompletionTimeout        = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->ClockPowerManagement     = EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  DevicePolicy->L1PmSubstates.Override   = 0;
}


VOID
EnumerateBridgePcieFeatures (
  PCIE_FEATURE_CONFIGURE Routine,
  PCI_IO_DEVICE          *Parent,
  VOID                   **Context
)
{
  LIST_ENTRY            *Link;
  PCI_IO_DEVICE         *PciDevice;
  for ( Link = Parent->ChildList.ForwardLink
      ; Link != &Parent->ChildList
      ; Link = Link->ForwardLink
  ) {
    PciDevice = PCI_IO_DEVICE_FROM_LINK (Link);
    if (PciDevice->IsPciExp) {
      Routine (PciDevice, Context);
    }
    
    if (IS_PCI_BRIDGE (&PciDevice->Pci)) {
      EnumerateBridgePcieFeatures (Routine, PciDevice, Context);
    }
  }
}

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
  )
{
  UINTN                 Index;
  LIST_ENTRY            *Link;
  PCI_IO_DEVICE         *PciDevice;
  VOID                  *Context[ARRAY_SIZE (mPcieFeatures)];
  PCIE_FEATURE_CONFIGURATION_PHASE Phase;

  DEBUG_CODE (
    CHAR16                *Str;
    Str = ConvertDevicePathToText (RootBridge->DevicePath, FALSE, FALSE);
    DEBUG ((
      DEBUG_INFO,
      "Enumerating PCI-E features for Root Bridge %s ...\n",
      Str != NULL ? Str : L"<no-devicepath>"
      ));

    if (Str != NULL) {
      FreePool (Str);
    }
  );

  for ( Link = RootBridge->ChildList.ForwardLink
      ; Link != &RootBridge->ChildList
      ; Link = Link->ForwardLink
  ) {
    PciDevice = PCI_IO_DEVICE_FROM_LINK (Link);
    //
    // Some features such as MaxPayloadSize requires the settings in the heirarchy are aligned.
    // Context[Index] holds the feature specific settings in current heirarchy/device-tree.
    //
    for (Index = 0; Index < ARRAY_SIZE (mPcieFeatures); Index++) {
      Context[Index] = NULL;
    }
    //
    // First feature is not a real PCIE feature, but to query device policy.
    // RootBridge handle is Context passed in.
    //
    Context[0] = RootBridge->Handle;
    //
    // For each heirachy/device-tree, firstly scan recursively to align the settings then program
    // the aligned settings recursively.
    //
    for (Phase = PcieFeatureScan; Phase < PcieFeatureConfigurationPhaseMax; Phase++) {
      DEBUG ((DEBUG_INFO, "<<********** Phase [%s]**********>>\n", mPcieFeatureConfigurePhaseStr[Phase]));
      for (Index = 0; Index < ARRAY_SIZE (mPcieFeatures); Index++) {
        if (!mPcieFeatures[Index].Enable) {
          continue;
        }

        if (mPcieFeatures[Index].Configure[Phase] == NULL) {
          continue;
        }
      
        if (PciDevice->IsPciExp) {
          mPcieFeatures[Index].Configure[Phase] (PciDevice, &Context[Index]);
        }

        if (IS_PCI_BRIDGE (&PciDevice->Pci)) {
          EnumerateBridgePcieFeatures (mPcieFeatures[Index].Configure[Phase], PciDevice, &Context[Index]);
        }
      }
    }
    
    Context[0] = NULL;

    for (Index = 0; Index < ARRAY_SIZE (mPcieFeatures); Index++) {
      if (Context[Index] != NULL) {
        FreePool (Context[Index]);
      }
    }
  }

  return EFI_SUCCESS;
}
