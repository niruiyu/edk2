/** @file
  Provides base mutex library implementation.

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
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

typedef struct {
  UINT32               RegisterId;
  UINT32               Padding;
} MUTEX_HW;

MUTEX_HW mMutex = { FixedPcdGet32 (PcdMutexHardwareId) };

/**
  Attempts acquire a mutex.

  This function checks the count of the mutex.
  If the count is non-zero, it is decreased and the routine returns.
  If the count is zero, the routine returns RETURN_NOT_READY.

  @param  Mutex             The mutex to acquire.

  @retval TRUE              The mutex is acquired successfully.
  @retval FALSE             The mutex cannot be acquired.
**/
BOOLEAN
EFIAPI
MutexAcquireOrFail (
  IN MUTEX *Mutex
  )
{
  ASSERT (CompareMem (Mutex, &mMutex, sizeof (MUTEX)) == 0);
  //
  // TODO: Operate on the hardware register identified by mMutex.RegisterId.
  //
  return TRUE;
}

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
  @retval RETURN_TIMEOUT           The mutex cannot be acquired in specified timeout.
**/
RETURN_STATUS
EFIAPI
MutexAcquire (
  IN MUTEX  *Mutex,
  IN UINT64 TimeoutInMicroSeconds
  )
{
  ASSERT (CompareMem (Mutex, &mMutex, sizeof (MUTEX)) == 0);
  //
  // TODO: Operate on the hardware register identified by mMutex.RegisterId.
  //
  return RETURN_SUCCESS;
}

/**
  Release a mutex.

  This function decreases the count of the mutex.

  @param  Mutex             The mutex to release.

  @retval RETURN_SUCCESS           The mutex is released successfully.
  @retval RETURN_ACCESS_DENIED     The mutex is not owned by current thread.
**/
RETURN_STATUS
EFIAPI
MutexRelease (
  IN MUTEX *Mutex
  )
{
  ASSERT (CompareMem (Mutex, &mMutex, sizeof (MUTEX)) == 0);
  //
  // TODO: Operate on the hardware register identified by mMutex.RegisterId.
  //
  return RETURN_SUCCESS;
}

/**
  Initialize a mutex.

  If Mutex is NULL, then ASSERT().

  @param  Mutex        The mutex to be initialized.
**/
VOID
EFIAPI
MutexInitialize (
  IN OUT MUTEX  *Mutex
  )
{
  //
  // The initialization to mMutex is done through setting correct PCD value in DSC file
  // Just return the global static mMutex
  //
  ASSERT (sizeof (*Mutex) == sizeof (mMutex));
  CopyMem (Mutex, &mMutex, sizeof (MUTEX));
}
