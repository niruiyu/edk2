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
#include <Ppi/MemoryDiscovered.h>
#include <Ppi/MpServices.h>
#include <Library/PeiServicesLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include "PeiSemaphoreLib.h"

#define SEMAPHORE_IDT_ENTRY_INDEX   34
#define MAX_INSTANCE                10

GUID mSemaphoreLibDatabaseGuid = { 0x42b28b9a, 0xf409, 0x494d,{ 0x8f, 0x71, 0x46, 0x54, 0x34, 0xec, 0xb1, 0x7f } };

typedef struct {
  SPIN_LOCK                Lock;
  NAMED_SEMAPHORE_INSTANCE NamedInstance[MAX_INSTANCE];
} SEMAPHORE_ARRAY;

/**
  Return address map of exception handler template so that C code can generate
  exception tables.

  @param IdtEntry          Pointer to IDT entry to be updated.
  @param InterruptHandler  IDT handler value.

**/
VOID
SemaphoreLibSetInterruptHandler (
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
SemaphoreLibGetInterruptHandler (
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
PeiSemaphoreLibMemoryDiscoveredCallback (
  IN EFI_PEI_SERVICES                     **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR            *NotifyDescriptor,
  IN VOID                                 *Ppi
  )
{
  IA32_DESCRIPTOR        Idtr;
  SEMAPHORE_ARRAY        *Semaphores;

  Semaphores = GetFirstGuidHob (&mSemaphoreLibDatabaseGuid);
  if (Semaphores == NULL) {
    //
    // Semaphore database GUIDed HOB is created in constructor. Impossible to get none here.
    //
    CpuDeadLoop ();
  }

  //
  // Save the semaphore database pointer to IDT[34] as well so AP can access it.
  // This should be done before PEI MP initialization.
  //
  AsmReadIdtr (&Idtr);
  SemaphoreLibSetInterruptHandler (&Idtr, SEMAPHORE_IDT_ENTRY_INDEX, (UINTN)Semaphores);
  return EFI_SUCCESS;
}

EFI_PEI_NOTIFY_DESCRIPTOR mPeiSemaphoreLibMemoryDiscoveredNotifyList = {
  (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEfiPeiMemoryDiscoveredPpiGuid,
  PeiSemaphoreLibMemoryDiscoveredCallback
};


EFI_STATUS
EFIAPI
PeiSemaphoreLibConstructor (
  IN EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES    **PeiServices
  )
{
  EFI_STATUS             Status;
  IA32_DESCRIPTOR        Idtr;
  VOID                   *NewIdt;
  UINT16                 NewIdtLimit;
  VOID                   *Ppi;
  SEMAPHORE_ARRAY        *Semaphores;

  Semaphores = GetFirstGuidHob (&mSemaphoreLibDatabaseGuid);
  if (Semaphores == NULL) {
    Status = PeiServicesLocatePpi (&gEfiPeiMpServicesPpiGuid, 0, NULL, (VOID **)&Ppi);
    if (!EFI_ERROR (Status)) {
      //
      // IDT[34] update should be before PEI MP initialization otherwise AP won't get the updated IDT.
      //
      return EFI_ABORTED;
    }
    AsmReadIdtr (&Idtr);

    //
    // Enlarge the IDT entry to be able to hold the SEMAPHORE global pointer.
    //
    if ((Idtr.Limit + 1) / sizeof (IA32_IDT_GATE_DESCRIPTOR) < SEMAPHORE_IDT_ENTRY_INDEX + 1) {
      NewIdtLimit = (SEMAPHORE_IDT_ENTRY_INDEX + 1) * sizeof (IA32_IDT_GATE_DESCRIPTOR) - 1;

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
    // Create the GUIDed HOB for semaphore database
    //
    Semaphores = BuildGuidHob (&mSemaphoreLibDatabaseGuid, sizeof (*Semaphores));
    InitializeSpinLock (&Semaphores->Lock);
    ZeroMem (Semaphores->NamedInstance, sizeof (Semaphores->NamedInstance));

    //
    // Save the semaphore database pointer to IDT[34] as well so AP can access it.
    //
    SemaphoreLibSetInterruptHandler (&Idtr, SEMAPHORE_IDT_ENTRY_INDEX, (UINTN)Semaphores);

    //
    // If physical memory hasn't been installed, register a callback to update IDT[34] when HOB migration is done.
    //
    Status = PeiServicesLocatePpi (&gEfiPeiMemoryDiscoveredPpiGuid, 0, NULL, (VOID **)&Ppi);
    if (EFI_ERROR (Status)) {
      PeiServicesNotifyPpi (&mPeiSemaphoreLibMemoryDiscoveredNotifyList);
    }
  }
  return EFI_SUCCESS;
}

/**
  Create a semaphore.

  This function creates or opens a named or nameless semaphore.
  It must be called by BSP because it uses the HOB service.

  @param  Name         Guided name.
                       If Name matches the name of an existing semaphore, the InitialCount if ignored
                       because it has already been set by the creating process.
                       If Name is NULL, nameless semaphore is created.
  @param  Semaphore    Return an existing semaphore or a new created semaphore.
  @param  InitialCount The count of resources available for the semaphore.

  @retval RETURN_SUCCESS           The semaphore is created or opened successfully.
  @retval RETURN_INVALID_PARAMETER Semaphore is NULL.
                                   Name is zero GUID.
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
  SEMAPHORE_ARRAY     *Semaphores;
  SEMAPHORE_INSTANCE  *Instance;
  UINTN               Index;
  INTN                FreeIndex;
  IA32_DESCRIPTOR     Idtr;

  if ((Semaphore == NULL) || ((Name != NULL) && IsZeroGuid (Name))) {
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
    // Find the semaphore with the same Name.
    // Access the database pointer through IDT to avoid calling PEI services from AP.
    //
    AsmReadIdtr (&Idtr);
    Semaphores = (SEMAPHORE_ARRAY *)SemaphoreLibGetInterruptHandler (&Idtr, SEMAPHORE_IDT_ENTRY_INDEX);
    if (Semaphores == NULL) {
      return RETURN_OUT_OF_RESOURCES;
    }

    AcquireSpinLock (&Semaphores->Lock);
    FreeIndex = -1;
    for (Index = 0; Index < ARRAY_SIZE (Semaphores->NamedInstance); Index++) {
      if (CompareGuid (Name, &Semaphores->NamedInstance[Index].Name)) {
        *Semaphore = &Semaphores->NamedInstance[Index].Instance;
        ReleaseSpinLock (&Semaphores->Lock);
        return RETURN_SUCCESS;
      } else if ((FreeIndex == -1) && IsZeroGuid (&Semaphores->NamedInstance[Index].Name)) {
        FreeIndex = Index;
      }
    }

    if (FreeIndex == -1) {
      ReleaseSpinLock (&Semaphores->Lock);
      return RETURN_OUT_OF_RESOURCES;
    }

    if (!MutexLibIsBsp ()) {
      //
      // AP supports to create semaphore in PEI. For consistency, still return UNSUPPORTED.
      //
      ReleaseSpinLock (&Semaphores->Lock);
      return RETURN_UNSUPPORTED;
    }

    CopyGuid (&Semaphores->NamedInstance[FreeIndex].Name, Name);
    Instance = &Semaphores->NamedInstance[FreeIndex].Instance;
  }
  Instance->Signature = SEMAPHORE_SIGNATURE;
  Instance->Count     = InitialCount;
  ReleaseSpinLock (&Semaphores->Lock);

  *Semaphore = Instance;
  return RETURN_SUCCESS;
}

/**
  Destroy a semaphore.

  This function destroys the semaphore.
  It must be called by BSP because it uses the Memory allocation service.

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
  SEMAPHORE_ARRAY     *Semaphores;
  SEMAPHORE_INSTANCE  *Instance;
  IA32_DESCRIPTOR     Idtr;
  UINTN               Index;

  Instance = (SEMAPHORE_INSTANCE *)Semaphore;
  if ((Instance == NULL) || (Instance->Signature != SEMAPHORE_SIGNATURE)) {
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
  Semaphores = (SEMAPHORE_ARRAY *)SemaphoreLibGetInterruptHandler (&Idtr, SEMAPHORE_IDT_ENTRY_INDEX);
  if (Semaphores == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  AcquireSpinLock (&Semaphores->Lock);
  //
  // Zero out the GUID name to destroy the named semaphore
  //
  for (Index = 0; Index < ARRAY_SIZE (Semaphores->NamedInstance); Index++) {
    if (Semaphore == &Semaphores->NamedInstance[Index].Instance) {
      ZeroMem (&Semaphores->NamedInstance[Index], sizeof (Semaphores->NamedInstance[Index]));
      ReleaseSpinLock (&Semaphores->Lock);
      return RETURN_SUCCESS;
    }
  }
  ReleaseSpinLock (&Semaphores->Lock);

  //
  // For nameless semaphore, free the memory only.
  // PEI version of FreePool() may do nothing. Still call it to match DXE implementation.
  //
  FreePool (Semaphore);

  return RETURN_SUCCESS;
}
