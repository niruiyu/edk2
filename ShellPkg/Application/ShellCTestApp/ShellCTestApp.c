/** @file
  This is a test application that demonstrates how to use the C-style entry point
  for a shell application.

  Copyright (c) 2009 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/IoLib.h>
#include <Library/BaseLib.h>

/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  The ShellCEntryLib library instance wrappers the actual UEFI application
  entry point and calls this ShellAppMain function.

  @param[in] Argc     The number of items in Argv.
  @param[in] Argv     Array of pointers to strings.

  @retval  0               The application exited normally.
  @retval  Other           An error occurred.

**/
INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
  UINTN  Index;
  UINT64 Tick1, Tick2;
  if (Argc == 1) {
    Print (L"Argv[1] = NULL\n");
  }
  for (Index = 1; Index < Argc; Index++) {
    Print(L"Argv[%d]: \"%s\"\n", Index, Argv[Index]);
  }

  
  for (Index = 0; Index < 10; Index++) {
    Tick1 = AsmReadTsc ();
    IoWrite8 (0xB2, 0);
    Tick2 = AsmReadTsc ();
    Print(L"=======================\n");
    Print(L"S:%16lx\n", Tick1);
    Print(L"E:%16lx\n", Tick2);
  }
  return 0;
}
