/** @file
  QEMU instance of Platform Sec Lib.

  Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

//
// The package level header files this module uses
//
#include "PlatformSecLib.h"

/**
  Perform those platform specific operations that are requried to be executed as early as possibile.

  @return TRUE always return true.
**/
EFI_STATUS
EFIAPI
PlatformSecLibConstructor (
  )
{
  return EFI_SUCCESS;
}

/**
  This function check the signture of UPD

  @param[in]  ApiIdx           Internal index of the FSP API.
  @param[in]  ApiParam         Parameter of the FSP API.

**/

EFI_STATUS
EFIAPI
FspUpdSignatureCheck (
  IN UINT32   ApiIdx,
  IN VOID     *ApiParam
  )
{
  EFI_STATUS    Status;
  FSPM_UPD      *FspmUpd;
  FSPS_UPD      *FspsUpd;

  Status = EFI_SUCCESS;
  FspmUpd = NULL;
  FspsUpd = NULL;

  if (ApiIdx == FspMemoryInitApiIndex) {
    //
    // FspMemoryInit check
    //
    FspmUpd = (FSPM_UPD *)ApiParam;
    if (FspmUpd != NULL) {
      if ((FspmUpd->FspUpdHeader.Signature != FSPM_UPD_SIGNATURE)
        || ((UINTN)FspmUpd->FspmArchUpd.StackBase == 0 )
        || ((FspmUpd->FspmArchUpd.BootLoaderTolumSize % EFI_PAGE_SIZE) != 0)) {
        Status = EFI_INVALID_PARAMETER;
      }
    }
  } else if (ApiIdx == FspSiliconInitApiIndex) {
    //
    // FspSiliconInit check
    //
    FspsUpd = (FSPS_UPD *)ApiParam;
    if (FspsUpd != NULL) {
      if (FspsUpd->FspUpdHeader.Signature != FSPS_UPD_SIGNATURE) {
        Status = EFI_INVALID_PARAMETER;
      }
    }
  }

  return Status;
}


/**
  FSP MultiPhase Platform Get Number Of Phases Function.

  Allows an FSP binary to dynamically update the number of phases at runtime.
  For example, UPD settings could negate the need to enter the multi-phase flow
  in certain scenarios. If this function returns FALSE, the default number of phases
  provided by PcdMultiPhaseNumberOfPhases will be returned to the bootloader instead.

  @param[in] ApiIdx                  - Internal index of the FSP API.
  @param[in] NumberOfPhasesSupported - How many phases are supported by current FSP Component.

  @retval  TRUE  - NumberOfPhases are modified by Platform during runtime.
  @retval  FALSE - The Default build time NumberOfPhases should be used.

**/
BOOLEAN
EFIAPI
FspMultiPhasePlatformGetNumberOfPhases (
  IN     UINT8   ApiIdx,
  IN OUT UINT32  *NumberOfPhasesSupported
  )
{
  return FALSE;
}
