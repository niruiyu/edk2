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

#ifndef __MUTEX_LIB__
#define __MUTEX_LIB__

///
/// Definitions for MUTEX
///
#pragma pack(1)
typedef struct {
  volatile UINT32      LockCount;
  volatile UINT32      Owner;
} MUTEX;
#pragma pack()

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
  IN MUTEX  *Mutex,
  IN UINT64 TimeoutInMicroSeconds
  );

/**
  Attempts acquire a mutex.

  If the mutex is unlocked, it is locked and the routine returns.
  If the mutex is locked by the caller thread, its lock reference count increases and the routine returns.
  If the mutex is locked by other thread, the routine returns RETURN_NOT_READY.

  @param  Mutex             The mutex to acquire.

  @retval TRUE              The mutex is acquired successfully.
  @retval FALSE             The mutex cannot be acquired.
**/
BOOLEAN
EFIAPI
MutexAcquireOrFail (
  IN MUTEX *Mutex
  );

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
  IN MUTEX *Mutex
  );

/**
  Create a mutex.

  This function initializes a mutex.

  ASSERT () if Mutex is NULL.

  @param  Mutex        The mutex to be initialized.
**/
VOID
EFIAPI
MutexInitialize (
  IN OUT MUTEX  *Mutex
  );

#endif
