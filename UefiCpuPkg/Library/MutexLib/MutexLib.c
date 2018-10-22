/** @file
  Provides general mutex library implementation.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD
License which accompanies this distribution.  The full text of the license may
be found at http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "PeiMutexLib.h"

/**
  Return TRUE when current CPU is BSP.

  @retval TRUE  Current CPU is BSP.
  @retval FALSE Current CPU is AP.
**/
BOOLEAN
MutexLibIsBsp (
  VOID
  )
{
  MSR_IA32_APIC_BASE_REGISTER  MsrApicBase;

  MsrApicBase.Uint64 = AsmReadMsr64 (MSR_IA32_APIC_BASE);
  return (BOOLEAN)(MsrApicBase.Bits.BSP == 1);
}

/**
  Internal function to acquire a mutex.

  @param Count  Pointer to count of the mutex.

  @retval TRUE  The mutex is acquired.
  @retval FALSE The mutex is not acquired.
**/
BOOLEAN
MutexLibMutexAcquire (
  IN  MUTEX_INSTANCE   *Instance
)
{
  UINT64               OriginalValue;
  UINT64               ExpectedValue;
  UINT64               NewValue;
  UINT32               Owner;

  Owner = GetApicId ();
  OriginalValue = Instance->OwnerAndCount;
  if (MUTEX_OWNER (OriginalValue) == Owner) {
    if (MUTEX_COUNT (OriginalValue) == MAX_UINT32) {
      CpuDeadLoop ();
    }
    ExpectedValue = OriginalValue;
    NewValue = OriginalValue + 1;
  } else {
    ExpectedValue = MUTEX_RELEASED;
    NewValue = LShiftU64 (Owner, 32) | 1;
  }

  return (BOOLEAN) (
    (OriginalValue == ExpectedValue) &&
    (InterlockedCompareExchange64 (&Instance->OwnerAndCount, OriginalValue, NewValue) == OriginalValue)
    );
}


/**
  Return the result of (Multiplicand * Multiplier / Divisor).

  @param Multiplicand A 64-bit unsigned value.
  @param Multiplier   A 64-bit unsigned value.
  @param Divisor      A 32-bit unsigned value.
  @param Remainder    A pointer to a 32-bit unsigned value. This parameter is
                      optional and may be NULL.

  @return Multiplicand * Multiplier / Divisor.
**/
UINT64
MutexLibMultThenDivU64x64x32 (
  IN      UINT64                    Multiplicand,
  IN      UINT64                    Multiplier,
  IN      UINT32                    Divisor,
  OUT     UINT32                    *Remainder  OPTIONAL
  )
{
  UINT64                            Uint64;
  UINT32                            LocalRemainder;
  UINT32                            Uint32;

  if (Multiplicand > DivU64x64Remainder (MAX_UINT64, Multiplier, NULL)) {
    //
    // Make sure Multiplicand is the bigger one.
    //
    if (Multiplicand < Multiplier) {
      Uint64       = Multiplicand;
      Multiplicand = Multiplier;
      Multiplier   = Uint64;
    }
    //
    // Because Multiplicand * Multiplier overflows,
    //   Multiplicand * Multiplier / Divisor
    // = (2 * Multiplicand' + 1) * Multiplier / Divisor
    // = 2 * (Multiplicand' * Multiplier / Divisor) + Multiplier / Divisor
    //
    Uint64 = MutexLibMultThenDivU64x64x32 (RShiftU64 (Multiplicand, 1), Multiplier, Divisor, &LocalRemainder);
    Uint64 = LShiftU64 (Uint64, 1);
    Uint32 = 0;
    if ((Multiplicand & 0x1) == 1) {
      Uint64 += DivU64x32Remainder (Multiplier, Divisor, &Uint32);
    }
    return Uint64 + DivU64x32Remainder (Uint32 + LShiftU64 (LocalRemainder, 1), Divisor, Remainder);
  } else {
    return DivU64x32Remainder (MultU64x64 (Multiplicand, Multiplier), Divisor, Remainder);
  }
}

/**
  Return the elapsed tick count from CurrentTick.

  @param  CurrentTick  On input, the previous tick count.
                       On output, the current tick count.
  @param  StartTick    The value the performance counter starts with when it
                       rolls over.
  @param  EndTick      The value that the performance counter ends with before
                       it rolls over.

  @return  The elapsed tick count from CurrentTick.
**/
UINT64
MutexLibGetElapsedTick (
  UINT64  *CurrentTick,
  UINT64  StartTick,
  UINT64  EndTick
  )
{
  UINT64  PreviousTick;

  PreviousTick = *CurrentTick;
  *CurrentTick = GetPerformanceCounter();
  if (StartTick < EndTick) {
    return *CurrentTick - PreviousTick;
  } else {
    return PreviousTick - *CurrentTick;
  }
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
  @retval RETURN_INVALID_PARAMETER The mutex is not created by MutexCreate().
  @retval RETURN_TIMEOUT           The mutex cannot be acquired in specified timeout.
**/
RETURN_STATUS
EFIAPI
MutexAcquire (
  IN MUTEX  Mutex,
  IN UINT64 TimeoutInMicroSeconds
  )
{
  MUTEX_INSTANCE     *Instance;
  UINT64             NumberOfTicks;
  UINT32             Remainder;
  UINT64             StartTick;
  UINT64             EndTick;
  UINT64             CurrentTick;
  UINT64             ElapsedTick;

  Instance = (MUTEX_INSTANCE *)Mutex;
  //
  // The function could be called from AP.
  // So accessing HOB to verify the Mutex is impossible.
  //
  if ((Instance == NULL) || (Instance->Signature != MUTEX_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (TimeoutInMicroSeconds != 0) {
    NumberOfTicks = MutexLibMultThenDivU64x64x32 (
                      GetPerformanceCounterProperties (&StartTick, &EndTick),
                      TimeoutInMicroSeconds,
                      1000000,
                      &Remainder
                      );
    if (Remainder >= 1000000 / 2) {
      NumberOfTicks++;
    }
    for ( ElapsedTick = 0, CurrentTick = GetPerformanceCounter ()
        ; ElapsedTick <= NumberOfTicks
        ; ElapsedTick += MutexLibGetElapsedTick (&CurrentTick, StartTick, EndTick)
      ) {
      if (MutexLibMutexAcquire (Instance)) {
        break;
      }
      CpuPause ();
    }
    if (ElapsedTick > NumberOfTicks) {
      return RETURN_TIMEOUT;
    }
  } else {
    while (!MutexLibMutexAcquire (Instance)) {
      CpuPause ();
    }
  }
  return RETURN_SUCCESS;
}

/**
  Attempts acquire a mutex.

  This function checks the count of the mutex.
  If the count is non-zero, it is decreased and the routine returns.
  If the count is zero, the routine returns RETURN_NOT_READY.

  @param  Mutex             The mutex to acquire.

  @retval RETURN_SUCCESS           The mutex is acquired successfully.
  @retval RETURN_INVALID_PARAMETER The mutex is not created by MutexCreate().

**/
RETURN_STATUS
EFIAPI
MutexAcquireOrFail (
  IN MUTEX Mutex
  )
{
  MUTEX_INSTANCE *Instance;

  Instance = (MUTEX_INSTANCE *)Mutex;
  //
  // The function could be called from AP.
  // So accessing HOB to verify the Mutex is impossible.
  //
  if ((Instance == NULL) || (Instance->Signature != MUTEX_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (MutexLibMutexAcquire (Instance)) {
    return RETURN_SUCCESS;
  }
  return RETURN_NOT_READY;
}

/**
  Release a mutex.

  This function decreases the count of the mutex.

  @param  Mutex             The mutex to release.

  @retval RETURN_SUCCESS           The mutex is released successfully.
  @retval RETURN_INVALID_PARAMETER The mutex is not created by MutexCreate().
  @retval RETURN_ACCESS_DENIED     The mutex is not owned by current thread.

**/
RETURN_STATUS
EFIAPI
MutexRelease (
  IN MUTEX Mutex
  )
{
  UINT64         OriginalValue;
  UINT64         NewValue;
  MUTEX_INSTANCE *Instance;
  UINT32         Owner;

  Instance = (MUTEX_INSTANCE *)Mutex;
  //
  // The function could be called from AP.
  // So accessing HOB to verify the Mutex is impossible.
  //
  if ((Instance == NULL) || (Instance->Signature != MUTEX_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  Owner = GetApicId ();
  if (MUTEX_OWNER (Instance->OwnerAndCount) != Owner) {
    //
    // It covers two cases:
    // 1. The mutex is owned by another one.
    // 2. The mutex is in released state (not owned by me).
    //
    return RETURN_ACCESS_DENIED;
  }

  if (MUTEX_COUNT (Instance->OwnerAndCount) == 0) {
    //
    // Impossible
    //
    CpuDeadLoop ();
  }

  do {
    OriginalValue = Instance->OwnerAndCount;
    if (MUTEX_COUNT (OriginalValue) == 1) {
      NewValue = MUTEX_RELEASED;
    } else {
      NewValue = OriginalValue - 1;
    }
  } while (OriginalValue != InterlockedCompareExchange64 (&Instance->OwnerAndCount, OriginalValue, NewValue));
  return RETURN_SUCCESS;
}
