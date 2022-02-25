/** @file
SMM MP service implementation

Copyright (c) 2009 - 2021, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PiSmmCpuDxeSmm.h"

//
// Slots for all MTRR( FIXED MTRR + VARIABLE MTRR + MTRR_LIB_IA32_MTRR_DEF_TYPE)
//

/**
  Performs an atomic compare exchange operation to get semaphore.
  The compare exchange operation must be performed using
  MP safe mechanisms.

  @param      Sem        IN:  32-bit unsigned integer
                         OUT: original integer - 1
  @return     Original integer - 1

**/
UINT32
WaitForSemaphore (
  IN OUT  volatile UINT32  *Sem
  )
{
  UINT32  Value;

  for ( ; ;) {
    Value = *Sem;
    if ((Value != 0) &&
        (InterlockedCompareExchange32 (
           (UINT32 *)Sem,
           Value,
           Value - 1
           ) == Value))
    {
      break;
    }

    CpuPause ();
  }

  return Value - 1;
}

/**
  Performs an atomic compare exchange operation to release semaphore.
  The compare exchange operation must be performed using
  MP safe mechanisms.

  @param      Sem        IN:  32-bit unsigned integer
                         OUT: original integer + 1
  @return     Original integer + 1

**/
UINT32
ReleaseSemaphore (
  IN OUT  volatile UINT32  *Sem
  )
{
  UINT32  Value;

  do {
    Value = *Sem;
  } while (Value + 1 != 0 &&
           InterlockedCompareExchange32 (
             (UINT32 *)Sem,
             Value,
             Value + 1
             ) != Value);

  return Value + 1;
}

/**
  Performs an atomic compare exchange operation to lock semaphore.
  The compare exchange operation must be performed using
  MP safe mechanisms.

  @param      Sem        IN:  32-bit unsigned integer
                         OUT: -1
  @return     Original integer

**/
UINT32
LockdownSemaphore (
  IN OUT  volatile UINT32  *Sem
  )
{
  UINT32  Value;

  do {
    Value = *Sem;
  } while (InterlockedCompareExchange32 (
             (UINT32 *)Sem,
             Value,
             (UINT32)-1
             ) != Value);

  return Value;
}

/**
  Wait all APs to performs an atomic compare exchange operation to release semaphore.

  @param   NumberOfAPs      AP number

**/
VOID
WaitForAllAPs (
  IN      UINTN  NumberOfAPs
  )
{
  UINTN  BspIndex;

  BspIndex = mSmmMpSyncData->BspIndex;
  while (NumberOfAPs-- > 0) {
    WaitForSemaphore (mSmmMpSyncData->CpuData[BspIndex].Run);
  }
}

/**
  Performs an atomic compare exchange operation to release semaphore
  for each AP.

**/
VOID
ReleaseAllAPs (
  VOID
  )
{
  UINTN  Index;

  for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
    if (IsPresentAp (Index)) {
      ReleaseSemaphore (mSmmMpSyncData->CpuData[Index].Run);
    }
  }
}

/**
  Checks if all CPUs (with certain exceptions) have checked in for this SMI run

  @param   Exceptions     CPU Arrival exception flags.

  @retval   TRUE  if all CPUs the have checked in.
  @retval   FALSE  if at least one Normal AP hasn't checked in.

**/
BOOLEAN
AllCpusInSmmWithExceptions (
  SMM_CPU_ARRIVAL_EXCEPTIONS  Exceptions
  )
{
  UINTN                      Index;
  SMM_CPU_DATA_BLOCK         *CpuData;
  EFI_PROCESSOR_INFORMATION  *ProcessorInfo;

  ASSERT (*mSmmMpSyncData->Counter <= mNumberOfCpus);

  if (*mSmmMpSyncData->Counter == mNumberOfCpus) {
    return TRUE;
  }

  CpuData       = mSmmMpSyncData->CpuData;
  ProcessorInfo = gSmmCpuPrivate->ProcessorInfo;
  for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
    if (!(*(CpuData[Index].Present)) && (ProcessorInfo[Index].ProcessorId != INVALID_APIC_ID)) {
      if (((Exceptions & ARRIVAL_EXCEPTION_DELAYED) != 0) && (SmmCpuFeaturesGetSmmRegister (Index, SmmRegSmmDelayed) != 0)) {
        continue;
      }

      if (((Exceptions & ARRIVAL_EXCEPTION_BLOCKED) != 0) && (SmmCpuFeaturesGetSmmRegister (Index, SmmRegSmmBlocked) != 0)) {
        continue;
      }

      if (((Exceptions & ARRIVAL_EXCEPTION_SMI_DISABLED) != 0) && (SmmCpuFeaturesGetSmmRegister (Index, SmmRegSmmEnable) != 0)) {
        continue;
      }

      return FALSE;
    }
  }

  return TRUE;
}

/**
  Has OS enabled Lmce in the MSR_IA32_MCG_EXT_CTL

  @retval TRUE     Os enable lmce.
  @retval FALSE    Os not enable lmce.

**/
BOOLEAN
IsLmceOsEnabled (
  VOID
  )
{
  MSR_IA32_MCG_CAP_REGISTER          McgCap;
  MSR_IA32_FEATURE_CONTROL_REGISTER  FeatureCtrl;
  MSR_IA32_MCG_EXT_CTL_REGISTER      McgExtCtrl;

  McgCap.Uint64 = AsmReadMsr64 (MSR_IA32_MCG_CAP);
  if (McgCap.Bits.MCG_LMCE_P == 0) {
    return FALSE;
  }

  FeatureCtrl.Uint64 = AsmReadMsr64 (MSR_IA32_FEATURE_CONTROL);
  if (FeatureCtrl.Bits.LmceOn == 0) {
    return FALSE;
  }

  McgExtCtrl.Uint64 = AsmReadMsr64 (MSR_IA32_MCG_EXT_CTL);
  return (BOOLEAN)(McgExtCtrl.Bits.LMCE_EN == 1);
}

/**
  Return if Local machine check exception signaled.

  Indicates (when set) that a local machine check exception was generated. This indicates that the current machine-check event was
  delivered to only the logical processor.

  @retval TRUE    LMCE was signaled.
  @retval FALSE   LMCE was not signaled.

**/
BOOLEAN
IsLmceSignaled (
  VOID
  )
{
  MSR_IA32_MCG_STATUS_REGISTER  McgStatus;

  McgStatus.Uint64 = AsmReadMsr64 (MSR_IA32_MCG_STATUS);
  return (BOOLEAN)(McgStatus.Bits.LMCE_S == 1);
}

/**
  Given timeout constraint, wait for all APs to arrive, and insure when this function returns, no AP will execute normal mode code before
  entering SMM, except SMI disabled APs.

**/
VOID
SmmWaitForApArrival (
  VOID
  )
{
  UINT64   Timer;
  UINTN    Index;
  BOOLEAN  LmceSignal;

  ASSERT (*mSmmMpSyncData->Counter <= mNumberOfCpus);

  LmceSignal = FALSE;
  if (mMachineCheckSupported && IsLmceOsEnabled ()) {
    LmceSignal = IsLmceSignaled ();
  }

  //
  // Platform implementor should choose a timeout value appropriately:
  // - The timeout value should balance the SMM time constrains and the likelihood that delayed CPUs are excluded in the SMM run. Note
  //   the SMI Handlers must ALWAYS take into account the cases that not all APs are available in an SMI run.
  // - The timeout value must, in the case of 2nd timeout, be at least long enough to give time for all APs to receive the SMI IPI
  //   and either enter SMM or buffer the SMI, to insure there is no CPU running normal mode code when SMI handling starts. This will
  //   be TRUE even if a blocked CPU is brought out of the blocked state by a normal mode CPU (before the normal mode CPU received the
  //   SMI IPI), because with a buffered SMI, and CPU will enter SMM immediately after it is brought out of the blocked state.
  // - The timeout value must be longer than longest possible IO operation in the system
  //

  //
  // Sync with APs 1st timeout
  // [Ray] If LMCE is signaled, do not wait. Special case: MC happens locally, doesn't need to wait for other CPUs.
  //     Exit the loop when:
  //        a. all CPU are in SMM
  //        b. all CPU except BLOCKED or DISABLED are in SMM
  //        b. timeout
  //
  for (Timer = StartSyncTimer ();
       !IsSyncTimerTimeout (Timer) && !LmceSignal &&
       !AllCpusInSmmWithExceptions (ARRIVAL_EXCEPTION_BLOCKED | ARRIVAL_EXCEPTION_SMI_DISABLED);
       )
  {
    CpuPause ();
  }

  //
  // Not all APs have arrived, so we need 2nd round of timeout. IPIs should be sent to ALL none present APs,
  // because:
  // a) Delayed AP may have just come out of the delayed state. Blocked AP may have just been brought out of blocked state by some AP running
  //    normal mode code. These APs need to be guaranteed to have an SMI pending to insure that once they are out of delayed / blocked state, they
  //    enter SMI immediately without executing instructions in normal mode. Note traditional flow requires there are no APs doing normal mode
  //    work while SMI handling is on-going.
  //    [Ray] ??? The requirement (no APs doing normal mode work while SMI handling is on-going) is for security consideration or other?
  // b) As a consequence of SMI IPI sending, (spurious) SMI may occur after this SMM run.
  // c) ** NOTE **: Use SMI disabling feature VERY CAREFULLY (if at all) for traditional flow, because a processor in SMI-disabled state
  //    will execute normal mode code, which breaks the traditional SMI handlers' assumption that no APs are doing normal
  //    mode work while SMI handling is on-going.
  // d) We don't add code to check SMI disabling status to skip sending IPI to SMI disabled APs, because:
  //    - In traditional flow, SMI disabling is discouraged.
  //    - In relaxed flow, CheckApArrival() will check SMI disabling status before calling this function. [Ray] ???
  //    In both cases, adding SMI-disabling checking code increases overhead.
  //
  if (*mSmmMpSyncData->Counter < mNumberOfCpus) {
    //
    // Send SMI IPIs to bring outside processors in
    // [Ray] If Local MC doesn't need to involve other CPUs, why we pull other CPU inside SMM here?
    //
    for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
      if (!(*(mSmmMpSyncData->CpuData[Index].Present)) && (gSmmCpuPrivate->ProcessorInfo[Index].ProcessorId != INVALID_APIC_ID)) {
        SendSmiIpi ((UINT32)gSmmCpuPrivate->ProcessorInfo[Index].ProcessorId);
      }
    }

    //
    // Sync with APs 2nd timeout.
    // [Ray] Why is LMCE not checked?
    //
    for (Timer = StartSyncTimer ();
         !IsSyncTimerTimeout (Timer) &&
         !AllCpusInSmmWithExceptions (ARRIVAL_EXCEPTION_BLOCKED | ARRIVAL_EXCEPTION_SMI_DISABLED);
         )
    {
      CpuPause ();
    }
  }

  return;
}

/**
  Replace OS MTRR's with SMI MTRR's.

  @param    CpuIndex             Processor Index

**/
VOID
ReplaceOSMtrrs (
  IN      UINTN  CpuIndex
  )
{
  SmmCpuFeaturesDisableSmrr ();

  //
  // Replace all MTRRs registers
  //
  MtrrSetAllMtrrs (&gSmiMtrrs);
}

/**
  Wheck whether task has been finished by all APs.

  @param       BlockMode   Whether did it in block mode or non-block mode.

  @retval      TRUE        Task has been finished by all APs.
  @retval      FALSE       Task not has been finished by all APs.

**/
BOOLEAN
WaitForAllAPsNotBusy (
  IN BOOLEAN  BlockMode
  )
{
  UINTN  Index;

  for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
    //
    // Ignore BSP and APs which not call in SMM.
    //
    if (!IsPresentAp (Index)) {
      continue;
    }

    if (BlockMode) {
      AcquireSpinLock (mSmmMpSyncData->CpuData[Index].Busy);
      ReleaseSpinLock (mSmmMpSyncData->CpuData[Index].Busy);
    } else {
      if (AcquireSpinLockOrFail (mSmmMpSyncData->CpuData[Index].Busy)) {
        ReleaseSpinLock (mSmmMpSyncData->CpuData[Index].Busy);
      } else {
        return FALSE;
      }
    }
  }

  return TRUE;
}

/**
  Check whether it is an present AP.

  @param   CpuIndex      The AP index which calls this function.

  @retval  TRUE           It's a present AP.
  @retval  TRUE           This is not an AP or it is not present.

**/
BOOLEAN
IsPresentAp (
  IN UINTN  CpuIndex
  )
{
  return ((CpuIndex != gSmmCpuPrivate->SmmCoreEntryContext.CurrentlyExecutingCpu) &&
          *(mSmmMpSyncData->CpuData[CpuIndex].Present));
}

/**
  Clean up the status flags used during executing the procedure.

  @param   CpuIndex      The AP index which calls this function.

**/
VOID
ReleaseToken (
  IN UINTN  CpuIndex
  )
{
  PROCEDURE_TOKEN  *Token;

  Token = mSmmMpSyncData->CpuData[CpuIndex].Token;

  if (InterlockedDecrement (&Token->RunningApCount) == 0) {
    ReleaseSpinLock (Token->SpinLock);
  }

  mSmmMpSyncData->CpuData[CpuIndex].Token = NULL;
}

/**
  Free the tokens in the maintained list.

**/
VOID
ResetTokens (
  VOID
  )
{
  //
  // Reset the FirstFreeToken to the beginning of token list upon exiting SMI.
  //
  gSmmCpuPrivate->FirstFreeToken = GetFirstNode (&gSmmCpuPrivate->TokenList);
}

EFI_STATUS
InternalSmmStartupThisAp (
  IN      EFI_AP_PROCEDURE2  Procedure,
  IN      UINTN              CpuIndex,
  IN OUT  VOID               *ProcArguments OPTIONAL,
  IN      MM_COMPLETION      *Token,
  IN      UINTN              TimeoutInMicroseconds,
  IN OUT  EFI_STATUS         *CpuStatus
  )
{
  PROCEDURE_TOKEN  *ProcToken;

  if (CpuIndex >= gSmmCpuPrivate->SmmCoreEntryContext.NumberOfCpus) {
    DEBUG ((DEBUG_ERROR, "CpuIndex(%d) >= gSmmCpuPrivate->SmmCoreEntryContext.NumberOfCpus(%d)\n", CpuIndex, gSmmCpuPrivate->SmmCoreEntryContext.NumberOfCpus));
    return EFI_INVALID_PARAMETER;
  }

  if (CpuIndex == gSmmCpuPrivate->SmmCoreEntryContext.CurrentlyExecutingCpu) {
    DEBUG ((DEBUG_ERROR, "CpuIndex(%d) == gSmmCpuPrivate->SmmCoreEntryContext.CurrentlyExecutingCpu\n", CpuIndex));
    return EFI_INVALID_PARAMETER;
  }

  if (gSmmCpuPrivate->ProcessorInfo[CpuIndex].ProcessorId == INVALID_APIC_ID) {
    return EFI_INVALID_PARAMETER;
  }

  if (!(*(mSmmMpSyncData->CpuData[CpuIndex].Present))) {
    if (mSmmMpSyncData->EffectiveSyncMode == SmmCpuSyncModeTradition) {
      DEBUG ((DEBUG_ERROR, "!mSmmMpSyncData->CpuData[%d].Present\n", CpuIndex));
    }

    return EFI_INVALID_PARAMETER;
  }

  if (gSmmCpuPrivate->Operation[CpuIndex] == SmmCpuRemove) {
    if (!FeaturePcdGet (PcdCpuHotPlugSupport)) {
      DEBUG ((DEBUG_ERROR, "gSmmCpuPrivate->Operation[%d] == SmmCpuRemove\n", CpuIndex));
    }

    return EFI_INVALID_PARAMETER;
  }

  if ((TimeoutInMicroseconds != 0) && ((mSmmMp.Attributes & EFI_MM_MP_TIMEOUT_SUPPORTED) == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Procedure == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireSpinLock (mSmmMpSyncData->CpuData[CpuIndex].Busy);

  mSmmMpSyncData->CpuData[CpuIndex].Procedure = Procedure;
  mSmmMpSyncData->CpuData[CpuIndex].Parameter = ProcArguments;
  if (Token != NULL) {
    if (Token != &mSmmStartupThisApToken) {
      //
      // When Token points to mSmmStartupThisApToken, this routine is called
      // from SmmStartupThisAp() in non-blocking mode (PcdCpuSmmBlockStartupThisAp == FALSE).
      //
      // In this case, caller wants to startup AP procedure in non-blocking
      // mode and cannot get the completion status from the Token because there
      // is no way to return the Token to caller from SmmStartupThisAp().
      // Caller needs to use its implementation specific way to query the completion status.
      //
      // There is no need to allocate a token for such case so the 3 overheads
      // can be avoided:
      // 1. Call AllocateTokenBuffer() when there is no free token.
      // 2. Get a free token from the token buffer.
      // 3. Call ReleaseToken() in APHandler().
      //
      ProcToken                               = GetFreeToken (1);
      mSmmMpSyncData->CpuData[CpuIndex].Token = ProcToken;
      *Token                                  = (MM_COMPLETION)ProcToken->SpinLock;
    }
  }

  mSmmMpSyncData->CpuData[CpuIndex].Status = CpuStatus;
  if (mSmmMpSyncData->CpuData[CpuIndex].Status != NULL) {
    *mSmmMpSyncData->CpuData[CpuIndex].Status = EFI_NOT_READY;
  }

  ReleaseSemaphore (mSmmMpSyncData->CpuData[CpuIndex].Run); // Signal - 6

  if (Token == NULL) {
    AcquireSpinLock (mSmmMpSyncData->CpuData[CpuIndex].Busy);
    ReleaseSpinLock (mSmmMpSyncData->CpuData[CpuIndex].Busy);
  }

  return EFI_SUCCESS;
}

/**
  SMI handler for BSP.

  @param     CpuIndex         BSP processor Index
  @param     SyncMode         SMM MP sync mode

**/
VOID
BSPHandler (
  IN      UINTN              CpuIndex,
  IN      SMM_CPU_SYNC_MODE  SyncMode
  )
{
  UINTN          Index;
  MTRR_SETTINGS  Mtrrs;
  UINTN          ApCount;
  BOOLEAN        ClearTopLevelSmiResult;
  UINTN          PresentCount;

  ASSERT (CpuIndex == mSmmMpSyncData->BspIndex);
  ApCount = 0;

  //
  // Flag BSP's presence
  //
  *mSmmMpSyncData->InsideSmm = TRUE;

  //
  // Initialize Debug Agent to start source level debug in BSP handler
  //
  InitializeDebugAgent (DEBUG_AGENT_INIT_ENTER_SMI, NULL, NULL);

  //
  // Mark this processor's presence
  //
  *(mSmmMpSyncData->CpuData[CpuIndex].Present) = TRUE;

  //
  // Clear platform top level SMI status bit before calling SMI handlers. If
  // we cleared it after SMI handlers are run, we would miss the SMI that
  // occurs after SMI handlers are done and before SMI status bit is cleared.
  // [Ray] If the SMI status is cleared, how can following code know the source of SMI?
  //   In fact, all ClearTopLevelSmiStatus() is implemented as empty function.
  //
  ClearTopLevelSmiResult = ClearTopLevelSmiStatus ();
  ASSERT (ClearTopLevelSmiResult == TRUE);

  //
  // Set running processor index
  //
  gSmmCpuPrivate->SmmCoreEntryContext.CurrentlyExecutingCpu = CpuIndex;

  //
  // If Traditional Sync Mode or need to configure MTRRs: gather all available APs.
  //
  if ((SyncMode == SmmCpuSyncModeTradition) || SmmCpuFeaturesNeedConfigureMtrrs ()) {
    //
    // Wait for APs to arrive
    //
    SmmWaitForApArrival ();

    //
    // Lock the counter down and retrieve the number of APs
    //
    *mSmmMpSyncData->AllCpusInSync = TRUE;
    ApCount                        = LockdownSemaphore (mSmmMpSyncData->Counter) - 1;

    //
    // Wait for all APs to get ready for programming MTRRs
    // [Ray] Block my execution till all APs are ready.
    WaitForAllAPs (ApCount);  // Wait - 1

    if (SmmCpuFeaturesNeedConfigureMtrrs ()) {
      //
      // Signal all APs it's time for backup MTRRs
      //
      ReleaseAllAPs (); // Signal - 2

      //
      // WaitForSemaphore() may wait for ever if an AP happens to enter SMM at
      // exactly this point. Please make sure PcdCpuSmmMaxSyncLoops has been set
      // to a large enough value to avoid this situation.
      // Note: For HT capable CPUs, threads within a core share the same set of MTRRs.
      // We do the backup first and then set MTRR to avoid race condition for threads
      // in the same core.
      //
      MtrrGetAllMtrrs (&Mtrrs);

      //
      // Wait for all APs to complete their MTRR saving
      //
      WaitForAllAPs (ApCount); // Wait - 3

      //
      // Let all processors program SMM MTRRs together
      //
      ReleaseAllAPs (); // Signal - 4

      //
      // WaitForSemaphore() may wait for ever if an AP happens to enter SMM at
      // exactly this point. Please make sure PcdCpuSmmMaxSyncLoops has been set
      // to a large enough value to avoid this situation.
      //
      ReplaceOSMtrrs (CpuIndex);

      //
      // Wait for all APs to complete their MTRR programming
      //
      WaitForAllAPs (ApCount); // Wait - 5
    }
  }

  //
  // The BUSY lock is initialized to Acquired state
  //
  AcquireSpinLock (mSmmMpSyncData->CpuData[CpuIndex].Busy);

  //
  // Perform the pre tasks
  //
  PerformPreTasks ();

  //
  // Invoke SMM Foundation EntryPoint with the processor information context.
  //
  gSmmCpuPrivate->SmmCoreEntry (&gSmmCpuPrivate->SmmCoreEntryContext);  // Signal - 6 happens inside

  //
  // Make sure all APs have completed their pending none-block tasks
  //
  WaitForAllAPsNotBusy (TRUE);

  //
  // Perform the remaining tasks
  //
  PerformRemainingTasks ();

  //
  // If Relaxed-AP Sync Mode: gather all available APs after BSP SMM handlers are done, and
  // make those APs to exit SMI synchronously. APs which arrive later will be excluded and
  // will run through freely.
  //
  if ((SyncMode != SmmCpuSyncModeTradition) && !SmmCpuFeaturesNeedConfigureMtrrs ()) {
    //
    // Lock the counter down and retrieve the number of APs
    //
    *mSmmMpSyncData->AllCpusInSync = TRUE;
    ApCount                        = LockdownSemaphore (mSmmMpSyncData->Counter) - 1;
    //
    // Make sure all APs have their Present flag set
    //
    while (TRUE) {
      PresentCount = 0;
      for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
        if (*(mSmmMpSyncData->CpuData[Index].Present)) {
          PresentCount++;
        }
      }

      if (PresentCount > ApCount) { // [Ray] ??? How is it possible?
        break;
      }
    }
  }

  //
  // Notify all APs to exit
  //
  *mSmmMpSyncData->InsideSmm = FALSE;
  ReleaseAllAPs ();

  //
  // Wait for all APs to complete their pending tasks
  //
  WaitForAllAPs (ApCount); // Wait - 7

  if (SmmCpuFeaturesNeedConfigureMtrrs ()) {
    //
    // Signal APs to restore MTRRs
    //
    ReleaseAllAPs (); // Signal - 8

    //
    // Restore OS MTRRs
    //
    SmmCpuFeaturesReenableSmrr ();
    MtrrSetAllMtrrs (&Mtrrs);

    //
    // Wait for all APs to complete MTRR programming
    //
    WaitForAllAPs (ApCount); // Wait - 9
  }

  //
  // Stop source level debug in BSP handler, the code below will not be
  // debugged.
  //
  InitializeDebugAgent (DEBUG_AGENT_INIT_EXIT_SMI, NULL, NULL);

  //
  // Signal APs to Reset states/semaphore for this processor
  //
  ReleaseAllAPs (); // Signal - 10

  //
  // Perform pending operations for hot-plug
  //
  SmmCpuUpdate ();

  //
  // Clear the Present flag of BSP
  //
  *(mSmmMpSyncData->CpuData[CpuIndex].Present) = FALSE;

  //
  // Gather APs to exit SMM synchronously. Note the Present flag is cleared by now but
  // WaitForAllAps does not depend on the Present flag.
  //
  WaitForAllAPs (ApCount);  // Wait - 11

  //
  // Reset the tokens buffer.
  //
  ResetTokens ();

  //
  // Reset BspIndex to -1, meaning BSP has not been elected.
  //
  if (FeaturePcdGet (PcdCpuSmmEnableBspElection)) {
    mSmmMpSyncData->BspIndex = (UINT32)-1;
  }

  //
  // Allow APs to check in from this point on
  //
  *mSmmMpSyncData->Counter       = 0;
  *mSmmMpSyncData->AllCpusInSync = FALSE;
}

/**
  SMI handler for AP.

  @param     CpuIndex         AP processor Index.
  @param     ValidSmi         Indicates that current SMI is a valid SMI or not.
  @param     SyncMode         SMM MP sync mode.

**/
VOID
APHandler (
  IN      UINTN              CpuIndex,
  IN      BOOLEAN            ValidSmi,
  IN      SMM_CPU_SYNC_MODE  SyncMode
  )
{
  UINT64         Timer;
  UINTN          BspIndex;
  MTRR_SETTINGS  Mtrrs;
  EFI_STATUS     ProcedureStatus;

  //
  // Timeout BSP
  // [Ray] Wait InsideSmm becomes TRUE or timeout.
  //   Assume BSPHandler() will set InsideSmm to TRUE in this timeout if it runs.
  for (Timer = StartSyncTimer ();
       !IsSyncTimerTimeout (Timer) &&
       !(*mSmmMpSyncData->InsideSmm);
       )
  {
    CpuPause ();
  }

  if (!(*mSmmMpSyncData->InsideSmm)) {
    //
    // BSP timeout in the first round
    //
    if (mSmmMpSyncData->BspIndex != -1) {
      //
      // [Ray] When PcdCpuSmmEnableBspElection is FALSE and CPU[0] doesn't enter to SMM.
      //    When PcdCpuSmmEnableBspElection is TRUE, below SendSmiIpi should not be called
      //        because BSP should be already selected.
      //
      // BSP Index is known
      //
      BspIndex = mSmmMpSyncData->BspIndex;
      ASSERT (CpuIndex != BspIndex);

      //
      // Send SMI IPI to bring BSP in
      //
      SendSmiIpi ((UINT32)gSmmCpuPrivate->ProcessorInfo[BspIndex].ProcessorId);

      //
      // Now clock BSP for the 2nd time
      //
      for (Timer = StartSyncTimer ();
           !IsSyncTimerTimeout (Timer) &&
           !(*mSmmMpSyncData->InsideSmm);
           )
      {
        CpuPause ();
      }

      if (!(*mSmmMpSyncData->InsideSmm)) {
        //
        // Give up since BSP is unable to enter SMM
        // and signal the completion of this AP
        WaitForSemaphore (mSmmMpSyncData->Counter);
        return;
      }
    } else {
      //
      // Don't know BSP index. Give up without sending IPI to BSP.
      // [Ray] PlatformSmmBspElection() chooses the BSP while BSP hasn't been chosen.
      WaitForSemaphore (mSmmMpSyncData->Counter);
      return;
    }
  }

  //
  // BSP is available
  // [Ray] InsideSmm should be TRUE, so BspIndex should be a value other than -1.
  BspIndex = mSmmMpSyncData->BspIndex;
  ASSERT (CpuIndex != BspIndex);

  //
  // Mark this processor's presence
  //
  *(mSmmMpSyncData->CpuData[CpuIndex].Present) = TRUE;

  if ((SyncMode == SmmCpuSyncModeTradition) || SmmCpuFeaturesNeedConfigureMtrrs ()) {
    //
    // Notify BSP of arrival at this point
    //
    ReleaseSemaphore (mSmmMpSyncData->CpuData[BspIndex].Run); // Signal - 1
  }

  if (SmmCpuFeaturesNeedConfigureMtrrs ()) {
    //
    // Wait for the signal from BSP to backup MTRRs
    //
    WaitForSemaphore (mSmmMpSyncData->CpuData[CpuIndex].Run); // Wait - 2

    //
    // Backup OS MTRRs
    //
    MtrrGetAllMtrrs (&Mtrrs);

    //
    // Signal BSP the completion of this AP
    //
    ReleaseSemaphore (mSmmMpSyncData->CpuData[BspIndex].Run); // Signal - 3

    //
    // Wait for BSP's signal to program MTRRs
    //
    WaitForSemaphore (mSmmMpSyncData->CpuData[CpuIndex].Run); // Wait - 4

    //
    // Replace OS MTRRs with SMI MTRRs
    //
    ReplaceOSMtrrs (CpuIndex);

    //
    // Signal BSP the completion of this AP
    //
    ReleaseSemaphore (mSmmMpSyncData->CpuData[BspIndex].Run); // Signal - 5
  }

  while (TRUE) {
    //
    // Wait for something to happen
    //
    WaitForSemaphore (mSmmMpSyncData->CpuData[CpuIndex].Run); // Wait - 6

    //
    // Check if BSP wants to exit SMM
    //
    if (!(*mSmmMpSyncData->InsideSmm)) {
      break;
    }

    //
    // BUSY should be acquired by SmmStartupThisAp()
    //
    ASSERT (
      !AcquireSpinLockOrFail (mSmmMpSyncData->CpuData[CpuIndex].Busy)
      );

    //
    // Invoke the scheduled procedure
    //
    ProcedureStatus = (*mSmmMpSyncData->CpuData[CpuIndex].Procedure)(
         (VOID *)mSmmMpSyncData->CpuData[CpuIndex].Parameter
  );
    if (mSmmMpSyncData->CpuData[CpuIndex].Status != NULL) {
      *mSmmMpSyncData->CpuData[CpuIndex].Status = ProcedureStatus;
    }

    if (mSmmMpSyncData->CpuData[CpuIndex].Token != NULL) {
      ReleaseToken (CpuIndex);
    }

    //
    // Release BUSY
    //
    ReleaseSpinLock (mSmmMpSyncData->CpuData[CpuIndex].Busy);
  }

  if (SmmCpuFeaturesNeedConfigureMtrrs ()) {
    //
    // Notify BSP the readiness of this AP to program MTRRs
    //
    ReleaseSemaphore (mSmmMpSyncData->CpuData[BspIndex].Run); // Signal - 7

    //
    // Wait for the signal from BSP to program MTRRs
    //
    WaitForSemaphore (mSmmMpSyncData->CpuData[CpuIndex].Run); // Wait - 8

    //
    // Restore OS MTRRs
    //
    SmmCpuFeaturesReenableSmrr ();
    MtrrSetAllMtrrs (&Mtrrs);
  }

  //
  // Notify BSP the readiness of this AP to Reset states/semaphore for this processor
  //
  ReleaseSemaphore (mSmmMpSyncData->CpuData[BspIndex].Run); // Signal - 9

  //
  // Wait for the signal from BSP to Reset states/semaphore for this processor
  //
  WaitForSemaphore (mSmmMpSyncData->CpuData[CpuIndex].Run); // Wait - 10

  //
  // Reset states/semaphore for this processor
  //
  *(mSmmMpSyncData->CpuData[CpuIndex].Present) = FALSE;

  //
  // Notify BSP the readiness of this AP to exit SMM
  //
  ReleaseSemaphore (mSmmMpSyncData->CpuData[BspIndex].Run); // Signal - 11
}

/**
  C function for SMI entry, each processor comes here upon SMI trigger.

  @param    CpuIndex              CPU Index

**/
VOID
EFIAPI
SmiRendezvous (
  IN      UINTN  CpuIndex
  )
{
  EFI_STATUS  Status;
  BOOLEAN     ValidSmi;
  BOOLEAN     IsBsp;
  BOOLEAN     BspInProgress;
  UINTN       Index;
  UINTN       Cr2;

  ASSERT (CpuIndex < mMaxNumberOfCpus);

  //
  // Save Cr2 because Page Fault exception in SMM may override its value,
  // when using on-demand paging for above 4G memory.
  //
  Cr2 = 0;
  SaveCr2 (&Cr2);

  //
  // Call the user register Startup function first.
  //
  if (mSmmMpSyncData->StartupProcedure != NULL) {
    mSmmMpSyncData->StartupProcedure (mSmmMpSyncData->StartupProcArgs);
  }

  //
  // Perform CPU specific entry hooks
  //
  SmmCpuFeaturesRendezvousEntry (CpuIndex);

  //
  // Determine if this is a valid SMI
  //
  ValidSmi = PlatformValidSmi ();

  //
  // Determine if BSP has been already in progress. Note this must be checked after
  // ValidSmi because BSP may clear a valid SMI source after checking in.
  //
  BspInProgress = *mSmmMpSyncData->InsideSmm;

  if (!BspInProgress && !ValidSmi) {
    //
    // If we reach here, it means when we sampled the ValidSmi flag, SMI status had not
    // been cleared by BSP in a new SMI run (so we have a truly invalid SMI), or SMI
    // status had been cleared by BSP and an existing SMI run has almost ended. (Note
    // we sampled ValidSmi flag BEFORE judging BSP-in-progress status.) In both cases, there
    // is nothing we need to do.
    // [Ray] Why BspInProgress is checked here?
    //   4 cases:
    //   BspInProcess ValidSmi
    //     0            0   -> do nothing
    //     0            1
    //     1            0
    //     1            1
    //
    goto Exit;
  }

  //
  // Signal presence of this processor
  //
  if (ReleaseSemaphore (mSmmMpSyncData->Counter) == 0) {
    //
    // BSP has already ended the synchronization, so QUIT!!!
    //

    //
    // Wait for BSP's signal to finish SMI
    //
    while (*mSmmMpSyncData->AllCpusInSync) {
      CpuPause ();
    }

    goto Exit;
  }

  //
  // The BUSY lock is initialized to Released state.
  // This needs to be done early enough to be ready for BSP's SmmStartupThisAp() call.
  // E.g., with Relaxed AP flow, SmmStartupThisAp() may be called immediately
  // after AP's present flag is detected.
  //
  InitializeSpinLock (mSmmMpSyncData->CpuData[CpuIndex].Busy);

  if (FeaturePcdGet (PcdCpuSmmProfileEnable)) {
    ActivateSmmProfile (CpuIndex);
  }

  if (BspInProgress) {
    //
    // BSP has been elected. Follow AP path, regardless of ValidSmi flag
    // as BSP may have cleared the SMI status
    // [Ray] Why APHandler() is called here? for efficiency?
    //
    APHandler (CpuIndex, ValidSmi, mSmmMpSyncData->EffectiveSyncMode);
  } else {
    //
    // We have a valid SMI
    //

    //
    // Elect BSP
    //
    IsBsp = FALSE;
    if (FeaturePcdGet (PcdCpuSmmEnableBspElection)) {
      if (!mSmmMpSyncData->SwitchBsp || mSmmMpSyncData->CandidateBsp[CpuIndex]) {
        // [Ray] If no-switch-bsp, the first B is the BSP. otherwise the candidate is the BSP.
        //
        // Call platform hook to do BSP election
        //
        Status = PlatformSmmBspElection (&IsBsp);
        if (EFI_SUCCESS == Status) {
          //
          // Platform hook determines successfully
          //
          if (IsBsp) {
            mSmmMpSyncData->BspIndex = (UINT32)CpuIndex;
          }
        } else {
          //
          // Platform hook fails to determine, use default BSP election method
          //
          InterlockedCompareExchange32 (
            (UINT32 *)&mSmmMpSyncData->BspIndex,
            (UINT32)-1,
            (UINT32)CpuIndex
            );
        }
      }
    }

    // [Ray] mSmmMpSyncData->BspIndex is 0 if PcdCpuSmmEnableBspElection is FALSE.
    // "mSmmMpSyncData->BspIndex == CpuIndex" means this is the BSP
    //
    if (mSmmMpSyncData->BspIndex == CpuIndex) {
      //
      // Clear last request for SwitchBsp.
      //
      if (mSmmMpSyncData->SwitchBsp) {
        mSmmMpSyncData->SwitchBsp = FALSE;
        for (Index = 0; Index < mMaxNumberOfCpus; Index++) {
          mSmmMpSyncData->CandidateBsp[Index] = FALSE;
        }
      }

      if (FeaturePcdGet (PcdCpuSmmProfileEnable)) {
        SmmProfileRecordSmiNum ();
      }

      //
      // BSP Handler is always called with a ValidSmi == TRUE
      //
      BSPHandler (CpuIndex, mSmmMpSyncData->EffectiveSyncMode);
    } else {
      APHandler (CpuIndex, ValidSmi, mSmmMpSyncData->EffectiveSyncMode);
    }
  }

  ASSERT (*mSmmMpSyncData->CpuData[CpuIndex].Run == 0);

  //
  // Wait for BSP's signal to exit SMI
  //
  while (*mSmmMpSyncData->AllCpusInSync) {
    CpuPause ();
  }

Exit:
  SmmCpuFeaturesRendezvousExit (CpuIndex);

  //
  // Restore Cr2
  //
  RestoreCr2 (Cr2);
}
