/** @file
  This is a simple shell application

  Copyright (c) 2008 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Protocol/ShellParameters.h>
#include <Protocol/LoadedImage.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/ShellParametersLib.h>
#include <Library/UefiBootServicesTableLib.h>

CONST SHELL_FLAG_ITEM mParams[] = {
  { L"-s", FlagTypeSwitch },
  { L"-t", FlagTypeTimeValue},
  { L"-v", FlagTypeValue},
  { L"-dv", FlagTypeDoubleValue},
  { L"-mv", FlagTypeMaxValue},
  { L"-p", FlagTypeStart},
  { NULL, 0}
};


/**
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  LIST_ENTRY Parameters;
  CONST SHELL_FLAG_ITEM *P;
  CHAR16 *ProblemParam;
  UINTN Index;
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, &LoadedImage);
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_ERROR, "ARG = '%s'\n", LoadedImage->LoadOptions));

  Status = gBS->HandleProtocol (ImageHandle, &gEfiShellParametersProtocolGuid, &ShellParameters);
  ASSERT_EFI_ERROR (Status);

  Status = ShellParametersParse (ShellParameters->Argv, ShellParameters->Argc, mParams, &Parameters, &ProblemParam, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "error: %r, Problem = %s\n", Status, ProblemParam));
    return Status;
  }

  for (P = mParams; P->Name != NULL; P++) {
    DEBUG ((DEBUG_ERROR, "%s = (%d)%s\n", P->Name, ShellParametersGetFlag (&Parameters, P->Name), ShellParametersGetFlagValue (&Parameters, P->Name)));
  }

  for (Index = 0; Index < ShellParametersGetPositionValueCount (&Parameters); Index++) {
    DEBUG ((DEBUG_ERROR, "[%d] = %s\n", Index, ShellParametersGetPositionValue (&Parameters, Index)));
  }

  return EFI_SUCCESS;
}
