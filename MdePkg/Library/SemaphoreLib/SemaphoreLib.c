/** @file
  Provides base semaphore library implementation.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD
License which accompanies this distribution.  The full text of the license may
be found at http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/SemaphoreLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>
#include <Library/DebugLib.h>


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
  Attempts acquire a semaphore.

  This function checks the count of the semaphore.
  If the count is non-zero, it is decreased and the routine returns.
  If the count is zero, the routine returns RETURN_NOT_READY.

  @param  Semaphore             The semaphore to acquire.

  @retval TRUE                  The semaphore is acquired successfully.
  @retval FALSE                 The semaphore cannot be acquired.
**/
BOOLEAN
EFIAPI
SemaphoreAcquireOrFail (
  IN SEMAPHORE *Semaphore
  )
{
  UINT32       OriginalValue;

  ASSERT (Semaphore != NULL);

  OriginalValue = *Semaphore;
  return (BOOLEAN)(
    (OriginalValue != 0) &&
    (InterlockedCompareExchange32 (Semaphore, OriginalValue, OriginalValue - 1) == OriginalValue)
    );
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
  @retval RETURN_TIMEOUT           The semaphore cannot be acquired in specified timeout.
**/
RETURN_STATUS
EFIAPI
SemaphoreAcquire (
  IN SEMAPHORE *Semaphore,
  IN UINT64    TimeoutInMicroSeconds
  )
{
  UINT64             NumberOfTicks;
  UINT32             Remainder;
  UINT64             StartTick;
  UINT64             EndTick;
  UINT64             CurrentTick;
  UINT64             ElapsedTick;

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
      if (SemaphoreAcquireOrFail (Semaphore)) {
        break;
      }
      CpuPause ();
    }
    if (ElapsedTick > NumberOfTicks) {
      return RETURN_TIMEOUT;
    }
  } else {
    while (!SemaphoreAcquireOrFail (Semaphore)) {
      CpuPause ();
    }
  }
  return RETURN_SUCCESS;
}

/**
  Release a semaphore.

  This function decreases the count of the semaphore.

  @param  Semaphore             The semaphore to release.
**/
VOID
EFIAPI
SemaphoreRelease (
  IN SEMAPHORE *Semaphore
  )
{
  UINT32             OriginalValue;

  ASSERT (Semaphore != NULL);

  do {
    OriginalValue = *Semaphore;
  } while (OriginalValue != InterlockedCompareExchange32 (Semaphore, OriginalValue, OriginalValue + 1));
}
/**
  Initialize a semaphore.

  If Semaphore is NULL, then ASSERT().

  @param  Semaphore    The semaphore to be initialized.
  @param  InitialCount The count of resources available for the semaphore.
**/
VOID
EFIAPI
SemaphoreInitialize (
  IN OUT SEMAPHORE  *Semaphore,
  IN  UINT32        InitialCount
  )
{
  ASSERT (Semaphore != NULL);
  *Semaphore = InitialCount;
}
