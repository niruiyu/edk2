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
#include <Ppi/MemoryDiscovered.h>
#include <Ppi/MpServices.h>
#include <Library/PeiServicesLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include "PeiMutexLib.h"

#define MUTEX_IDT_ENTRY_INDEX   35
#define MAX_INSTANCE            10

GUID mMutexLibDatabaseGuid = { 0x822aa30e, 0xc79, 0x49c7,{ 0xb8, 0x92, 0x60, 0xbb, 0x4b, 0x58, 0xf1, 0xf6 } };

typedef struct {
  SPIN_LOCK                Lock;
  NAMED_MUTEX_INSTANCE     NamedInstance[MAX_INSTANCE];
} MUTEX_ARRAY;

/**
  Return address map of exception handler template so that C code can generate
  exception tables.

  @param IdtEntry          Pointer to IDT entry to be updated.
  @param InterruptHandler  IDT handler value.

**/
VOID
MutexLibSetInterruptHandler (
  IN IA32_DESCRIPTOR                 *Idtr,
  IN UINTN                           InterruptNumber,
  IN UINTN                           InterruptHandler
  )
{
  IA32_IDT_GATE_DESCRIPTOR           *IdtEntry;

  IdtEntry = (IA32_IDT_GATE_DESCRIPTOR *)Idtr->Base;
  //
  // Mark the Interrupt Handler is not present because it stores a data pointer.
  //
  IdtEntry[InterruptNumber].Bits.GateType = 0;
  IdtEntry[InterruptNumber].Bits.OffsetLow   = (UINT16)(UINTN)InterruptHandler;
  IdtEntry[InterruptNumber].Bits.OffsetHigh  = (UINT16)((UINTN)InterruptHandler >> 16);
}

/**
  Read IDT handler value from IDT entry.

  @param IdtEntry          Pointer to IDT entry to be read.

**/
UINTN
MutexLibGetInterruptHandler (
  IN IA32_DESCRIPTOR                 *Idtr,
  IN UINTN                           InterruptNumber
  )
{
  IA32_IDT_GATE_DESCRIPTOR           *IdtEntry;

  IdtEntry = (IA32_IDT_GATE_DESCRIPTOR *)Idtr->Base;
  return (UINTN)IdtEntry[InterruptNumber].Bits.OffsetLow + (((UINTN)IdtEntry[InterruptNumber].Bits.OffsetHigh) << 16);
}

EFI_STATUS
EFIAPI
PeiMutexLibMemoryDiscoveredCallback (
  IN EFI_PEI_SERVICES                     **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR            *NotifyDescriptor,
  IN VOID                                 *Ppi
  )
{
  IA32_DESCRIPTOR        Idtr;
  MUTEX_ARRAY            *Mutexes;

  Mutexes = GetFirstGuidHob (&mMutexLibDatabaseGuid);
  if (Mutexes == NULL) {
    //
    // Mutex database GUIDed HOB is created in constructor. Impossible to get none here.
    //
    CpuDeadLoop ();
  }

  //
  // Save the semaphore database pointer to IDT[35] as well so AP can access it.
  // This should be done before PEI MP initialization.
  //
  AsmReadIdtr (&Idtr);
  MutexLibSetInterruptHandler (&Idtr, MUTEX_IDT_ENTRY_INDEX, (UINTN)Mutexes);
  return EFI_SUCCESS;
}

EFI_PEI_NOTIFY_DESCRIPTOR mPeiMutexLibMemoryDiscoveredNotifyList = {
  (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEfiPeiMemoryDiscoveredPpiGuid,
  PeiMutexLibMemoryDiscoveredCallback
};


EFI_STATUS
EFIAPI
PeiMutexLibConstructor (
  IN EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES    **PeiServices
  )
{
  EFI_STATUS             Status;
  IA32_DESCRIPTOR        Idtr;
  VOID                   *NewIdt;
  UINT16                 NewIdtLimit;
  VOID                   *Ppi;
  MUTEX_ARRAY            *Mutexes;

  Mutexes = GetFirstGuidHob (&mMutexLibDatabaseGuid);
  if (Mutexes == NULL) {
    Status = PeiServicesLocatePpi (&gEfiPeiMpServicesPpiGuid, 0, NULL, (VOID **)&Ppi);
    if (!EFI_ERROR (Status)) {
      //
      // IDT[35] update should be before PEI MP initialization otherwise AP won't get the updated IDT.
      //
      return EFI_ABORTED;
    }
    AsmReadIdtr (&Idtr);

    //
    // Enlarge the IDT entry to be able to hold the MUTEX global pointer.
    //
    if ((Idtr.Limit + 1) / sizeof (IA32_IDT_GATE_DESCRIPTOR) < MUTEX_IDT_ENTRY_INDEX + 1) {
      NewIdtLimit = (MUTEX_IDT_ENTRY_INDEX + 1) * sizeof (IA32_IDT_GATE_DESCRIPTOR) - 1;

      //
      // Allocate the permanent memory (AllocatePool allocates 8-byte aligned buffer).
      //
      NewIdt = AllocatePool (NewIdtLimit + 1 + sizeof (UINT64));
      if (NewIdt == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      //
      // Reserve 8 bytes for PeiServices pointer
      //
      NewIdt = (VOID *)((UINTN)NewIdt + sizeof (UINT64));

      //
      // Set the PeiServices pointer
      //
      *(CONST EFI_PEI_SERVICES ***)((UINTN)NewIdt - sizeof (UINTN)) = PeiServices;

      //
      // Idt table needs to be migrated to new memory.
      //
      CopyMem ((VOID *)(UINTN)NewIdt, (VOID *)Idtr.Base, Idtr.Limit + 1);
      ZeroMem ((VOID *)((UINTN)NewIdt + Idtr.Limit + 1), NewIdtLimit - Idtr.Limit);
      Idtr.Base  = (UINTN)NewIdt;
      Idtr.Limit = NewIdtLimit;
      AsmWriteIdtr (&Idtr);
    }

    //
    // Create the GUIDed HOB for mutex database
    //
    Mutexes = BuildGuidHob (&mMutexLibDatabaseGuid, sizeof (*Mutexes));
    InitializeSpinLock (&Mutexes->Lock);
    ZeroMem (Mutexes->NamedInstance, sizeof (Mutexes->NamedInstance));

    //
    // Save the semaphore database pointer to IDT[35] as well so AP can access it.
    //
    MutexLibSetInterruptHandler (&Idtr, MUTEX_IDT_ENTRY_INDEX, (UINTN)Mutexes);

    //
    // If physical memory hasn't been installed, register a callback to update IDT[34] when HOB migration is done.
    //
    Status = PeiServicesLocatePpi (&gEfiPeiMemoryDiscoveredPpiGuid, 0, NULL, (VOID **)&Ppi);
    if (EFI_ERROR (Status)) {
      PeiServicesNotifyPpi (&mPeiMutexLibMemoryDiscoveredNotifyList);
    }
  }
  return EFI_SUCCESS;
}

/**
  Create a mutex.

  This function creates or opens a named or nameless mutex.
  It must be called by BSP because it uses the HOB service.

  @param  Name         Guided name.
                       If Name is NULL, nameless semaphore is created.
  @param  Mutex        Return an existing mutex or a new created mutex.

  @retval RETURN_SUCCESS           The mutex is created or opened successfully.
  @retval RETURN_INVALID_PARAMETER Semaphore is NULL.
                                   Name is zero GUID.
  @retval RETURN_OUT_OF_RESOURCES  There is no sufficient resource to create the mutex.
  @retval RETURN_UNSUPPORTED       AP doesn't support to create a semaphore.
**/
RETURN_STATUS
EFIAPI
MutexCreate (
  IN  CONST GUID *Name,
  OUT MUTEX      *Mutex
  )
{
  MUTEX_ARRAY         *Mutexes;
  MUTEX_INSTANCE      *Instance;
  UINTN               Index;
  INTN                FreeIndex;
  IA32_DESCRIPTOR     Idtr;

  if ((Mutex == NULL) || ((Name != NULL) && IsZeroGuid (Name))) {
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
  } else {
    //
    // Find the mutex with the same Name.
    // Access the database pointer through IDT to avoid calling PEI services from AP.
    //
    AsmReadIdtr (&Idtr);
    Mutexes = (MUTEX_ARRAY *)MutexLibGetInterruptHandler (&Idtr, MUTEX_IDT_ENTRY_INDEX);
    if (Mutexes == NULL) {
      return RETURN_OUT_OF_RESOURCES;
    }

    AcquireSpinLock (&Mutexes->Lock);
    FreeIndex = -1;
    for (Index = 0; Index < ARRAY_SIZE (Mutexes->NamedInstance); Index++) {
      if (CompareGuid (Name, &Mutexes->NamedInstance[Index].Name)) {
        *Mutex = &Mutexes->NamedInstance[Index].Instance;
        ReleaseSpinLock (&Mutexes->Lock);
        return RETURN_SUCCESS;
      } else if ((FreeIndex == -1) && IsZeroGuid (&Mutexes->NamedInstance[Index].Name)) {
        FreeIndex = Index;
      }
    }

    if (FreeIndex == -1) {
      ReleaseSpinLock (&Mutexes->Lock);
      return RETURN_OUT_OF_RESOURCES;
    }

    if (!MutexLibIsBsp ()) {
      //
      // AP supports to create mutex in PEI. For consistency, still return UNSUPPORTED.
      //
      ReleaseSpinLock (&Mutexes->Lock);
      return RETURN_UNSUPPORTED;
    }

    CopyGuid (&Mutexes->NamedInstance[FreeIndex].Name, Name);
    Instance = &Mutexes->NamedInstance[FreeIndex].Instance;
  }
  Instance->Signature     = MUTEX_SIGNATURE;
  Instance->OwnerAndCount = MUTEX_RELEASED;
  ReleaseSpinLock (&Mutexes->Lock);

  *Mutex = Instance;
  return RETURN_SUCCESS;
}

/**
  Destroy a mutex.

  This function destroys the mutex.
  It must be called by BSP because it uses the Memory allocation service.

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
  MUTEX_ARRAY         *Mutexes;
  MUTEX_INSTANCE      *Instance;
  IA32_DESCRIPTOR     Idtr;
  UINTN               Index;

  Instance = (MUTEX_INSTANCE *)Mutex;
  if ((Instance == NULL) || (Instance->Signature != MUTEX_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (!MutexLibIsBsp ()) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Find the semaphore with the same Name.
  // Access the database pointer through IDT to avoid calling PEI services from AP.
  //
  AsmReadIdtr (&Idtr);
  Mutexes = (MUTEX_ARRAY *)MutexLibGetInterruptHandler (&Idtr, MUTEX_IDT_ENTRY_INDEX);
  if (Mutexes == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  AcquireSpinLock (&Mutexes->Lock);
  //
  // Zero out the GUID name to destroy the named semaphore
  //
  for (Index = 0; Index < ARRAY_SIZE (Mutexes->NamedInstance); Index++) {
    if (Mutex == &Mutexes->NamedInstance[Index].Instance) {
      ZeroMem (&Mutexes->NamedInstance[Index], sizeof (Mutexes->NamedInstance[Index]));
      ReleaseSpinLock (&Mutexes->Lock);
      return RETURN_SUCCESS;
    }
  }
  ReleaseSpinLock (&Mutexes->Lock);

  //
  // For nameless semaphore, free the memory only.
  // PEI version of FreePool() may do nothing. Still call it to match DXE implementation.
  //
  FreePool (Mutex);

  return RETURN_SUCCESS;
}
