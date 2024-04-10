/** @file
  SMM Relocation Lib for each processor.

  This Lib produces the SMM_BASE_HOB in HOB database which tells
  the PiSmmCpuDxeSmm driver (runs at a later phase) about the new
  SMBASE for each processor. PiSmmCpuDxeSmm driver installs the
  SMI handler at the SMM_BASE_HOB.SmBase[Index]+0x8000 for processor
  Index.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "InternalSmmRelocationLib.h"

UINTN  mMaxNumberOfCpus = 1;
UINTN  mNumberOfCpus    = 1;

//
// Record all Processors Info
//
EFI_PROCESSOR_INFORMATION  *mProcessorInfo = NULL;

//
// IDT used during SMM Init
//
IA32_DESCRIPTOR  gcSmiIdtr;

//
// Smbase for all CPUs
//
UINT64  *mSmBase = NULL;

//
// SmBase Rebased flag for all CPUs
//
volatile BOOLEAN  *mRebased;

/**
  This function will create SmBase for all CPUs.

  @param[in] SmBase    Pointer to SmBase for all CPUs.

  @retval EFI_SUCCESS           Create SmBase for all CPUs successfully.
  @retval Others                Failed to create SmBase for all CPUs.

**/
EFI_STATUS
CreateSmmBaseHob (
  IN UINT64  *SmBase
  )
{
  UINTN              Index;
  SMM_BASE_HOB_DATA  *SmmBaseHobData;
  UINT32             CpuCount;
  UINT32             NumberOfProcessorsInHob;
  UINT32             MaxCapOfProcessorsInHob;
  UINT32             HobCount;

  SmmBaseHobData          = NULL;
  CpuCount                = 0;
  NumberOfProcessorsInHob = 0;
  MaxCapOfProcessorsInHob = 0;
  HobCount                = 0;

  //
  // Count the HOB instance maximum capacity of CPU (MaxCapOfProcessorsInHob) since the max HobLength is 0xFFF8.
  //
  MaxCapOfProcessorsInHob = (0xFFF8 - sizeof (EFI_HOB_GUID_TYPE) - sizeof (SMM_BASE_HOB_DATA)) / sizeof (UINT64) + 1;
  DEBUG ((DEBUG_INFO, "CreateSmmBaseHob - MaxCapOfProcessorsInHob: %d\n", MaxCapOfProcessorsInHob));

  //
  // Create Guided SMM Base HOB Instances.
  //
  while (CpuCount != mMaxNumberOfCpus) {
    NumberOfProcessorsInHob = MIN ((UINT32)mMaxNumberOfCpus - CpuCount, MaxCapOfProcessorsInHob);

    SmmBaseHobData = BuildGuidHob (
                       &gSmmBaseHobGuid,
                       sizeof (SMM_BASE_HOB_DATA) + sizeof (UINT64) * NumberOfProcessorsInHob
                       );
    if (SmmBaseHobData == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    SmmBaseHobData->ProcessorIndex     = CpuCount;
    SmmBaseHobData->NumberOfProcessors = NumberOfProcessorsInHob;

    DEBUG ((DEBUG_INFO, "CreateSmmBaseHob - SmmBaseHobData[%d]->ProcessorIndex: %d\n", HobCount, SmmBaseHobData->ProcessorIndex));
    DEBUG ((DEBUG_INFO, "CreateSmmBaseHob - SmmBaseHobData[%d]->NumberOfProcessors: %d\n", HobCount, SmmBaseHobData->NumberOfProcessors));
    for (Index = 0; Index < SmmBaseHobData->NumberOfProcessors; Index++) {
      //
      // Calculate the new SMBASE address
      //
      SmmBaseHobData->SmBase[Index] = SmBase[Index + CpuCount];
      DEBUG ((DEBUG_INFO, "CreateSmmBaseHob - SmmBaseHobData[%d]->SmBase[%d]: 0x%08x\n", HobCount, Index, SmmBaseHobData->SmBase[Index]));
    }

    CpuCount += NumberOfProcessorsInHob;
    HobCount++;
    SmmBaseHobData = NULL;
  }

  return EFI_SUCCESS;
}

/**
  C function for SMI handler. To change all processor's SMMBase Register.

**/
VOID
EFIAPI
SmmInitHandler (
  VOID
  )
{
  UINT32  ApicId;
  UINTN   Index;

  //
  // Update SMM IDT entries' code segment and load IDT
  //
  AsmWriteIdtr (&gcSmiIdtr);
  ApicId = GetApicId ();

  for (Index = 0; Index < mNumberOfCpus; Index++) {
    if (ApicId == (UINT32)mProcessorInfo[Index].ProcessorId) {
      //
      // Configure SmBase.
      //
      ConfigureSmBase (mSmBase[Index]);

      //
      // Hook return after RSM to set SMM re-based flag
      // SMM re-based flag can't be set before RSM, because SMM save state context might be override
      // by next AP flow before it take effect.
      //
      SemaphoreHook (Index, &mRebased[Index]);
      return;
    }
  }

  ASSERT (FALSE);
}

/**
  Relocate SmmBases for each processor.
  Execute on first boot and all S3 resumes

**/
VOID
SmmRelocateBases (
  VOID
  )
{
  UINT8                 BakBuf[BACK_BUF_SIZE];
  SMRAM_SAVE_STATE_MAP  BakBuf2;
  SMRAM_SAVE_STATE_MAP  *CpuStatePtr;
  UINT8                 *U8Ptr;
  UINTN                 Index;
  UINTN                 BspIndex;
  UINT32                BspApicId;

  //
  // Make sure the reserved size is large enough for procedure SmmInitTemplate.
  //
  ASSERT (sizeof (BakBuf) >= gcSmmInitSize);

  //
  // Patch ASM code template with current CR0, CR3, and CR4 values
  //
  PatchInstructionX86 (gPatchSmmCr0, AsmReadCr0 (), 4);
  PatchInstructionX86 (gPatchSmmCr3, AsmReadCr3 (), 4);
  PatchInstructionX86 (gPatchSmmCr4, AsmReadCr4 () & (~CR4_CET_ENABLE), 4);

  U8Ptr       = (UINT8 *)(UINTN)(SMM_DEFAULT_SMBASE + SMM_HANDLER_OFFSET);
  CpuStatePtr = (SMRAM_SAVE_STATE_MAP *)(UINTN)(SMM_DEFAULT_SMBASE + SMRAM_SAVE_STATE_MAP_OFFSET);

  //
  // Backup original contents at address 0x38000
  //
  CopyMem (BakBuf, U8Ptr, sizeof (BakBuf));
  CopyMem (&BakBuf2, CpuStatePtr, sizeof (BakBuf2));

  //
  // Load image for relocation
  //
  CopyMem (U8Ptr, gcSmmInitTemplate, gcSmmInitSize);

  //
  // Retrieve the local APIC ID of current processor
  //
  BspApicId = GetApicId ();

  //
  // Relocate SM bases for all APs
  // This is APs' 1st SMI - rebase will be done here, and APs' default SMI handler will be overridden by gcSmmInitTemplate
  //
  BspIndex = (UINTN)-1;
  for (Index = 0; Index < mNumberOfCpus; Index++) {
    mRebased[Index] = FALSE;
    if (BspApicId != (UINT32)mProcessorInfo[Index].ProcessorId) {
      SendSmiIpi ((UINT32)mProcessorInfo[Index].ProcessorId);
      //
      // Wait for this AP to finish its 1st SMI
      //
      while (!mRebased[Index]) {
      }
    } else {
      //
      // BSP will be Relocated later
      //
      BspIndex = Index;
    }
  }

  //
  // Relocate BSP's SMM base
  //
  ASSERT (BspIndex != (UINTN)-1);
  SendSmiIpi (BspApicId);

  //
  // Wait for the BSP to finish its 1st SMI
  //
  while (!mRebased[BspIndex]) {
  }

  //
  // Restore contents at address 0x38000
  //
  CopyMem (CpuStatePtr, &BakBuf2, sizeof (BakBuf2));
  CopyMem (U8Ptr, BakBuf, sizeof (BakBuf));
}

/**
  Initialize IDT to setup exception handlers in SMM.

**/
VOID
InitSmmIdt (
  VOID
  )
{
  EFI_STATUS              Status;
  BOOLEAN                 InterruptState;
  IA32_DESCRIPTOR         PeiIdtr;
  CONST EFI_PEI_SERVICES  **PeiServices;

  //
  // There are 32 (not 255) entries in it since only processor
  // generated exceptions will be handled.
  //
  gcSmiIdtr.Limit = (sizeof (IA32_IDT_GATE_DESCRIPTOR) * 32) - 1;

  //
  // Allocate for IDT.
  // sizeof (UINTN) is for the PEI Services Table pointer.
  //
  gcSmiIdtr.Base = (UINTN)AllocateZeroPool (gcSmiIdtr.Limit + 1 + sizeof (UINTN));
  ASSERT (gcSmiIdtr.Base != 0);
  gcSmiIdtr.Base += sizeof (UINTN);

  //
  // Disable Interrupt, save InterruptState and save PEI IDT table
  //
  InterruptState = SaveAndDisableInterrupts ();
  AsmReadIdtr (&PeiIdtr);

  //
  // Save the PEI Services Table pointer
  // The PEI Services Table pointer will be stored in the sizeof (UINTN) bytes
  // immediately preceding the IDT in memory.
  //
  PeiServices                                   = (CONST EFI_PEI_SERVICES **)(*(UINTN *)(PeiIdtr.Base - sizeof (UINTN)));
  (*(UINTN *)(gcSmiIdtr.Base - sizeof (UINTN))) = (UINTN)PeiServices;

  //
  // Load SMM temporary IDT table
  //
  AsmWriteIdtr (&gcSmiIdtr);

  //
  // Setup SMM default exception handlers, SMM IDT table
  // will be updated and saved in gcSmiIdtr
  //
  Status = InitializeCpuExceptionHandlers (NULL);
  ASSERT_EFI_ERROR (Status);

  //
  // Restore PEI IDT table and CPU InterruptState
  //
  AsmWriteIdtr ((IA32_DESCRIPTOR *)&PeiIdtr);
  SetInterruptState (InterruptState);
}

/**
  This routine will split SmramReserve HOB to reserve SmmRelocationSize for Smm relocated memory.

  @param[in]       SmmRelocationSize   SmmRelocationSize for all processors.
  @param[in,out]   SmmRelocationStart  Return the start address of Smm relocated memory in SMRAM.

  @retval EFI_SUCCESS           The gEfiSmmSmramMemoryGuid is split successfully.
  @retval EFI_DEVICE_ERROR      Failed to build new HOB for gEfiSmmSmramMemoryGuid.
  @retval EFI_NOT_FOUND         The gEfiSmmSmramMemoryGuid is not found.

**/
EFI_STATUS
SplitSmramHobForSmmRelocation (
  IN     UINT64                SmmRelocationSize,
  IN OUT EFI_PHYSICAL_ADDRESS  *SmmRelocationStart
  )
{
  EFI_HOB_GUID_TYPE               *GuidHob;
  EFI_SMRAM_HOB_DESCRIPTOR_BLOCK  *Block;
  EFI_SMRAM_HOB_DESCRIPTOR_BLOCK  *NewBlock;
  UINTN                           NewBlockSize;

  ASSERT (SmmRelocationStart != NULL);

  //
  // Retrieve the GUID HOB data that contains the set of SMRAM descriptors
  //
  GuidHob = GetFirstGuidHob (&gEfiSmmSmramMemoryGuid);
  if (GuidHob == NULL) {
    return EFI_NOT_FOUND;
  }

  Block = (EFI_SMRAM_HOB_DESCRIPTOR_BLOCK *)GET_GUID_HOB_DATA (GuidHob);

  //
  // Allocate one extra EFI_SMRAM_DESCRIPTOR to describe smram carved out for all SMBASE
  //
  NewBlockSize = sizeof (EFI_SMRAM_HOB_DESCRIPTOR_BLOCK) + (Block->NumberOfSmmReservedRegions * sizeof (EFI_SMRAM_DESCRIPTOR));

  NewBlock = (EFI_SMRAM_HOB_DESCRIPTOR_BLOCK *)BuildGuidHob (
                                                 &gEfiSmmSmramMemoryGuid,
                                                 NewBlockSize
                                                 );
  ASSERT (NewBlock != NULL);
  if (NewBlock == NULL) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Copy old EFI_SMRAM_HOB_DESCRIPTOR_BLOCK to new allocated region
  //
  CopyMem ((VOID *)NewBlock, Block, NewBlockSize - sizeof (EFI_SMRAM_DESCRIPTOR));

  //
  // Increase the number of SMRAM descriptors by 1 to make room for the ALLOCATED descriptor of size EFI_PAGE_SIZE
  //
  NewBlock->NumberOfSmmReservedRegions = (UINT32)(Block->NumberOfSmmReservedRegions + 1);

  ASSERT (Block->NumberOfSmmReservedRegions >= 1);
  //
  // Copy last entry to the end - we assume TSEG is last entry.
  //
  CopyMem (&NewBlock->Descriptor[Block->NumberOfSmmReservedRegions], &NewBlock->Descriptor[Block->NumberOfSmmReservedRegions - 1], sizeof (EFI_SMRAM_DESCRIPTOR));

  //
  // Update the entry in the array with a size of SmmRelocationSize and put into the ALLOCATED state
  //
  NewBlock->Descriptor[Block->NumberOfSmmReservedRegions - 1].PhysicalSize = SmmRelocationSize;
  NewBlock->Descriptor[Block->NumberOfSmmReservedRegions - 1].RegionState |= EFI_ALLOCATED;

  //
  // Return the start address of Smm relocated memory in SMRAM.
  //
  *SmmRelocationStart = NewBlock->Descriptor[Block->NumberOfSmmReservedRegions - 1].CpuStart;

  //
  // Reduce the size of the last SMRAM descriptor by SmmRelocationSize
  //
  NewBlock->Descriptor[Block->NumberOfSmmReservedRegions].PhysicalStart += SmmRelocationSize;
  NewBlock->Descriptor[Block->NumberOfSmmReservedRegions].CpuStart      += SmmRelocationSize;
  NewBlock->Descriptor[Block->NumberOfSmmReservedRegions].PhysicalSize  -= SmmRelocationSize;

  //
  // Last step, we can scrub old one
  //
  ZeroMem (&GuidHob->Name, sizeof (GuidHob->Name));

  return EFI_SUCCESS;
}

/**
  This function will initialize SmBase for all CPUs.

  @param[in,out] SmBase    Pointer to SmBase for all CPUs.

  @retval EFI_SUCCESS           Initialize SmBase for all CPUs successfully.
  @retval Others                Failed to initialize SmBase for all CPUs.

**/
EFI_STATUS
InitSmBaseForAllCpus (
  IN OUT UINT64  **SmBase
  )
{
  EFI_STATUS            Status;
  UINTN                 TileSize;
  UINT64                SmmRelocationSize;
  EFI_PHYSICAL_ADDRESS  SmmRelocationStart;
  UINTN                 Index;

  SmmRelocationStart = 0;

  ASSERT (SmBase != NULL);

  //
  // Calculate SmmRelocationSize for all of the tiles.
  //
  // The CPU save state and code for the SMI entry point are tiled within an SMRAM
  // allocated buffer. The minimum size of this buffer for a uniprocessor system
  // is 32 KB, because the entry point is SMBASE + 32KB, and CPU save state area
  // just below SMBASE + 64KB. If more than one CPU is present in the platform,
  // then the SMI entry point and the CPU save state areas can be tiles to minimize
  // the total amount SMRAM required for all the CPUs. The tile size can be computed
  // by adding the CPU save state size, any extra CPU specific context, and
  // the size of code that must be placed at the SMI entry point to transfer
  // control to a C function in the native SMM execution mode. This size is
  // rounded up to the nearest power of 2 to give the tile size for a each CPU.
  // The total amount of memory required is the maximum number of CPUs that
  // platform supports times the tile size.
  //
  TileSize          = SIZE_8KB;
  SmmRelocationSize = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (SIZE_32KB + TileSize * (mMaxNumberOfCpus - 1)));

  //
  // Split SmramReserve HOB to reserve SmmRelocationSize for Smm relocated memory
  //
  Status = SplitSmramHobForSmmRelocation (
             SmmRelocationSize,
             &SmmRelocationStart
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (SmmRelocationStart != 0);
  DEBUG ((DEBUG_INFO, "InitSmBaseForAllCpus - SmmRelocationSize: 0x%08x\n", SmmRelocationSize));
  DEBUG ((DEBUG_INFO, "InitSmBaseForAllCpus - SmmRelocationStart: 0x%08x\n", SmmRelocationStart));

  //
  // Init SmBase
  //
  *SmBase = (UINT64 *)AllocatePages (EFI_SIZE_TO_PAGES (sizeof (UINT64) * mMaxNumberOfCpus));
  if (*SmBase == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
    //
    // Return each SmBase in SmBase
    //
    (*SmBase)[Index] = (UINTN)(SmmRelocationStart)+ Index * TileSize - SMM_HANDLER_OFFSET;
    DEBUG ((DEBUG_INFO, "InitSmBaseForAllCpus - SmBase For CPU[%d]: 0x%08x\n", Index, (*SmBase)[Index]));
  }

  return EFI_SUCCESS;
}

/**
  CPU SmmBase Relocation Init.

  This function is to relocate CPU SmmBase.

  @param[in] MpServices2        Pointer to this instance of the MpServices.

  @retval EFI_SUCCESS           CPU SmmBase Relocated successfully.
  @retval Others                CPU SmmBase Relocation failed.

**/
EFI_STATUS
EFIAPI
SmmRelocationInit (
  IN EDKII_PEI_MP_SERVICES2_PPI  *MpServices2
  )
{
  EFI_STATUS  Status;
  UINTN       NumberOfEnabledCpus;
  UINTN       SmmStackSize;
  UINT8       *SmmStacks;
  UINTN       Index;

  SmmStacks = NULL;

  DEBUG ((DEBUG_INFO, "SmmRelocationInit Start \n"));
  if (MpServices2 == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Get the number of processors
  //
  Status = MpServices2->GetNumberOfProcessors (
                          MpServices2,
                          &mNumberOfCpus,
                          &NumberOfEnabledCpus
                          );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  if (FeaturePcdGet (PcdCpuHotPlugSupport)) {
    mMaxNumberOfCpus = PcdGet32 (PcdCpuMaxLogicalProcessorNumber);
  } else {
    mMaxNumberOfCpus = mNumberOfCpus;
  }

  ASSERT (mNumberOfCpus <= mMaxNumberOfCpus);

  //
  // Retrieve the Processor Info for all CPUs
  //
  mProcessorInfo = (EFI_PROCESSOR_INFORMATION *)AllocatePool (sizeof (EFI_PROCESSOR_INFORMATION) * mMaxNumberOfCpus);
  if (mProcessorInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
    if (Index < mNumberOfCpus) {
      Status = MpServices2->GetProcessorInfo (MpServices2, Index | CPU_V2_EXTENDED_TOPOLOGY, &mProcessorInfo[Index]);
      if (EFI_ERROR (Status)) {
        goto ON_EXIT;
      }
    }
  }

  //
  // Initialize the SmBase for all CPUs
  //
  Status = InitSmBaseForAllCpus (&mSmBase);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  //
  // Fix up the address of the global variable or function referred in
  // SmmInit assembly files to be the absolute address
  //
  SmmInitFixupAddress ();

  //
  // Patch SMI stack for SMM base relocation
  // Note: No need allocate stack for all CPUs since the relocation
  // occurs serially for each CPU
  //
  SmmStackSize = EFI_PAGE_SIZE;
  SmmStacks    = (UINT8 *)AllocatePages (EFI_SIZE_TO_PAGES (SmmStackSize));
  if (SmmStacks == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  DEBUG ((DEBUG_INFO, "SmmRelocationInit - SmmStackSize: 0x%08x\n", SmmStackSize));
  DEBUG ((DEBUG_INFO, "SmmRelocationInit - SmmStacks: 0x%08x\n", SmmStacks));

  PatchInstructionX86 (
    gPatchSmmInitStack,
    (UINTN)(SmmStacks + SmmStackSize - sizeof (UINTN)),
    sizeof (UINTN)
    );

  //
  // Initialize the SMM IDT for SMM base relocation
  //
  InitSmmIdt ();

  //
  // Relocate SmmBases for each processor.
  // Allocate mRebased as the flag to indicate the relocation is done for each CPU.
  //
  mRebased = (BOOLEAN *)AllocateZeroPool (sizeof (BOOLEAN) * mMaxNumberOfCpus);
  if (mRebased == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  SmmRelocateBases ();

  //
  // Create the SmBase HOB for all CPUs
  //
  Status = CreateSmmBaseHob (mSmBase);

ON_EXIT:
  if (SmmStacks != NULL) {
    FreePages (SmmStacks, EFI_SIZE_TO_PAGES (SmmStackSize));
  }

  if (mSmBase != NULL) {
    FreePages (mSmBase, EFI_SIZE_TO_PAGES (sizeof (UINT64) * mMaxNumberOfCpus));
  }

  DEBUG ((DEBUG_INFO, "SmmRelocationInit Done\n"));
  return Status;
}
