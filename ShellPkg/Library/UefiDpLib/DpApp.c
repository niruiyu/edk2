/** @file
  Main file for NULL named library for install1 shell command functions.

  Copyright (c) 2010 - 2013, Intel Corporation. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "UefiDpLib.h"
#include <Protocol/HiiDatabase.h>

EFI_HANDLE gDpHiiHandle = NULL;

#define DP_HII_GUID \
  { \
  0xeb832fd9, 0x9089, 0x4898, { 0x83, 0xc9, 0x41, 0x61, 0x8f, 0x5c, 0x48, 0xb9 } \
  }

EFI_GUID gDpHiiGuid = DP_HII_GUID;
//
// String token ID of help message text.
// Shell supports to find help message in the resource section of an application image if
// .MAN file is not found. This global variable is added to make build tool recognizes
// that the help string is consumed by user and then build tool will add the string into
// the resource section. Thus the application can use '-?' option to show help message in
// Shell.
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_STRING_ID mStringHelpTokenId = STRING_TOKEN (STR_GET_HELP_DP);

/**
  Constructor for the Shell Level 1 Commands library.

  Install the handlers for level 1 UEFI Shell 2.0 commands.

  @param ImageHandle    the image handle of the process
  @param SystemTable    the EFI System Table pointer

  @retval EFI_SUCCESS        the shell command handlers were installed sucessfully
  @retval EFI_UNSUPPORTED    the shell level required was not found.
**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  SHELL_STATUS                ShellStatus;
  EFI_STATUS                  Status;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList;
  EFI_HII_DATABASE_PROTOCOL   *HiiDatabase;

  Status = gBS->LocateProtocol (&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **) &HiiDatabase);
  if (EFI_ERROR (Status)) {
    return SHELL_UNSUPPORTED;
  }

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **) &PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return SHELL_NOT_FOUND;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = HiiDatabase->NewPackageList (
                         HiiDatabase,
                         PackageList,
                         NULL,
                         &gDpHiiHandle
                         );
  if (EFI_ERROR (Status)) {
    return SHELL_DEVICE_ERROR;
  }
  

  ShellStatus = ShellCommandRunDp (ImageHandle, SystemTable);
  HiiRemovePackages(gDpHiiHandle);

  return ShellStatus;
}

/**
  Unloads the application and its installed protocol.

  @param[in]  ImageHandle       Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
**/
EFI_STATUS
EFIAPI
DpUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  if (gDpHiiHandle != NULL) {
    HiiRemovePackages (gDpHiiHandle);
    gDpHiiHandle = NULL;
  }

  return EFI_SUCCESS;
}

