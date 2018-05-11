/** @file
  Provides semaphore library implementation for PEI phase.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD
License which accompanies this distribution.  The full text of the license may
be found at http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiPei.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include "PeiSemaphoreLib.h"

GUID  mSemaphoreLibEmptyGuid = { 0xa883791c, 0x79b0, 0x417c,{ 0xb6, 0xec, 0x29, 0xd5, 0x71, 0xba, 0xa3, 0x2f } };

/**
  Create a semaphore.

  This function creates or opens a named semaphore.
  It must be called by BSP because it uses the HOB service.
  TODO: AP needs to call this to get pre-created semaphore. We need to guarantee it AP callable in a certain level.

  @param  Name         Guided name. If Name matches the name of an existing semaphore, the InitialCount if ignored
                       because it has already been set by the creating process.
  @param  Semaphore    Return the semaphore of an existing semaphore or a new created semaphore.
  @param  InitialCount The count of resources available for the semaphore.
                       Consumer can supply 1 to use semaphore as a mutex to protect a critical section.

  @retval RETURN_SUCCESS           The semaphore is created or opened successfully.
  @retval RETURN_INVALID_PARAMETER Semaphore is NULL.
                                   Name equals to reserved GUID.
  @retval RETURN_OUT_OF_RESOURCES  There is no sufficient resource to create the semaphore.
**/
RETURN_STATUS
EFIAPI
SemaphoreCreate (
  IN  CONST GUID *Name,
  OUT SEMAPHORE  *Semaphore,
  IN  UINT32     InitialCount
  )
{
  EFI_HOB_GUID_TYPE   *Hob;
  SEMAPHORE_INSTANCE  *Instance;

  if (Semaphore == NULL) {
    return RETURN_INVALID_PARAMETER;
  }
  if (CompareGuid (Name, &mSemaphoreLibEmptyGuid)) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // Find the semaphore with the same Name.
  //
  Hob = GetFirstGuidHob (Name);
  if (Hob != NULL) {
    *Semaphore = Hob + 1;
    return RETURN_SUCCESS;
  }
  //
  // Till now, the above operation might be AP callable.
  // AP needs to have the same IDTR as that of BSP, for PeiServices pointer retrieval.
  //

  Hob = GetFirstGuidHob (&mSemaphoreLibEmptyGuid);
  if (Hob != NULL) {
    //
    // Use the recyled storage.
    //
    CopyGuid (&Hob->Name, Name);
    Instance = (SEMAPHORE_INSTANCE *)(Hob + 1);
  } else {
    Instance = BuildGuidHob (Name, sizeof (*Instance));
    if (Instance == NULL) {
      return RETURN_OUT_OF_RESOURCES;
    }
  }
  Instance->Signature = SEMAPHORE_SIGNATURE;
  Instance->Count     = InitialCount;
  *Semaphore = Instance;
  return RETURN_SUCCESS;
}

/**
  Destroy a semaphore.

  This function destroys the semaphore.
  It must be called by BSP because it uses the HOB service.

  @param  Semaphore             The semaphore to destroy.

  @retval RETURN_SUCCESS           The semaphore is destroyed successfully.
  @retval RETURN_INVALID_PARAMETER The semaphore is not created by SemaphoreCreate().

**/
RETURN_STATUS
EFIAPI
SemaphoreDestroy (
  IN SEMAPHORE Semaphore
)
{
  EFI_HOB_GUID_TYPE   *Hob;
  SEMAPHORE_INSTANCE  *Instance;

  Instance = (SEMAPHORE_INSTANCE *)Semaphore;
  if ((Instance == NULL) || (Instance->Signature != SEMAPHORE_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  for ( Hob = GetFirstHob (EFI_HOB_TYPE_GUID_EXTENSION)
      ; Hob != NULL
      ; Hob = GetNextHob (EFI_HOB_TYPE_GUID_EXTENSION, GET_NEXT_HOB (Hob))) {
    if (Hob + 1 == Semaphore) {
      break;
    }
  }

  if (Hob == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // Mark the GUIDed HOB using EMPTY GUID so that next SemaphoreCreate() can re-use the same storage.
  //
  CopyGuid (&Hob->Name, &mSemaphoreLibEmptyGuid);
  return RETURN_SUCCESS;
}