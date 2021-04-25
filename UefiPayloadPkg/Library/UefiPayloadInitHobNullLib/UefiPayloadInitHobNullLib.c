/** @file
  This library retrieve the EFI_BOOT_SERVICES pointer from EFI system table in
  library's constructor.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include <Uefi.h>



EFI_STATUS
EFIAPI
UefiPayloadInitHobNullLibConstructor (

  )
{

  return EFI_SUCCESS;
}
