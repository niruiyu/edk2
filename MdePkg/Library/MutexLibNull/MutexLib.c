/** @file
  Provides mutex functions.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD
License which accompanies this distribution.  The full text of the license may
be found at http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>
#include <Library/MutexLib.h>

/**
  Acquire a mutex.

  If the mutex is unlocked, it is locked and the routine returns.
  If the mutex is locked by the caller thread, its lock reference count increases and the routine returns.
  If the mutex is locked by other thread, the routine waits specified timeout for mutex to be unlocked, and then lock
  it.

  @param  Mutex                 The mutex to acquire.
  @param  TimeoutInMicroSeconds The timeout in micro seconds.
                                0 to wait infinitely.

  @retval RETURN_SUCCESS           The mutex is acquired successfully.
  @retval RETURN_INVALID_PARAMETER The mutex is not created by MutexCreate().
  @retval RETURN_TIMEOUT           The mutex cannot be acquired in specified timeout.
**/
RETURN_STATUS
EFIAPI
MutexAcquire (
  IN MUTEX  Mutex,
  IN UINT64 TimeoutInMicroSeconds
  )
{
  return RETURN_SUCCESS;
}

/**
  Attempts acquire a mutex.

  If the mutex is unlocked, it is locked and the routine returns.
  If the mutex is locked by the caller thread, its lock reference count increases and the routine returns.
  If the mutex is locked by other thread, the routine returns RETURN_NOT_READY.

  @param  Mutex             The mutex to acquire.

  @retval RETURN_SUCCESS           The mutex is acquired successfully.
  @retval RETURN_INVALID_PARAMETER The mutex is not created by MutexCreate().

**/
RETURN_STATUS
EFIAPI
MutexAcquireOrFail (
  IN MUTEX Mutex
  )
{
  return RETURN_SUCCESS;
}

/**
  Release a mutex.

  This function unlocks mutex.

  @param  Mutex             The mutex to release.

  @retval RETURN_SUCCESS           The mutex is released successfully.
  @retval RETURN_INVALID_PARAMETER The mutex is not created by MutexCreate().

**/
RETURN_STATUS
EFIAPI
MutexRelease (
  IN MUTEX Mutex
  )
{
  return RETURN_SUCCESS;
}

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
  OUT MUTEX  *Mutex
  )
{
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
  return RETURN_SUCCESS;
}
