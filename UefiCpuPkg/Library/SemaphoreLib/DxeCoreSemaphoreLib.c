/** @file
  Provides semaphore library implementation for DXE phase.

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
#include "DxeSemaphoreLib.h"

GUID           mSemaphoreLibDatabaseGuid = SEMAPHORE_LIB_DATABASE_GUID;
SEMAPHORE_LIST *mSemaphoreLibList           = NULL;

#define MAX_GLOBAL_SEMAPHORE_ENTRY  10
SEMAPHORE_LIST       mSemaphoreListHead;
SEMAPHORE_LIST_ENTRY mSemaphoreLibListEntry[MAX_GLOBAL_SEMAPHORE_ENTRY];
UINTN                mSemaphoreLibListEntryUsedCount = 0;

EFI_STATUS
EFIAPI
DxeCoreSemaphoreLibConstructor (
  IN EFI_HANDLE                         ImageHandle,
  IN EFI_SYSTEM_TABLE                   *SystemTable
)
{
  EFI_STATUS      Status;

  if (mSemaphoreLibList == NULL) {
    //
    // SemaphoreCreate() isn't called before constructor.
    //
    mSemaphoreLibList = &mSemaphoreListHead;
    InitializeListHead (&mSemaphoreLibList->List);
    InitializeSpinLock (&mSemaphoreLibList->Lock);
  }
  Status = SystemTable->BootServices->InstallConfigurationTable (&mSemaphoreLibDatabaseGuid, mSemaphoreLibList);
  if (RETURN_ERROR (Status)) {
    return EFI_OUT_OF_RESOURCES;
  }
  return EFI_SUCCESS;
}

/**
  Create a semaphore.

  This function creates the semaphore.
  It can be called by BSP and AP.

  If Semaphore is NULL, then ASSERT().

  @param  Name         Guided name.
  @param  Semaphore    Return the semaphore.
  @param  InitialCount The count of resources available for the semaphore.
                       Consumer can supply 1 to use semaphore as a mutex to protect a critical section.

  @retval RETURN_SUCCESS           The semaphore is created successfully.
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
  LIST_ENTRY           *Link;
  SEMAPHORE_LIST_ENTRY *Entry;

  if (Semaphore == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // mSemaphoreLibList is NULL when called from DxeCore before constructor list is processed.
  //
  if (mSemaphoreLibList == NULL) {
    mSemaphoreLibList = &mSemaphoreListHead;
    InitializeListHead (&mSemaphoreLibList->List);
    InitializeSpinLock (&mSemaphoreLibList->Lock);
  }

  AcquireSpinLock (&mSemaphoreLibList->Lock);
  //
  // Find the semaphore with the same Name.
  //
  for ( Link = GetFirstNode (&mSemaphoreLibList->List)
      ; !IsNull (&mSemaphoreLibList->List, Link)
      ; Link = GetNextNode (&mSemaphoreLibList->List, Link)) {
    Entry = SEMAPHORE_LIST_ENTRY_FROM_LINK (Link);
    if (CompareGuid (&Entry->Name, Name)) {
      *Semaphore = &Entry->Instance;
      ReleaseSpinLock (&mSemaphoreLibList->Lock);
      return RETURN_SUCCESS;
    }
  }

  //
  // We assume DXE CORE only needs maximum MAX_GLOBAL_SEMAPHORE_ENTRY semaphore objects.
  //
  if (mSemaphoreLibListEntryUsedCount == MAX_GLOBAL_SEMAPHORE_ENTRY) {
    Entry = NULL;
  } else {
    Entry = &mSemaphoreLibListEntry[mSemaphoreLibListEntryUsedCount++];
    Entry->Allocated = FALSE;
    Entry->Instance.Signature = SEMAPHORE_SIGNATURE;
    Entry->Instance.Count     = InitialCount;
    CopyGuid (&Entry->Name, Name);
    InsertTailList (&mSemaphoreLibList->List, &Entry->Link);
    *Semaphore = &Entry->Instance;
  }
  ReleaseSpinLock (&mSemaphoreLibList->Lock);

  if (Entry == NULL) {
    return RETURN_OUT_OF_RESOURCES;
  }
  return RETURN_SUCCESS;
}

/**
  Destroy a semaphore.

  This function destroys the semaphore.
  It can be called by BSP and AP.

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
  LIST_ENTRY           *Link;
  SEMAPHORE_LIST_ENTRY *Entry;
  SEMAPHORE_INSTANCE   *Instance;

  Instance = (SEMAPHORE_INSTANCE *)Semaphore;
  if ((Instance == NULL) || (Instance->Signature != SEMAPHORE_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // Find the semaphore to destroy.
  //
  Entry = NULL;
  AcquireSpinLock (&mSemaphoreLibList->Lock);
  for ( Link = GetFirstNode (&mSemaphoreLibList->List)
      ; !IsNull (&mSemaphoreLibList->List, Link)
      ; Link = GetNextNode (&mSemaphoreLibList->List, Link)
      ) {
    Entry = SEMAPHORE_LIST_ENTRY_FROM_LINK (Link);
    if (Instance == &Entry->Instance) {
      RemoveEntryList (Link);
      break;
    }
  }
  ReleaseSpinLock (&mSemaphoreLibList->Lock);

  if (Entry == NULL) {
    return RETURN_INVALID_PARAMETER;
  }
  if (Entry->Allocated) {
    FreePool (Entry);
  }
  return RETURN_SUCCESS;
}