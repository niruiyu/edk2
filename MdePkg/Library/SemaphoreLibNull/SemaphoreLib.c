/** @file
  Provides NULL semaphore library implementation.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD
License which accompanies this distribution.  The full text of the license may
be found at http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/SemaphoreLib.h>

/**
  Acquire a semaphore.

  This function checks the count of the semaphore.
  If the count is non-zero, it is decremented and the routine returns.
  If the count is zero, the routine waits specified timeout for the count to be non-zero, and then places it in the
  acquired state.

  @param  Semaphore             The semaphore to acquire.
  @param  TimeoutInMicroSeconds The timeout in micro seconds.
                                0 to wait infinitely.

  @retval RETURN_SUCCESS           The semaphore is acquired successfully.
  @retval RETURN_INVALID_PARAMETER The semaphore is not created by SemaphoreCreate().
  @retval RETURN_TIMEOUT           The semaphore cannot be acquired in specified timeout.
**/
RETURN_STATUS
EFIAPI
SemaphoreAcquire (
  IN SEMAPHORE Semaphore,
  IN UINT64    TimeoutInMicroSeconds
  )
{
  return RETURN_SUCCESS;
}

/**
  Attempts acquire a semaphore.

  This function checks the count of the semaphore.
  If the count is non-zero, it is decreased and the routine returns.
  If the count is zero, the routine returns RETURN_NOT_READY.

  @param  Semaphore             The semaphore to acquire.

  @retval RETURN_SUCCESS           The semaphore is acquired successfully.
  @retval RETURN_INVALID_PARAMETER The semaphore is not created by SemaphoreCreate().

**/
RETURN_STATUS
EFIAPI
SemaphoreAcquireOrFail (
  IN SEMAPHORE Semaphore
  )
{
  return RETURN_SUCCESS;
}

/**
  Release a semaphore.

  This function decreases the count of the semaphore.

  @param  Semaphore             The semaphore to release.

  @retval RETURN_SUCCESS           The semaphore is released successfully.
  @retval RETURN_INVALID_PARAMETER The semaphore is not created by SemaphoreCreate().

**/
RETURN_STATUS
EFIAPI
SemaphoreRelease (
  IN SEMAPHORE Semaphore
  )
{
  return RETURN_SUCCESS;
}

/**
  Create a semaphore.

  This function creates or opens a named semaphore.
  It must be called by BSP because it uses the HOB service.

  If Semaphore is NULL, then ASSERT().

  @param  Name         Guided name. If Name matches the name of an existing semaphore, the InitialCount if ignored
                       because it has already been set by the creating process.
  @param  Semaphore    Return the semaphore of an existing semaphore or a new created semaphore.
  @param  InitialCount The count of resources available for the semaphore.
                       Consumer can supply 1 to use semaphore as a mutex to protect a critical section.

  @retval RETURN_SUCCESS           The semaphore is created or opened successfully.
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
  return RETURN_SUCCESS;
}