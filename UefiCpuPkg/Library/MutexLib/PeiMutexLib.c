/** @file
  Provides mutex library implementation for PEI phase.

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
#include "PeiMutexLib.h"
#include <Library/SerialPortLib.h>
#include <Library/PrintLib.h>

GUID  mMutexLibEmptyGuid = { 0x822aa30e, 0xc79, 0x49c7,{ 0xb8, 0x92, 0x60, 0xbb, 0x4b, 0x58, 0xf1, 0xf6 } };

/**
  Create a mutex.

  This function creates or opens a named mutex.
  It must be called by BSP because it uses the HOB service.

  If Mutex is NULL, then ASSERT().

  @param  Name         Guided name.
  @param  Mutex        Return the mutex of an existing mutex or a new created mutex.

  @retval RETURN_SUCCESS           The mutex is created or opened successfully.
  @retval RETURN_OUT_OF_RESOURCES  There is no sufficient resource to create the mutex.
**/
RETURN_STATUS
EFIAPI
MutexCreate (
  IN  CONST GUID *Name,
  OUT MUTEX      *Mutex
  )
{
  EFI_HOB_GUID_TYPE   *Hob;
  MUTEX_INSTANCE      *Instance;

  if (Mutex == NULL) {
    return RETURN_INVALID_PARAMETER;
  }
  if (CompareGuid (Name, &mMutexLibEmptyGuid)) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // Find the mutex with the same Name.
  //
  Hob = GetFirstGuidHob (Name);
  if (Hob != NULL) {
    *Mutex = Hob + 1;
    return RETURN_SUCCESS;
  }
  //
  // Till now, the above operation might be AP callable.
  // AP needs to have the same IDTR as that of BSP, for PeiServices pointer retrieval.
  //

  Hob = GetFirstGuidHob (&mMutexLibEmptyGuid);
  if (Hob != NULL) {
    //
    // Use the recyled storage.
    //
    CopyGuid (&Hob->Name, Name);
    Instance = (MUTEX_INSTANCE *)(Hob + 1);
  } else {
    Instance = BuildGuidHob (Name, sizeof (*Instance));
    if (Instance == NULL) {
      return RETURN_OUT_OF_RESOURCES;
    }
  }
  Instance->Signature     = MUTEX_SIGNATURE;
  Instance->OwnerAndCount = MUTEX_RELEASED;
  *Mutex = Instance;
  return RETURN_SUCCESS;
}

/**
  Destroy a mutex.

  This function destroys the mutex.
  It must be called by BSP because it uses the HOB service.

  @param  Mutex             The mutex to destroy.

  @retval RETURN_SUCCESS           The mutex is destroyed successfully.
  @retval RETURN_INVALID_PARAMETER The mutex is not created by MutexCreate().

**/
RETURN_STATUS
EFIAPI
MutexDestroy (
  IN MUTEX Mutex
  )
{
  EFI_HOB_GUID_TYPE   *Hob;
  MUTEX_INSTANCE      *Instance;

  Instance = (MUTEX_INSTANCE *)Mutex;
  if ((Instance == NULL) || (Instance->Signature != MUTEX_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  for ( Hob = GetFirstHob (EFI_HOB_TYPE_GUID_EXTENSION)
      ; Hob != NULL
      ; Hob = GetNextHob (EFI_HOB_TYPE_GUID_EXTENSION, GET_NEXT_HOB (Hob))) {
    if (Hob + 1 == Mutex) {
      break;
    }
  }

  if (Hob == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // Mark the GUIDed HOB using EMPTY GUID so that next MutexCreate() can re-use the same storage.
  //
  CopyGuid (&Hob->Name, &mMutexLibEmptyGuid);
  return RETURN_SUCCESS;
}
