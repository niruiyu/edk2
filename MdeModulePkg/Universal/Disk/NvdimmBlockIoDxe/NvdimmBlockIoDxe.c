/** @file

  Provide driver binding functions.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "NvdimmBlockIoDxe.h"

typedef struct {
  ACPI_ADR_DEVICE_PATH      Adr;
  EFI_DEVICE_PATH_PROTOCOL  End;
} NVDIMM_LABEL_DEVICE_PATH;


CACHE_LINE_FLUSH CacheLineFlush;

#ifdef NT32
VOID *
EFIAPI
FlushDummy (
  IN      VOID                      *LinearAddress
)
{
  return NULL;
}
#endif
/**
  Initialize the cache line flush function.
**/
VOID
InitializeCpuCommands (
  VOID
  )
{
  CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_EBX  Ebx;

  AsmCpuidEx (
    CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS, CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_SUB_LEAF_INFO,
    NULL, &Ebx.Uint32, NULL, NULL
  );

  if (Ebx.Bits.CLFLUSHOPT == 1) {
    CacheLineFlush = AsmFlushCacheLineOpt;
    DEBUG ((DEBUG_INFO, "Flushing assigned to ClFlushOpt.\n"));
  } else {
    CacheLineFlush = AsmFlushCacheLine;
    DEBUG ((DEBUG_INFO, "Flushing assigned to ClFlush.\n"));
  }
#ifdef NT32
  CacheLineFlush = FlushDummy;
#endif
}

/**
  Return whether the device path node is of NVDIMM namespace type.

  @param Node  The device path node to check.

  @retval TRUE  The device path node is of NVDIMM namespace type.
  @retval FALSE The device path node is not of NVDIMM namespace type.
**/
BOOLEAN
IsDevicePathNvdimmNamespace (
  EFI_DEVICE_PATH_PROTOCOL           *Node
  )
{
  return (BOOLEAN) (
    (DevicePathType (Node) == MESSAGING_DEVICE_PATH) &&
    (DevicePathSubType (Node) == MSG_NVDIMM_NAMESPACE_DP)
    );
}

/**
  Return the NVDIMM Label handles array indicated by the remaining device path.

  @param Controller          The NVDIMM Label handle that's passed to Start()/Supported().
  @param RemainingDevicePath The remaining device path that's passed to Start()/Supported().
  @param Handles             Return the NVDIMM Label handles array.
  @param HandleNum           Return the number of NVDIMM Label handles in the array.

  @retval EFI_SUCCESS           The NVDIMM Label handles array is returned.
  @retval EFI_INVALID_PARAMETER The Controller handle is not a valid NVDIMM Label handle, or
                                one of the ACPI_ADR node in RemainingDevicePath does't identify the NVDIMM Label handle.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource to create the NVDIMM Label handles.
**/
EFI_STATUS
GetAllNvdimmLabelHandles (
  IN EFI_HANDLE                      Controller,
  IN OUT EFI_DEVICE_PATH_PROTOCOL    **RemainingDevicePath,
  OUT EFI_HANDLE                     **Handles,
  OUT UINTN                          *HandleNum
  )
{
  EFI_STATUS                         Status;
  EFI_DEVICE_PATH_PROTOCOL           *AcpiAdr;
  EFI_DEVICE_PATH_PROTOCOL           *Node;
  NVDIMM_LABEL_DEVICE_PATH           NvdimmLabelDp;
  EFI_HANDLE                         Handle;

  ASSERT (HandleNum != NULL);
  ASSERT (Handles != NULL);
  Status = gBS->OpenProtocol (
    Controller,
    &gEfiNvdimmLabelProtocolGuid,
    NULL,
    gImageHandle,
    Controller,
    EFI_OPEN_PROTOCOL_TEST_PROTOCOL
    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (*RemainingDevicePath == NULL) {
    Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiNvdimmLabelProtocolGuid, NULL, HandleNum, Handles);
    if (EFI_ERROR (Status)) {
      *Handles = NULL;
      *HandleNum = 0;
    }
  } else {
    SetDevicePathEndNode (&NvdimmLabelDp.End);
    *HandleNum = 1;
    for (AcpiAdr = *RemainingDevicePath
      ; (DevicePathType (AcpiAdr) == ACPI_DEVICE_PATH) && (DevicePathSubType (AcpiAdr) == ACPI_ADR_DP)
      ; AcpiAdr = NextDevicePathNode (AcpiAdr)
      ) {
      if (DevicePathNodeLength (AcpiAdr) != sizeof (ACPI_ADR_DEVICE_PATH)) {
        return EFI_INVALID_PARAMETER;
      }
      CopyMem (&NvdimmLabelDp.Adr, AcpiAdr, sizeof (ACPI_ADR_DEVICE_PATH));
      Node = (EFI_DEVICE_PATH_PROTOCOL *)&NvdimmLabelDp;
      Status = gBS->LocateDevicePath (&gEfiNvdimmLabelProtocolGuid, &Node, &Handle);
      if (EFI_ERROR (Status) || !IsDevicePathEnd (Node)) {
        return EFI_INVALID_PARAMETER;
      }
      (*HandleNum)++;
    }
    *Handles = AllocatePool (*HandleNum * sizeof (EFI_HANDLE));
    if (*Handles == NULL) {
      *HandleNum = 0;
      return EFI_OUT_OF_RESOURCES;
    }
    (*Handles)[0] = Controller;
    *HandleNum = 1;
    for (AcpiAdr = *RemainingDevicePath
      ; (DevicePathType (AcpiAdr) == ACPI_DEVICE_PATH) && (DevicePathSubType (AcpiAdr) == ACPI_ADR_DP)
      ; AcpiAdr = NextDevicePathNode (AcpiAdr)
      ) {
      CopyMem (&NvdimmLabelDp.Adr, AcpiAdr, sizeof (ACPI_ADR_DEVICE_PATH));
      Node = (EFI_DEVICE_PATH_PROTOCOL *)&NvdimmLabelDp;
      Status = gBS->LocateDevicePath (&gEfiNvdimmLabelProtocolGuid, &Node, &Handle);
      ASSERT (!EFI_ERROR (Status) && IsDevicePathEnd (Node));
      (*Handles)[*HandleNum] = Handle;
      (*HandleNum)++;
    }

    //
    // Update RemainingDevicePath to the node after ACPI_ADR nodes.
    //
    *RemainingDevicePath = AcpiAdr;
  }
  return Status;
}

/**
  Test to see if this driver supports ControllerHandle.

  @param  This                Protocol instance pointer.
  @param  Controller          Handle of device to test.
  @param  RemainingDevicePath Optional parameter use to pick a specific child
                              device to start.

  @retval EFI_SUCCESS         This driver supports this device.
  @retval EFI_ALREADY_STARTED This driver is already running on this device.
  @retval other               This driver does not support this device.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN EFI_HANDLE                      Controller,
  IN EFI_DEVICE_PATH_PROTOCOL        *RemainingDevicePath
  )
{
  EFI_STATUS                         Status;
  EFI_NVDIMM_LABEL_PROTOCOL          *NvdimmLabel;
  UINTN                              Index;
  EFI_HANDLE                         *Handles;
  UINTN                              HandleNum;

  Status = GetAllNvdimmLabelHandles (Controller, &RemainingDevicePath, &Handles, &HandleNum);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleNum; Index++) {
    Status = gBS->OpenProtocol (
      Handles[Index],
      &gEfiNvdimmLabelProtocolGuid,
      (VOID **)&NvdimmLabel,
      gImageHandle,
      Handles[Index],
      EFI_OPEN_PROTOCOL_BY_DRIVER
    );
    if (!EFI_ERROR (Status) && (Status != EFI_ALREADY_STARTED)) {
      gBS->CloseProtocol (Controller, &gEfiNvdimmLabelProtocolGuid, gImageHandle, Controller);
    } else {
      if (Status != EFI_ALREADY_STARTED) {
        return Status;
      }
    }
  }

  if (RemainingDevicePath == NULL) {
    return EFI_SUCCESS;
  }

  //
  // After optional multiple ACPI_ADR nodes, NVDIMM_NAMESPACE node or END node may come.
  //
  if (IsDevicePathEnd (RemainingDevicePath)) {
    return EFI_SUCCESS;
  }
  if ((DevicePathType (RemainingDevicePath) != MESSAGING_DEVICE_PATH) ||
    (DevicePathSubType (RemainingDevicePath) != MSG_NVDIMM_NAMESPACE_DP)) {
    return EFI_INVALID_PARAMETER;
  }
  return EFI_SUCCESS;
}


/**
  Start to manage the NVDIMM labels.

  @param  This                Protocol instance pointer.
  @param  Controller          Handle of device to test.
  @param  RemainingDevicePath Optional parameter use to pick a specific child
                              device to start.

  @retval EFI_SUCCESS         This driver supports this device.
  @retval EFI_ALREADY_STARTED This driver is already running on this device.
  @retval other               This driver does not support this device.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN EFI_HANDLE                      Controller,
  IN EFI_DEVICE_PATH_PROTOCOL        *RemainingDevicePath
  )
{
  EFI_STATUS                         Status;
  EFI_NVDIMM_LABEL_PROTOCOL          *NvdimmLabel;
  UINTN                              Index;
  EFI_HANDLE                         *Handles;
  UINTN                              HandleNum;
  BOOLEAN                            *Opened;

  Status = GetAllNvdimmLabelHandles (Controller, &RemainingDevicePath, &Handles, &HandleNum);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Opened = AllocateZeroPool (HandleNum * sizeof (BOOLEAN));
  if (Opened == NULL) {
    FreePool (Handles);
    return EFI_OUT_OF_RESOURCES;
  }
  for (Index = 0; Index < HandleNum; Index++) {
    Status = gBS->OpenProtocol (
      Handles[Index],
      &gEfiNvdimmLabelProtocolGuid,
      (VOID **)&NvdimmLabel,
      gImageHandle,
      Handles[Index],
      EFI_OPEN_PROTOCOL_BY_DRIVER
    );
    if (!EFI_ERROR (Status) && (Status != EFI_ALREADY_STARTED)) {
      Opened[Index] = TRUE;
    } else {
      if (Status != EFI_ALREADY_STARTED) {
        goto ErrorExit;
      }
    }
  }

  if (RemainingDevicePath != NULL) {
    //
    // After optional multiple ACPI_ADR nodes, END or NVDIMM_NAMESPACE node may come.
    //
    if (!IsDevicePathEnd (RemainingDevicePath) && !IsDevicePathNvdimmNamespace (RemainingDevicePath)) {
      Status = EFI_INVALID_PARAMETER;
      goto ErrorExit;
    }
  }

  if ((RemainingDevicePath == NULL) || !IsDevicePathEnd (RemainingDevicePath)) {

    if (!mPmem.Initialized) {
      //
      // Parse ACPI NFIT table and create all NVDIMM instances referenced in ACPI NFIT table.
      // It may create more than HandleNum NVDIMM instances.
      //
      Status = ParseNfit ();
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }
      mPmem.Initialized = TRUE;
    }

    //
    // Load and parse all the NVDIMM Labels
    //
    Status = LoadAllNvdimmLabels (Handles, HandleNum);
    if (!EFI_ERROR (Status)) {
      Status = ParseNvdimmLabels ();
    }
  }

ErrorExit:
  FreePool (Opened);
  FreePool (Handles);

  if (EFI_ERROR (Status)) {
    //
    // Roll back the open operations for previous handles,
    //
    while (Index-- != 0) {
      if (Opened[Index]) {
        gBS->CloseProtocol (Handles[Index], &gEfiNvdimmLabelProtocolGuid, gImageHandle, Handles[Index]);
      }
    }
  }
  return Status;
}

/**
  Build the parent-child open relation ship between the namespace blockio and the NVDIMMs.

  @param Namespace  The namespace where the blockio is populated.
**/
VOID
OpenNvdimmLabelsByChild (
  NVDIMM_NAMESPACE          *Namespace
  )
{
  EFI_STATUS                Status;
  EFI_NVDIMM_LABEL_PROTOCOL *NvdimmLabel;
  UINTN                     LabelIndex;
  for (LabelIndex = 0; LabelIndex < Namespace->LabelCount; LabelIndex++) {
    Status = gBS->OpenProtocol (
      Namespace->Labels[LabelIndex].Nvdimm->Handle,
      &gEfiNvdimmLabelProtocolGuid,
      (VOID **)&NvdimmLabel,
      gImageHandle,
      Namespace->Handle,
      EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
    );
    ASSERT_EFI_ERROR (Status);
    //
    // BLOCK namespace doesn't across NVDIMMs.
    //
    if (Namespace->Type == NamespaceTypeBlock) {
      break;
    }
  }
}

/**
  Uninstall all BlockIo protocols and stop to manage the NVDIMM Labels.

  @param[in] This                 Pointer to driver binding protocol.
  @param[in] Controller           Controller handle to connect.
  @param[in] NumberOfChildren     Number of children handle created by this driver.
  @param[in] ChildHandleBuffer    Buffer containing child handle created.

  @retval EFI_SUCCESS      Driver disconnected successfully from controller.
  @retval EFI_DEVICE_ERROR Driver fails to stop.
**/
EFI_STATUS
EFIAPI
NvdimmBlockIoDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL *This,
  IN  EFI_HANDLE                  Controller,
  IN  UINTN                       NumberOfChildren,
  IN  EFI_HANDLE                  *ChildHandleBuffer
  )
{
  EFI_STATUS                      Status;
  UINTN                           Index;
  LIST_ENTRY                      *Link;
  NVDIMM                          *Nvdimm;
  EFI_BLOCK_IO_PROTOCOL           *BlockIo;
  NVDIMM_NAMESPACE                *Namespace;
  UINTN                           LabelIndex;
  UINTN                           NumberOfChildrenStopped;

  if (NumberOfChildren == 0) {
    //
    // # of namespaces equals to # of child BlockIo.
    //
    ASSERT (IsListEmpty (&mPmem.Namespaces));

    //
    // Close all BY_DRIVER relationships between each NVDIMM instances and this driver.
    //
    for (Link = GetFirstNode (&mPmem.Nvdimms)
      ; !IsNull (&mPmem.Nvdimms, Link)
      ; Link = GetNextNode (&mPmem.Nvdimms, Link)
      ) {
      Nvdimm = NVDIMM_FROM_LINK (Link);
      if (Nvdimm->Handle != NULL) {
        Status = gBS->CloseProtocol (Nvdimm->Handle, &gEfiNvdimmLabelProtocolGuid, gImageHandle, Nvdimm->Handle);
        ASSERT_EFI_ERROR (Status);
      }
    }

    //
    // Below destruction isn't really necessary.
    // Let's do them for better code coverage.
    //
    FreeNvdimms (&mPmem.Nvdimms);
    FreeNfitStructs ();
    mPmem.Initialized = FALSE;
    return EFI_SUCCESS;
  }

  NumberOfChildrenStopped = 0;
  for (Index = 0; Index < NumberOfChildren; Index++) {
    Status = gBS->OpenProtocol (
      ChildHandleBuffer[Index],
      &gEfiBlockIoProtocolGuid,
      (VOID **)&BlockIo,
      gImageHandle,
      Controller,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Namespace = NVDIMM_NAMESPACE_FROM_BLOCK_IO (BlockIo);

    //
    // Close the parent-child relationship between NVDIMM and namespace.
    //
    for (LabelIndex = 0; LabelIndex < Namespace->LabelCount; LabelIndex++) {
      Status = gBS->CloseProtocol (Namespace->Labels[LabelIndex].Nvdimm->Handle, &gEfiNvdimmLabelProtocolGuid, gImageHandle, Namespace->Handle);
      ASSERT_EFI_ERROR (Status);
      if (Namespace->Type == NamespaceTypeBlock) {
        break;
      }
    }

    Status = gBS->UninstallMultipleProtocolInterfaces (
      Namespace->Handle,
      &gEfiBlockIoProtocolGuid, &Namespace->BlockIo,
      &gEfiDevicePathProtocolGuid, &Namespace->DevicePath,
      NULL
    );
    if (EFI_ERROR (Status)) {
      OpenNvdimmLabelsByChild (Namespace);
      continue;
    }

    RemoveEntryList (&Namespace->Link);
    FreeNamespace (Namespace);
    NumberOfChildrenStopped++;
  }

  if (NumberOfChildrenStopped == NumberOfChildren) {
    return EFI_SUCCESS;
  } else {
    return EFI_DEVICE_ERROR;
  }
}

EFI_DRIVER_BINDING_PROTOCOL gNvdimmBlockIoDriverBinding = {
  NvdimmBlockIoDriverBindingSupported,
  NvdimmBlockIoDriverBindingStart,
  NvdimmBlockIoDriverBindingStop,
  0xa,
  NULL,
  NULL

};

/**
  The user Entry Point for module NvdimmBlockIoDxe. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeNvdimmBlockIo (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS              Status;

  InitializeCpuCommands ();
  //
  // Install driver model protocol(s).
  //
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gNvdimmBlockIoDriverBinding,
             ImageHandle,
             &gNvdimmBlockIoComponentName,
             &gNvdimmBlockIoComponentName2
             );
  ASSERT_EFI_ERROR (Status);


  return Status;
}
