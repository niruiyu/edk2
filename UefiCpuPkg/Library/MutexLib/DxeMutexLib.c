/** @file
  Provides mutex library implementation for DXE phase.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD
License which accompanies this distribution.  The full text of the license may
be found at http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

// TODO: DXE version is super good now!! No gap!!!

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include "DxeMutexLib.h"

GUID       mMutexLibDatabaseGuid         = MUTEX_LIB_DATABASE_GUID;
MUTEX_LIST *mMutexLibList                = NULL;

EFI_STATUS
EFIAPI
MutexLibConstructor (
  IN EFI_HANDLE                         ImageHandle,
  IN EFI_SYSTEM_TABLE                   *SystemTable
  )
{
  EFI_STATUS      Status;
  UINTN           Index;

  for (Index = 0; Index < SystemTable->NumberOfTableEntries; Index++) {
    if (CompareGuid (&mMutexLibDatabaseGuid, &(SystemTable->ConfigurationTable[Index].VendorGuid))) {
      mMutexLibList = SystemTable->ConfigurationTable[Index].VendorTable;
    }
  }

  if (mMutexLibList == NULL) {
    //
    // In whole DXE phase, the below code is called only once from the first module which links to MutexLib.
    // Other modules who link to MutexLib don't call it again.
    //
    mMutexLibList = AllocatePool (sizeof (*mMutexLibList));
    if (mMutexLibList == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    InitializeListHead (&mMutexLibList->List);
    InitializeSpinLock (&mMutexLibList->Lock);
    Status = SystemTable->BootServices->InstallConfigurationTable (&mMutexLibDatabaseGuid, mMutexLibList);
    if (RETURN_ERROR (Status)) {
      FreePool (mMutexLibList);
      return EFI_OUT_OF_RESOURCES;
    }
  }
  return EFI_SUCCESS;
}

/**
  Create a mutex.

  This function creates or opens a named or nameless mutex.
  It can be called by BSP and AP.

  @param  Name         Guided name.
  @param  Mutex        Return an existing mutex or a new created mutex.

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
  LIST_ENTRY           *Link;
  MUTEX_LIST_ENTRY     *Entry;
  MUTEX_INSTANCE       *Instance;

  if (Mutex == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Name == NULL) {
    if (!MutexLibIsBsp ()) {
      return RETURN_UNSUPPORTED;
    }
    Instance = AllocatePool (sizeof (*Instance));
    if (Instance == NULL) {
      return RETURN_OUT_OF_RESOURCES;
    }
    Instance->Signature     = MUTEX_SIGNATURE;
    Instance->OwnerAndCount = MUTEX_RELEASED;
    *Mutex                  = Instance;
    return RETURN_SUCCESS;
  }

  AcquireSpinLock (&mMutexLibList->Lock);
  //
  // Find the mutex with the same Name.
  //
  for ( Link = GetFirstNode (&mMutexLibList->List)
      ; !IsNull (&mMutexLibList->List, Link)
      ; Link = GetNextNode (&mMutexLibList->List, Link)) {
    Entry = MUTEX_LIST_ENTRY_FROM_LINK (Link);
    if (CompareGuid (&Entry->Name, Name)) {
      *Mutex = &Entry->Instance;
      ReleaseSpinLock (&mMutexLibList->Lock);
      return RETURN_SUCCESS;
    }
  }

  if (!MutexLibIsBsp ()) {
    ReleaseSpinLock (&mMutexLibList->Lock);
    return RETURN_UNSUPPORTED;
  }

  Entry = AllocatePool (sizeof (*Entry));
  if (Entry != NULL) {
    Entry->Allocated                        = TRUE;
    Entry->Instance.Signature               = MUTEX_SIGNATURE;
    Entry->Instance.OwnerAndCount           = MUTEX_RELEASED;
    CopyGuid (&Entry->Name, Name);
    InsertTailList (&mMutexLibList->List, &Entry->Link);
    *Mutex = &Entry->Instance;
  }
  ReleaseSpinLock (&mMutexLibList->Lock);

  if (Entry == NULL) {
    return RETURN_OUT_OF_RESOURCES;
  }
  return RETURN_SUCCESS;
}

/**
  Destroy a mutex.

  This function destroys the mutex.
  It can only be called by BSP.

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
  LIST_ENTRY       *Link;
  MUTEX_LIST_ENTRY *Entry;
  MUTEX_INSTANCE   *Instance;

  Instance = (MUTEX_INSTANCE *)Mutex;
  if ((Instance == NULL) || (Instance->Signature != MUTEX_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (!MutexLibIsBsp ()) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Find the mutex to destroy.
  //
  Entry = NULL;
  AcquireSpinLock (&mMutexLibList->Lock);
  for ( Link = GetFirstNode (&mMutexLibList->List)
      ; !IsNull (&mMutexLibList->List, Link)
      ; Link = GetNextNode (&mMutexLibList->List, Link)
      ) {
    Entry = MUTEX_LIST_ENTRY_FROM_LINK (Link);
    if (Instance == &Entry->Instance) {
      RemoveEntryList (Link);
      break;
    }
  }
  ReleaseSpinLock (&mMutexLibList->Lock);

  if (Entry == NULL) {
    //
    // Assume it's a nameless mutex.
    //
    FreePool (Instance);
  } else if (Entry->Allocated) {
    FreePool (Entry);
  }
  return RETURN_SUCCESS;
}