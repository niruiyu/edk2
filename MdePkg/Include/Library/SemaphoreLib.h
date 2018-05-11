/** @file
  Provides semaphore functions.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD
License which accompanies this distribution.  The full text of the license may
be found at http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __SEMAPHORE_LIB__
#define __SEMAPHORE_LIB__

///
/// Definitions for SEMAPHORE
///
typedef volatile UINT32 SEMAPHORE;


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
  IN SEMAPHORE *Semaphore,
  IN UINT64    TimeoutInMicroSeconds
  );

/**
  Attempts acquire a semaphore.

  This function checks the count of the semaphore.
  If the count is non-zero, it is decreased and the routine returns.
  If the count is zero, the routine returns RETURN_NOT_READY.

  @param  Semaphore             The semaphore to acquire.

  @retval TRUE                  The semaphore is acquired successfully.
  @retval FALSE                 The semaphore cannot be acquired.
**/
BOOLEAN
EFIAPI
SemaphoreAcquireOrFail (
  IN SEMAPHORE *Semaphore
  );

/**
  Release a semaphore.

  This function decreases the count of the semaphore.

  @param  Semaphore             The semaphore to release.
**/
VOID
EFIAPI
SemaphoreRelease (
  IN SEMAPHORE *Semaphore
  );

/**
  Initialize a semaphore.

  If Semaphore is NULL, then ASSERT().

  @param  Name         Guided name. If Name matches the name of an existing semaphore, the InitialCount if ignored
                       because it has already been set by the creating process.
  @param  Semaphore    Return the semaphore of an existing semaphore or a new created semaphore.
  @param  InitialCount The count of resources available for the semaphore.
                       Consumer can supply 1 to use semaphore as a mutex to protect a critical section.
**/
VOID
EFIAPI
SemaphoreInitialize (
  IN OUT SEMAPHORE  *Semaphore,
  IN  UINT32        InitialCount
  );
#endif
