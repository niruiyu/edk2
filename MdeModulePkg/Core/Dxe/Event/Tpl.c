/** @file
  Task priority (TPL) functions.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DxeMain.h"
#include "Event.h"

//
// It's used to support nested interrupts.
// The bit position is the TPL that was interrupted.
// The bit is set when the TPL is interrupted.
//
// Example 1):
//   Assume system runs at TPL_APPLICATION (4) and there is no pending event.
//
//   Timer interrupt happens. CPU runs to the interrupt context.
//   1. Interrupt context (Interrupted TPL = TPL_APPLICATION):
//     CoreRaiseTpl(TPL_APPLICATION -> TPL_HIGH) is called where
//     mInterruptedTplMask is changed from 0 to 0x10.
//
//     Note: CoreRaiseTpl(TPL_HIGH) could be called from Timer driver, or CoreTimerTick().
//       If it's called from both, the 2nd call in CoreTimerTick() will not change mInterruptedTplMask
//       as it's a TPL raise from TPL_HIGH to TPL_HIGH.
//
//     When CoreRestoreTpl(TPL_HIGH -> TPL_APPLICATION) is called, because there is no pending event it will
//     lower TPL to TPL_APPLICATION, but with interrupts disabled as the TPL_APPLICATION bit is set in
//     mInterruptedTplMask.
//     mInterruptedTplMask is changed from 0x10 to 0.
//
// Example 2):
//   Assume system runs at TPL_APPLICATION (4) and there is only one event at TPL_CALLBACK (8) which will
//   register another event at TPL_NOTIFY (16).
//
//   Timer interrupt happens. CPU runs to the (outer) interrupt context.
//   1. Outer interrupt context (Interrupted TPL = TPL_APPLICATION):
//     CoreRaiseTpl(TPL_APPLICATION -> TPL_HIGH) is called where
//     mInterruptedTplMask is changed from 0 to 0x10.
//
//     When CoreRestoreTpl(TPL_HIGH -> TPL_APPLICATION) is called, because there is one event at
//     TPL_CALLBACK (8) it will lower TPL to TPL_CALLBACK, enable the interrupts and dispatch it.
//
//     The event registers another event associating with TPL_NOTIFY.
//
//     2nd timer interrupt happens. CPU runs to the inner-1/nested-1 interrupt context.
//
//     2. Inner-1/nested-1 interrupt context (Interrupted TPL = TPL_CALLBACK):
//       CoreRaiseTpl(TPL_CALLBACK -> TPL_HIGH) is called where
//       mInterruptedTplMask is changed from 0x10 to 0x110.
//
//       When CoreRestoreTpl(TPL_HIGH -> TPL_CALLBACK) is called, because there is one event at
//       TPL_NOTIFY (16) it will lower TPL to TPL_NOTIFY, enable the interrupts and dispatch it.
//
//       3rd timer interrupt happens. CPU runs to the inner-2/nested-2 interrupt context.
//
//       3. Inner-2/nested-2 interrupt context (Interrupt TPL = TPL_NOTIFY):
//         CoreRaiseTpl(TPL_NOTIFY -> TPL_HIGH) is called where
//         mInterruptedTplMask is changed from 0x110 to 0x10110.
//
//         CoreTimerTick() signals mEfiCheckTimerEvent which queues a event at TPL_HIGH - 1.
//
//         When CoreRestoreTpl(TPL_HIGH -> TPL_NOTIFY) is called, because there is one event at
//         TPL_HIGH - 1 (30) it will lower TPL to TPL_HIGH - 1, enable the interrupts and dispatch it.
//
//         4th timer interrupt happens. CPU runs to the inner-3/nested-3 interrupt context.
//
//           4. Inner-3/nested-3 interrupt context (Interrupt TPL = TPL_HIGH - 1):
//           CoreRaiseTpl(TPL_HIGH - 1 -> TPL_HIGH) is called where
//           mInterruptedTplMask is changed from 0x10110 to 0x40010110.
//
//           When CoreRestoreTpl(TPL_HIGH -> TPL_HIGH - 1) is called, because there is no pending event it will
//           lower TPL to TPL_HIGH - 1, but with interrupts disabled as the (TPL_HIGH - 1) bit is set in
//           mInterruptedTplMask.
//           mInterruptedTplMask is changed from 0x40010110 to 0x10110.
//
//           Arch specific instruction (e.g.: IRET for X86) in the interrupt handler will re-enable the interrupts
//           and return to the inner-2/nested-2 interrupt context.
//
//       3. Inner-2/nested-2 interrupt context continues (Interrupt TPL = TPL_NOTIFY):
//         CoreRestoreTpl (TPL_HIGH -> TPL_NOTIFY) lowers TPL to TPL_NOTIFY with interrupts disabled
//         as the (TPL_NOTIFY) bit is set in mInterruptedTplMask.
//         mInterruptedTplMask is changed from 0x10110 to 0x110.
//
//         Arch specific instruction (e.g.: IRET for X86) in the interrupt handler will re-enable the interrupts
//         and return to the inner-1/nested-1 interrupt context.
//
//     2. Inner-1/nested-1 interrupt context continues (Interrupt TPL = TPL_CALLBACK):
//       CoreRestoreTpl (TPL_HIGH -> TPL_CALLBACK) lowers TPL to TPL_CALLBACK with interrupts disabled
//       as the (TPL_CALLBACK) bit is set in mInterruptedTplMask.
//       mInterruptedTplMask is changed from 0x110 to 0x10.
//       Arch specific instruction (e.g.: IRET for X86) in the interrupt handler will re-enable the interrupts
//       and return to the outer interrupt context.
//
//   1. Outer interrupt context continues (Interrupted TPL = TPL_APPLICATION):
//     CoreRestoreTpl (TPL_HIGH -> TPL_APPLICATION) lowers TPL to TPL_APPLICATION with interrupts disabled
//     as the (TPL_APPLICATION) bit is set in mInterruptedTplMask.
//     mInterruptedTplMask is changed from 0x10 to 0.
//     Arch specific instruction (e.g.: IRET for X86) in the interrupt handler will re-enable the interrupts
//     and return to main flow.
//
volatile static UINTN  mInterruptedTplMask = 0;

/**
  Set Interrupt State.

  @param  Enable  The state of enable or disable interrupt

**/
VOID
CoreSetInterruptState (
  IN BOOLEAN  Enable
  )
{
  EFI_STATUS  Status;
  BOOLEAN     InSmm;

  if (gCpu == NULL) {
    return;
  }

  if (!Enable) {
    gCpu->DisableInterrupt (gCpu);
    return;
  }

  if (gSmmBase2 == NULL) {
    gCpu->EnableInterrupt (gCpu);
    return;
  }

  Status = gSmmBase2->InSmm (gSmmBase2, &InSmm);
  if (!EFI_ERROR (Status) && !InSmm) {
    gCpu->EnableInterrupt (gCpu);
  }
}

/**
  Raise the task priority level to the new level.
  High level is implemented by disabling processor interrupts.

  @param  NewTpl  New task priority level

  @return The previous task priority level

**/
EFI_TPL
EFIAPI
CoreRaiseTpl (
  IN EFI_TPL  NewTpl
  )
{
  EFI_TPL  OldTpl;
  BOOLEAN  InterruptState;

  OldTpl = gEfiCurrentTpl;
  if (OldTpl > NewTpl) {
    DEBUG ((DEBUG_ERROR, "FATAL ERROR - RaiseTpl with OldTpl(0x%x) > NewTpl(0x%x)\n", OldTpl, NewTpl));
    ASSERT (FALSE);
  }

  ASSERT (VALID_TPL (NewTpl));

  //
  // If raising to high level, disable interrupts
  //
  if ((NewTpl >= TPL_HIGH_LEVEL) &&  (OldTpl < TPL_HIGH_LEVEL)) {
    //
    // When gCpu is NULL, State is TRUE.
    // Calling CoreSetInterruptState() with TRUE is safe as CoreSetInterruptState() will directly return
    // when gCpu is NULL.
    //
    InterruptState = TRUE;
    if (gCpu != NULL) {
      gCpu->GetInterruptState (gCpu, &InterruptState);
    }

    if (InterruptState) {
      //
      // Interrupts are currently enabled.
      // Disable them for going to HIGH level.
      //
      CoreSetInterruptState (FALSE);
    } else {
      //
      // Interrupts are already disabled.
      // gEfiCurrentTpl is the "Interrupted TPL".
      // It's a non-nested interrupt if mInterruptedTplMask is 0.
      // It's a nested interrupt otherwise.
      // Nested interrupts are ONLY allowed at TPL > "Interrupted TPL", otherwise
      // stack overflow might occur.
      // If it's a nested interrupt, the "Interrupted TPL" should be higher than
      // the outer's "Interrupted TPL". ASSERT() is to check that.
      //
      ASSERT ((INTN)gEfiCurrentTpl > HighBitSet64 (mInterruptedTplMask));

      //
      // Save the "Interrupted TPL" (TPL that was interrupted).
      //
      mInterruptedTplMask |= (UINTN)(1 << gEfiCurrentTpl);
    }
  }

  //
  // Set the new value
  //
  gEfiCurrentTpl = NewTpl;

  return OldTpl;
}

/**
  Lowers the task priority to the previous value.   If the new
  priority unmasks events at a higher priority, they are dispatched.

  @param  NewTpl  New, lower, task priority

**/
VOID
EFIAPI
CoreRestoreTpl (
  IN EFI_TPL  NewTpl
  )
{
  EFI_TPL  OldTpl;
  EFI_TPL  PendingTpl;

  OldTpl = gEfiCurrentTpl;
  if (NewTpl > OldTpl) {
    DEBUG ((DEBUG_ERROR, "FATAL ERROR - RestoreTpl with NewTpl(0x%x) > OldTpl(0x%x)\n", NewTpl, OldTpl));
    ASSERT (FALSE);
  }

  ASSERT (VALID_TPL (NewTpl));

  //
  // If lowering below HIGH_LEVEL, make sure
  // interrupts are enabled
  //

  if ((OldTpl >= TPL_HIGH_LEVEL) &&  (NewTpl < TPL_HIGH_LEVEL)) {
    gEfiCurrentTpl = TPL_HIGH_LEVEL;
  }

  //
  // Dispatch any pending events
  //
  while (gEventPending != 0) {
    PendingTpl = (UINTN)HighBitSet64 (gEventPending);
    if (PendingTpl <= NewTpl) {
      break;
    }

    gEfiCurrentTpl = PendingTpl;
    if (gEfiCurrentTpl < TPL_HIGH_LEVEL) {
      CoreSetInterruptState (TRUE);
    }

    CoreDispatchEventNotifies (gEfiCurrentTpl);
  }

  //
  // Set the new TPL with interrupt disabled.
  //
  CoreSetInterruptState (FALSE);
  gEfiCurrentTpl = NewTpl;

  //
  // If lowering below HIGH_LEVEL, make sure
  // interrupts are enabled
  //
  if (gEfiCurrentTpl < TPL_HIGH_LEVEL) {
    if ((INTN)gEfiCurrentTpl > HighBitSet64 (mInterruptedTplMask)) {
      //
      // Only enable interrupts if restoring to a level above the highest
      // interrupted TPL level.  This allows interrupt nesting, but only for
      // events at higher TPL level than the current TPL level.
      //
      CoreSetInterruptState (TRUE);
    } else {
      //
      // Clear interrupted TPL level mask, but do not re-enable interrupts here
      // This will return to CoreTimerTick() and interrupts will be re-enabled
      // when the timer interrupt handlers returns from interrupt context.
      //
      ASSERT ((INTN)gEfiCurrentTpl == HighBitSet64 (mInterruptedTplMask));
      mInterruptedTplMask &= ~(UINTN)(1 << gEfiCurrentTpl);
    }
  }
}
