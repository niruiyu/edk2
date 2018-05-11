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

#include "PeiSemaphoreLib.h"

/**
  Internal function to acquire a semaphore.

  @param Count  Pointer to count of the semaphore.

  @retval TRUE  The semaphore is acquired.
  @retval FALSE The semaphore is not acquired.
**/
BOOLEAN
SemaphoreLibSemaphoreAcquire (
  IN  UINT32           *Count
)
{
  UINT32               OriginalCount;

  OriginalCount = *Count;
  return (BOOLEAN)(
    (OriginalCount != 0) &&
    (InterlockedCompareExchange32 (Count, OriginalCount, OriginalCount - 1) == OriginalCount)
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
SemaphoreLibMultThenDivU64x64x32 (
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
    Uint64 = SemaphoreLibMultThenDivU64x64x32 (RShiftU64 (Multiplicand, 1), Multiplier, Divisor, &LocalRemainder);
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
SemaphoreLibGetElapsedTick (
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
  Acquire a semaphore.

  This function checks the count of the semaphore.
  If the count is non-zero, it is decremented and the routine returns.
  If the count is zero, the routine waits specified timeout for the count to be non-zero, and then places it in the
  acquired state.

  @param  Semaphore             The semaphore to acquire.
  @param  TimeoutInMicroSeconds The timeout in micro seconds.
                                0 to wait infinitely.

  @retval RETURN_SUCCESS           The semaphore is acquired successfully.
  @retval RETURN_INVALID_PARAMETER The semaphore is not created by SemaphoreCreate().
  @retval RETURN_TIMEOUT           The semaphore cannot be acquired in specified timeout.
**/
RETURN_STATUS
EFIAPI
SemaphoreAcquire (
  IN SEMAPHORE Semaphore,
  IN UINT64    TimeoutInMicroSeconds
  )
{
  SEMAPHORE_INSTANCE *Instance;
  UINT64             NumberOfTicks;
  UINT32             Remainder;
  UINT64             StartTick;
  UINT64             EndTick;
  UINT64             CurrentTick;
  UINT64             ElapsedTick;

  Instance = (SEMAPHORE_INSTANCE *)Semaphore;
  //
  // The function could be called from AP.
  // So accessing HOB to verify the Semaphore is impossible.
  //
  if ((Instance == NULL) || (Instance->Signature != SEMAPHORE_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (TimeoutInMicroSeconds != 0) {
    NumberOfTicks = SemaphoreLibMultThenDivU64x64x32 (
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
        ; ElapsedTick += SemaphoreLibGetElapsedTick (&CurrentTick, StartTick, EndTick)
      ) {
      if (SemaphoreLibSemaphoreAcquire (&Instance->Count)) {
        break;
      }
      CpuPause ();
    }
    if (ElapsedTick > NumberOfTicks) {
      return RETURN_TIMEOUT;
    }
  } else {
    while (!SemaphoreLibSemaphoreAcquire (&Instance->Count)) {
      CpuPause ();
    }
  }
  return RETURN_SUCCESS;
}

/**
  Attempts acquire a semaphore.

  This function checks the count of the semaphore.
  If the count is non-zero, it is decreased and the routine returns.
  If the count is zero, the routine returns RETURN_NOT_READY.

  @param  Semaphore             The semaphore to acquire.

  @retval RETURN_SUCCESS           The semaphore is acquired successfully.
  @retval RETURN_NOT_READY         The semaphore cannot be acquired.
  @retval RETURN_INVALID_PARAMETER The semaphore is not created by SemaphoreCreate().

**/
RETURN_STATUS
EFIAPI
SemaphoreAcquireOrFail (
  IN SEMAPHORE Semaphore
  )
{
  SEMAPHORE_INSTANCE *Instance;

  Instance = (SEMAPHORE_INSTANCE *)Semaphore;
  //
  // The function could be called from AP.
  // So accessing HOB to verify the Semaphore is impossible.
  //
  if ((Instance == NULL) || (Instance->Signature != SEMAPHORE_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  if (SemaphoreLibSemaphoreAcquire (&Instance->Count)) {
    return RETURN_SUCCESS;
  }
  return RETURN_NOT_READY;
}

/**
  Release a semaphore.

  This function decreases the count of the semaphore.

  @param  Semaphore             The semaphore to release.

  @retval RETURN_SUCCESS           The semaphore is released successfully.
  @retval RETURN_INVALID_PARAMETER The semaphore is not created by SemaphoreCreate().

**/
RETURN_STATUS
EFIAPI
SemaphoreRelease (
  IN SEMAPHORE Semaphore
  )
{
  UINT32             OriginalCount;
  SEMAPHORE_INSTANCE *Instance;

  Instance = (SEMAPHORE_INSTANCE *)Semaphore;
  //
  // The function could be called from AP.
  // So accessing HOB to verify the Semaphore is impossible.
  //
  if ((Instance == NULL) || (Instance->Signature != SEMAPHORE_SIGNATURE)) {
    return RETURN_INVALID_PARAMETER;
  }

  do {
    OriginalCount = Instance->Count;
  } while (OriginalCount != InterlockedCompareExchange32 (
                              &Instance->Count,
                              OriginalCount,
                              OriginalCount + 1
                              ));

  return RETURN_SUCCESS;
}
