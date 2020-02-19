/** @file
  Provides a platform-specific method to enable Secure Boot Custom Mode setup.

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>
#include <IndustryStandard/PciExpress50.h>
#include <Protocol/PciExpressPlatform.h>
#include <Protocol/PciIo.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

STATIC
EFI_STATUS
EFIAPI
PcieGetPolicy (
  IN CONST  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL     *This,
  IN        UINTN                                 Size,
  IN OUT    EFI_PCI_EXPRESS_PLATFORM_POLICY       *PlatformPolicy
)
{
  DEBUG ((DEBUG_ERROR, "%a\n", __FUNCTION__));
  PlatformPolicy->MaxPayloadSize = TRUE;
  PlatformPolicy->MaxReadRequestSize = TRUE;
  PlatformPolicy->RelaxedOrdering = TRUE;
  PlatformPolicy->NoSnoop = TRUE;
  PlatformPolicy->CompletionTimeout = TRUE;
  PlatformPolicy->Ltr = TRUE;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
PcieGetDevicePolicy (
  IN CONST  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL             *This,
  IN        EFI_HANDLE                                    RootBridge,
  IN        EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS   PciAddress,
  IN        UINTN                                         Size,
  IN OUT    EFI_PCI_EXPRESS_DEVICE_POLICY                 *PciePolicy
  )
{
  ASSERT (PciePolicy->AspmControl == EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE);
  PciePolicy->MaxPayloadSize = PCIE_MAX_PAYLOAD_SIZE_2048B;

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
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
PcieNotifyDeviceState (
  IN CONST  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL     *This,
  IN        EFI_HANDLE                            PciDevice,
  IN        UINTN                                 Size,
  IN        EFI_PCI_EXPRESS_DEVICE_STATE          *PcieState
  )
{
  EFI_STATUS                                      Status;
  EFI_PCI_IO_PROTOCOL                             *PciIo;
  UINTN                                           Seg, Bus, Dev, Func;

  Status = gBS->HandleProtocol (PciDevice, &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
  ASSERT_EFI_ERROR (Status);

  Status = PciIo->GetLocation (PciIo, &Seg, &Bus, &Dev, &Func);
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_ERROR, "%a: B/D/F=%d/%d/%d\n", __FUNCTION__, Bus, Dev, Func));
  DEBUG ((DEBUG_ERROR, "  MPS=%d\n", PcieState->MaxPayloadSize));
  return EFI_SUCCESS;
}

EFI_PCI_EXPRESS_PLATFORM_PROTOCOL mPciePlatform = {
  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL_REVISION,
  PcieGetDevicePolicy,
  PcieNotifyDeviceState,
  PcieGetPolicy
};

EFI_STATUS
EFIAPI
PciePlatformLibConstructor (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_HANDLE            Handle;

  Handle = NULL;
  DEBUG ((DEBUG_ERROR, "PciePlatform - Install\n"));
  return gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiPciExpressPlatformProtocolGuid, &mPciePlatform, NULL);
}