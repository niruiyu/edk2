/** @file
  PCI standard feature support functions implementation for PCI Bus module..

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PciBus.h"
#include "PcieFeatureSupport.h"
#include "PcieFeatures.h"

EFI_PCI_EXPRESS_PLATFORM_PROTOCOL    *mPciePlatformProtocol;
GLOBAL_REMOVE_IF_UNREFERENCED CHAR16 *mPcieFeatureConfigurePhaseStr[] = { L"Scan", L"Program" };

EFI_PCI_EXPRESS_DEVICE_POLICY  mPcieDevicePolicyTemplate = {
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

GLOBAL_REMOVE_IF_UNREFERENCED CHAR16 *mPcieFeatureStr[] = {
  L"Maximum Payload Size",
  L"Maximum Read Request Size",
  L"Extended Tag",
  L"Relaxed Ordering",
  L"No-Snoop",
  L"ASPM",
  L"Common Clock Configuration",
  L"Extended Sync",
  L"Atomic Op",
  L"LTR",
  L"PTM",
  L"Completion Timeout",
  L"Clock Power Management",
  L"L1 PM Substates"
};
STATIC_ASSERT (
  ARRAY_SIZE (mPcieFeatureStr) == sizeof (EFI_PCI_EXPRESS_PLATFORM_POLICY), "mPcieFeatureStr doesn't cover all PCI-E features!"
  );

PCIE_FEATURE_ENTRY  mPcieFeatures[] = {
  //
  // Individual PCIE features
  //
  { OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, MaxPayloadSize),
              TRUE, { TRUE,  TRUE }, { MaxPayloadSizeScan,      MaxPayloadSizeProgram } },
};

VOID
FormalizeDevicePolicy (
  EFI_PCI_EXPRESS_DEVICE_POLICY  *DevicePolicy
  )
{
  if ((DevicePolicy->MaxPayloadSize > PCIE_MAX_PAYLOAD_SIZE_4096B) &&
      (DevicePolicy->MaxPayloadSize != EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) &&
      (DevicePolicy->MaxPayloadSize != EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE)) {
    //
    // Treat invalid values as NOT_APPLICABLE.
    //
    DevicePolicy->MaxPayloadSize = EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE;
  }

  if ((DevicePolicy->MaxReadRequestSize > PCIE_MAX_READ_REQ_SIZE_4096B) &&
      (DevicePolicy->MaxReadRequestSize != EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) &&
      (DevicePolicy->MaxReadRequestSize != EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE)) {
    //
    // Treat invalid values as NOT_APPLICABLE.
    //
    DevicePolicy->MaxPayloadSize = EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE;
  }
  if ((DevicePolicy->RelaxedOrdering > 1) &&
      (DevicePolicy->RelaxedOrdering != EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO) &&
      (DevicePolicy->RelaxedOrdering != EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE)) {
    //
    // Treat invalid values as NOT_APPLICABLE.
    //
    DevicePolicy->MaxPayloadSize = EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE;
  }
}

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
  IN PCI_IO_DEVICE *PciIoDevice,
  IN UINTN         Level,
  IN VOID          **Context
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

  CopyMem (&PciIoDevice->DeviceState, &mPcieDevicePolicyTemplate, sizeof (mPcieDevicePolicyTemplate));
  Status = mPciePlatformProtocol->GetDevicePolicy (
                                    mPciePlatformProtocol,
                                    RootBridgeHandle,
                                    PciAddress,
                                    sizeof (PciIoDevice->DeviceState),
                                    &PciIoDevice->DeviceState
                                    );
   //                 "  Device   MPS MRRS RO NS CTO LTR\n"
  DEBUG ((
    DEBUG_INFO, "  %02x|%02x|%02x %03x %04x %02x %02x %03x %03x\n",
    PciIoDevice->BusNumber, PciIoDevice->DeviceNumber, PciIoDevice->FunctionNumber,
    PciIoDevice->DeviceState.MaxPayloadSize,
    PciIoDevice->DeviceState.MaxReadRequestSize,
    PciIoDevice->DeviceState.RelaxedOrdering,
    PciIoDevice->DeviceState.NoSnoop,
    PciIoDevice->DeviceState.CompletionTimeout,
    PciIoDevice->DeviceState.Ltr
  ));
  FormalizeDevicePolicy (&PciIoDevice->DeviceState);
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
  IN PCI_IO_DEVICE *PciIoDevice,
  IN UINTN         Level,
  IN VOID          **Context
  )
{
  return mPciePlatformProtocol->NotifyDeviceState (
                                  mPciePlatformProtocol,
                                  PciIoDevice->Handle,
                                  sizeof (PciIoDevice->DeviceState),
                                  &PciIoDevice->DeviceState
                                  );
}



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

  //
  // Update the default device policy based on platform policy.
  // For disabled features, the device policy is set to EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE;
  // For enabled features, the device policy is set to EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO;
  //
  switch (PlatformPolicyOffset) {
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, MaxPayloadSize):
    mPcieDevicePolicyTemplate.MaxPayloadSize = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, MaxReadRequestSize):
    mPcieDevicePolicyTemplate.MaxReadRequestSize = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, ExtendedTag):
    mPcieDevicePolicyTemplate.ExtendedTag = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, RelaxedOrdering):
    mPcieDevicePolicyTemplate.RelaxedOrdering = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, NoSnoop):
    mPcieDevicePolicyTemplate.NoSnoop = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, Aspm):
    mPcieDevicePolicyTemplate.AspmControl = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, CommonClockConfiguration):
    mPcieDevicePolicyTemplate.CommonClockConfiguration = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, ExtendedSynch):
    mPcieDevicePolicyTemplate.ExtendedSynch = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, AtomicOp):
    mPcieDevicePolicyTemplate.AtomicOp = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, Ltr):
    mPcieDevicePolicyTemplate.Ltr = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, Ptm):
    mPcieDevicePolicyTemplate.Ptm = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, CompletionTimeout):
    mPcieDevicePolicyTemplate.CompletionTimeout = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, ClockPowerManagement):
    mPcieDevicePolicyTemplate.ClockPowerManagement = (Enable ? EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO : EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
    break;
    case OFFSET_OF (EFI_PCI_EXPRESS_PLATFORM_POLICY, L1PmSubstates):
    mPcieDevicePolicyTemplate.L1PmSubstates.Override = 0;
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

/**
  The routine calls EFI_PCI_EXPRESS_PLATFORM_PROTOCOL::GetPolicy() to get platform policy
  regarding which PCI-E features are required to initialize by PCI core (PCI BUS) driver.
**/
EFI_STATUS
PcieGetPolicy (
  VOID
  )
{
  UINTN                           Index;
  EFI_STATUS                      Status;
  EFI_PCI_EXPRESS_PLATFORM_POLICY PciePolicy;
  EFI_PCI_EXPRESS_PLATFORM_POLICY PciePlatformPolicy;

  ASSERT (mPciePlatformProtocol != NULL);
  //
  // Init PciePolicy to all disabled.
  //
  ZeroMem (&PciePolicy, sizeof (PciePolicy));

  //
  // Get PciBus driver's capability
  //
  for (Index = 0; Index < ARRAY_SIZE (mPcieFeatures); Index++) {
    if (mPcieFeatures[Index].PlatformPolicyOffset != sizeof (PciePolicy)) {
      ASSERT (mPcieFeatures[Index].PlatformPolicyOffset < sizeof (PciePolicy));
      ((BOOLEAN *) &PciePolicy)[mPcieFeatures[Index].PlatformPolicyOffset] = mPcieFeatures[Index].Enable;
    }
  }

  CopyMem (&PciePlatformPolicy, &PciePolicy, sizeof (PciePolicy));
  Status = mPciePlatformProtocol->GetPolicy (
                                    mPciePlatformProtocol,
                                    sizeof (PciePlatformPolicy),
                                    &PciePlatformPolicy
                                    );
  if (!EFI_ERROR (Status)) {
    //
    // Follow platform policy for PCIE features that PciBus driver supports.
    // Ignore platform policy for PCIE features that PciBus driver doesn't support.
    //
    for (Index = 0; Index < sizeof (PciePolicy); Index++) {
      if (((BOOLEAN *) &PciePolicy)[Index]) {
        ((BOOLEAN *) &PciePolicy)[Index] = ((BOOLEAN *) &PciePlatformPolicy)[Index];
        DEBUG ((
          DEBUG_INFO, "PCIE: PciePlatform::GetPolicy() %s %s\n",
          ((BOOLEAN *) &PciePlatformPolicy)[Index] ? L"enabled " : L"disabled",
          mPcieFeatureStr[Index]
          ));

      } else {
        if (((BOOLEAN *) &PciePlatformPolicy)[Index]) {
          DEBUG ((DEBUG_ERROR, "PCIE: %s is NOT supported but enabled by PciePlatform::GetPolicy()! Keep it as disabled.\n", mPcieFeatureStr[Index]));
        }
      }
    }

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
  return Status;
}

VOID
EnumeratePcieDevices (
  PCI_IO_DEVICE          *Bridge,
  BOOLEAN                PreOrder,
  PCIE_FEATURE_CONFIGURE Routine,
  UINTN                  Level,
  VOID                   **Context
)
{
  LIST_ENTRY            *Link;
  PCI_IO_DEVICE         *PciIoDevice;

  if (PreOrder) {
    Routine (Bridge, Level, Context);
  }

  for ( Link = GetFirstNode (&Bridge->ChildList)
      ; !IsNull (&Bridge->ChildList, Link)
      ; Link = GetNextNode (&Bridge->ChildList, Link)
  ) {
    PciIoDevice = PCI_IO_DEVICE_FROM_LINK (Link);
    if (PciIoDevice->IsPciExp) {
      EnumeratePcieDevices (PciIoDevice, PreOrder, Routine, Level + 1, Context);
    }
  }

  if (!PreOrder) {
    Routine (Bridge, Level, Context);
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
    DEBUG ((DEBUG_INFO, "%a: %s ...\n", __FUNCTION__, Str != NULL ? Str : L"<no-devicepath>"));

    if (Str != NULL) {
      FreePool (Str);
    }
  );

  for ( Link = GetFirstNode (&RootBridge->ChildList)
      ; !IsNull (&RootBridge->ChildList, Link)
      ; Link = GetNextNode (&RootBridge->ChildList, Link)
  ) {
    PciDevice = PCI_IO_DEVICE_FROM_LINK (Link);
    if (!PciDevice->IsPciExp) {
      continue;
    }

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
    DEBUG ((
      DEBUG_INFO,
      " GetDevicePolicy phase ...\n"
      "  Device   MPS MRRS RO NS CTO LTR\n"
      ));
    EnumeratePcieDevices (PciDevice, TRUE, PcieGetDevicePolicy, 0, &RootBridge->Handle);

    DEBUG ((DEBUG_INFO, "PCIE[%02d|%02d|%02d] ...\n", PciDevice->BusNumber, PciDevice->DeviceNumber, PciDevice->FunctionNumber));
    //
    // For each heirachy/device-tree, firstly scan recursively to align the settings then program
    // the aligned settings recursively.
    //
    for (Phase = PcieFeatureScan; Phase < PcieFeatureConfigurationPhaseMax; Phase++) {
      DEBUG ((DEBUG_INFO, " %s phase ...\n", mPcieFeatureConfigurePhaseStr[Phase]));
      for (Index = 0; Index < ARRAY_SIZE (mPcieFeatures); Index++) {
        if (!mPcieFeatures[Index].Enable) {
          continue;
        }

        if (mPcieFeatures[Index].Configure[Phase] == NULL) {
          continue;
        }

        EnumeratePcieDevices (
          PciDevice, mPcieFeatures[Index].PreOrder[Phase],
          mPcieFeatures[Index].Configure[Phase], 0, &Context[Index]
          );
      }
    }

    for (Index = 0; Index < ARRAY_SIZE (mPcieFeatures); Index++) {
      if (Context[Index] != NULL) {
        FreePool (Context[Index]);
      }
    }

    //
    // Report device state for all devices in the same heirarchy.
    //
    DEBUG ((DEBUG_INFO, " NotifyDeviceState phase ...\n"));
    EnumeratePcieDevices (PciDevice, TRUE, PcieNotifyDeviceState, 0, NULL);
  }

  return EFI_SUCCESS;
}
