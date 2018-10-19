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
#include <Library/PeiServicesLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include "PeiMutexLib.h"
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

GUID  mMutexLibDatabaseGuid = { 0x822aa30e, 0xc79, 0x49c7,{ 0xb8, 0x92, 0x60, 0xbb, 0x4b, 0x58, 0xf1, 0xf6 } };

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

  ASSERT (InterruptNumber < (Idtr->Limit + 1) / sizeof (IA32_IDT_GATE_DESCRIPTOR));

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

  ASSERT (InterruptNumber < (Idtr->Limit + 1) / sizeof (IA32_IDT_GATE_DESCRIPTOR));

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

  DEBUG ((DEBUG_ERROR, "PHY MEM Callback\n"));
  CpuDeadLoop ();
  Mutexes = GetFirstGuidHob (&mMutexLibDatabaseGuid);
  if (Mutexes == NULL) {
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
  VOID                   *NewIdtBase;
  UINT16                 NewIdtLimit;
  VOID                   *Ppi;
  MUTEX_ARRAY            *Mutexes;

  Mutexes = GetFirstGuidHob (&mMutexLibDatabaseGuid);
  DEBUG ((DEBUG_ERROR, "MUTEX CONS\n"));
  if (Mutexes == NULL) {
    AsmReadIdtr (&Idtr);

    //
    // Enlarge the IDT entry to be able to hold the MUTEX global pointer.
    //
    if ((Idtr.Limit + 1) / sizeof (IA32_IDT_GATE_DESCRIPTOR) < MUTEX_IDT_ENTRY_INDEX + 1) {
      NewIdtLimit = (MUTEX_IDT_ENTRY_INDEX + 1) * sizeof (IA32_IDT_GATE_DESCRIPTOR) - 1;
      DEBUG ((DEBUG_ERROR, "MUTEX CONS - Enlarge IDT\n"));

      //
      // Allocate the permanent memory (AllocatePool allocates 8-byte aligned buffer).
      //
      Status = (*PeiServices)->AllocatePool (
        PeiServices,
        NewIdtLimit + 1 + sizeof (UINT64),
        &NewIdtBase
      );
      ASSERT_EFI_ERROR (Status);

      //
      // Reserve 8 bytes for PeiServices pointer
      //
      NewIdtBase = (VOID *)((UINTN)NewIdtBase + sizeof (UINT64));

      //
      // Set the PeiServices pointer
      //
      *(CONST EFI_PEI_SERVICES ***)((UINTN)NewIdtBase - sizeof (UINTN)) = PeiServices;

      //
      // Idt table needs to be migrated to new memory.
      //
      CopyMem ((VOID *)(UINTN)NewIdtBase, (VOID *)Idtr.Base, Idtr.Limit + 1);
      ZeroMem ((VOID *)((UINTN)NewIdtBase + Idtr.Limit + 1), NewIdtLimit - Idtr.Limit);
      Idtr.Base = (UINTN)NewIdtBase;
      Idtr.Limit = NewIdtLimit;
      AsmWriteIdtr (&Idtr);
    }

    //
    // Create the GUIDed HOB for mutex database
    //
    Mutexes = BuildGuidHob (&mMutexLibDatabaseGuid, sizeof (*Mutexes));
    ZeroMem (Mutexes, sizeof (*Mutexes));

    //
    // Save the semaphore database pointer to IDT[35] as well so AP can access it.
    //
    MutexLibSetInterruptHandler (&Idtr, MUTEX_IDT_ENTRY_INDEX, (UINTN)Mutexes);

    //
    // If physical memory hasn't been installed, register a callback to update IDT[34] when HOB migration is done.
    //
    Status = PeiServicesLocatePpi (&gEfiPeiMemoryDiscoveredPpiGuid, 0, NULL, (VOID **)&Ppi);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Listen PHY MEM.\n"));
      Status = PeiServicesNotifyPpi (&mPeiMutexLibMemoryDiscoveredNotifyList);
      ASSERT_EFI_ERROR (Status);
    }


  }
  return EFI_SUCCESS;
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

    FreeIndex = -1;
    for (Index = 0; Index < ARRAY_SIZE (Mutexes->NamedInstance); Index++) {
      DEBUG ((DEBUG_ERROR, "MUTEX[%d] Name=%g\n", Index, &Mutexes->NamedInstance[Index].Name));
      if (CompareGuid (Name, &Mutexes->NamedInstance[Index].Name)) {
        *Mutex = &Mutexes->NamedInstance[Index].Instance;
        return RETURN_SUCCESS;
      } else if ((FreeIndex == -1) && IsZeroGuid (&Mutexes->NamedInstance[Index].Name)) {
        FreeIndex = Index;
      }
    }

    if (FreeIndex == -1) {
      return RETURN_OUT_OF_RESOURCES;
    }

    CopyGuid (&Mutexes->NamedInstance[FreeIndex].Name, Name);
    Instance = &Mutexes->NamedInstance[FreeIndex].Instance;
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
  MUTEX_ARRAY         *Mutexes;
  MUTEX_INSTANCE      *Instance;
  IA32_DESCRIPTOR     Idtr;
  UINTN               Index;

  Instance = (MUTEX_INSTANCE *)Mutex;
  if ((Instance == NULL) || (Instance->Signature != MUTEX_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
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

  for (Index = 0; Index < ARRAY_SIZE (Mutexes->NamedInstance); Index++) {
    if (Mutex == &Mutexes->NamedInstance[Index].Instance) {
      ZeroMem (&Mutexes->NamedInstance[Index].Name, sizeof (GUID));
      return RETURN_SUCCESS;
    }
  }

  FreePool (Mutex);

  return RETURN_SUCCESS;
}
