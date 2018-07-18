/** @file
  UEFI Component Name(2) protocol implementation for NvdimmBlockIo driver.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "NvdimmBlockIoDxe.h"

//
// Driver name table for DiskIo module.
// It is shared by the implementation of ComponentName & ComponentName2 Protocol.
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mNvdimmBlockIoDriverNameTable[] = {
  {
    "eng;en",
    (CHAR16 *)L"Generic NVDIMM Block I/O Driver"
  },
  {
    NULL,
    NULL
  }
};



/**
  Retrieves a Unicode string that is the user readable name of the driver.

  This function retrieves the user readable name of a driver in the form of a
  Unicode string. If the driver specified by This has a user readable name in
  the language specified by Language, then a pointer to the driver name is
  returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
  by This does not support the language specified by Language,
  then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language. This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified
                                in RFC 4646 or ISO 639-2 language code format.

  @param  DriverName[out]       A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                driver specified by This in the language
                                specified by Language.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by
                                This and the language specified by Language was
                                returned in DriverName.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER DriverName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mNvdimmBlockIoDriverNameTable,
           DriverName,
           (BOOLEAN)(This == &gNvdimmBlockIoComponentName)
           );
}



/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver.

  This function retrieves the user readable name of the controller specified by
  ControllerHandle and ChildHandle in the form of a Unicode string. If the
  driver specified by This has a user readable name in the language specified by
  Language, then a pointer to the controller name is returned in ControllerName,
  and EFI_SUCCESS is returned.  If the driver specified by This is not currently
  managing the controller specified by ControllerHandle and ChildHandle,
  then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
  support the language specified by Language, then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  ControllerHandle[in]  The handle of a controller that the driver
                                specified by This is managing.  This handle
                                specifies the controller whose name is to be
                                returned.

  @param  ChildHandle[in]       The handle of the child controller to retrieve
                                the name of.  This is an optional parameter that
                                may be NULL.  It will be NULL for device
                                drivers.  It will also be NULL for a bus drivers
                                that wish to retrieve the name of the bus
                                controller.  It will not be NULL for a bus
                                driver that wishes to retrieve the name of a
                                child controller.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language.  This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified in
                                RFC 4646 or ISO 639-2 language code format.

  @param  ControllerName[out]   A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                controller specified by ControllerHandle and
                                ChildHandle in the language specified by
                                Language from the point of view of the driver
                                specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in
                                the language specified by Language for the
                                driver specified by This was returned in
                                DriverName.

  @retval EFI_INVALID_PARAMETER ControllerHandle is NULL.

  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid
                                EFI_HANDLE.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER ControllerName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by
                                ControllerHandle and ChildHandle.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
NvdimmBlockIoComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle        OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  )
{
  EFI_STATUS                       Status;
  EFI_BLOCK_IO_PROTOCOL            *BlockIo;
  NVDIMM_NAMESPACE                 *Namespace;

  //
  // Make sure this driver is currently managing ControllHandle
  //
  Status = EfiTestManagedDevice (
             ControllerHandle,
             gNvdimmBlockIoDriverBinding.DriverBindingHandle,
             &gEfiNvdimmLabelProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // This is a bus driver, so ChildHandle can not be NULL.
  //
  if (ChildHandle == NULL) {
    return EFI_UNSUPPORTED;
  }

  Status = EfiTestChildHandle (
             ControllerHandle,
             ChildHandle,
             &gEfiNvdimmLabelProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get our context back
  //
  Status = gBS->OpenProtocol (
                  ChildHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo,
                  gNvdimmBlockIoDriverBinding.DriverBindingHandle,
                  ChildHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  Namespace = NVDIMM_NAMESPACE_FROM_BLOCK_IO (BlockIo);

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           Namespace->ControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &gNvdimmBlockIoComponentName)
           );

}

#define NVDIMM_NAMESPACE_NAME_FMT  L"NVDIMM %a Namespace[%g]"
#define NVDIMM_NAMESPACE_NAME_LEN  sizeof ("NVDIMM PMEM Namespace[########-####-####-####-############]")

/**
  Initialize the controller name table for ComponentName(2).

  @param Namespace The NVDIMM Namespace instance.

  @retval EFI_SUCCESS          The controller name table is initialized.
  @retval EFI_OUT_OF_RESOURCES There is not enough memory to initialize the controller name table.
**/
EFI_STATUS
InitializeComponentName (
  IN NVDIMM_NAMESPACE  *Namespace
  )
{
  EFI_STATUS           Status;
  CHAR16               ControllerName[NVDIMM_NAMESPACE_NAME_LEN];

  UnicodeSPrint (
    ControllerName, sizeof (ControllerName),
    NVDIMM_NAMESPACE_NAME_FMT,
    (Namespace->Type == NamespaceTypePmem) ? "PMEM" : "BLK",
    &Namespace->Uuid
  );

  Namespace->ControllerNameTable = NULL;
  Status = AddUnicodeString2 (
    "eng",
    gNvdimmBlockIoComponentName.SupportedLanguages,
    &Namespace->ControllerNameTable,
    ControllerName,
    TRUE
  );
  if (!EFI_ERROR (Status)) {
    Status = AddUnicodeString2 (
      "en",
      gNvdimmBlockIoComponentName2.SupportedLanguages,
      &Namespace->ControllerNameTable,
      ControllerName,
      FALSE
    );
    if (EFI_ERROR (Status)) {
      FreeUnicodeStringTable (Namespace->ControllerNameTable);
      Namespace->ControllerNameTable = NULL;
    }
  }
  return Status;
}

//
// EFI Component Name Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL  gNvdimmBlockIoComponentName = {
  NvdimmBlockIoComponentNameGetDriverName,
  NvdimmBlockIoComponentNameGetControllerName,
  "eng"
};

//
// EFI Component Name 2 Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gNvdimmBlockIoComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME)NvdimmBlockIoComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME)NvdimmBlockIoComponentNameGetControllerName,
  "en"
};
