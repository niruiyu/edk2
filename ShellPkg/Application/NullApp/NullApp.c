/** @file
  Generic standalone application.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>

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
  UINTN                Index;
  SHELL_STATUS         Status;
  CONST COMMAND_LIST   *Commands;
  COMMAND_LIST         *Command;
  CHAR16               *CommandPath;
  CHAR16               *CommandName;

  CommandPath = AllocateCopyPool (
                    StrSize (gEfiShellParametersProtocol->Argv[0]),
                    gEfiShellParametersProtocol->Argv[0]
                    );
  if (CommandPath == NULL) {
    return SHELL_OUT_OF_RESOURCES;
  }
  
  for (Index = 0; Index < gEfiShellParametersProtocol->Argc; Index++) {
    Print (L"%d: %s\n", Index, gEfiShellParametersProtocol->Argv[Index]);
  }

  Status = SHELL_NOT_FOUND;
  Commands = ShellCommandGetCommandList (FALSE);
  Command = (COMMAND_LIST *) GetFirstNode (&Commands->Link);
  if (!IsNull (&Commands->Link, &Command->Link)) {
    ASSERT(Command->CommandString != NULL);
    ShellCommandRunCommandHandler (Command->CommandString, &Status, NULL);
  }
  return Status;
}

