/**@file

Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Name:

  WinNtThunk.c

Abstract:

  Since the SEC is the only windows program in our emulation we
  must use a Tiano mechanism to export Win32 APIs to other modules.
  This is the role of the EFI_WIN_NT_THUNK_PROTOCOL.

  The mWinNtThunkTable exists so that a change to EFI_WIN_NT_THUNK_PROTOCOL
  will cause an error in initializing the array if all the member functions
  are not added. It looks like adding a element to end and not initializing
  it may cause the table to be initaliized with the members at the end being
  set to zero. This is bad as jumping to zero will case the NT32 to crash.

  All the member functions in mWinNtThunkTable are Win32
  API calls, so please reference Microsoft documentation.


  gWinNt is a a public exported global that contains the initialized
  data.

**/

#include "Host.h"

//
// This pragma is needed for all the DLL entry points to be asigned to the array.
//  if warning 4232 is not dissabled a warning will be generated as a DLL entry
//  point could be modified dynamically. The SEC does not do that, so we must
//  disable the warning so we can compile the SEC. The previous method was to
//  asign each element in code. The disadvantage to that approach is it's harder
//  to tell if all the elements have been initialized properly.
//
#pragma warning(disable : 4232)
#pragma warning(disable : 4996)

#if __INTEL_COMPILER
#pragma warning ( disable : 144 )
#endif

UINTN
SecWriteStdErr (
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
  BOOL  Success;
  DWORD CharCount;

  CharCount = (DWORD)NumberOfBytes;
  Success = WriteFile (
    GetStdHandle (STD_ERROR_HANDLE),
    Buffer,
    CharCount,
    &CharCount,
    NULL
    );

  return Success ? CharCount : 0;
}


EFI_STATUS
SecConfigStdIn (
  VOID
  )
{
  BOOL     Success;
  DWORD    Mode;

  Success = GetConsoleMode (GetStdHandle (STD_INPUT_HANDLE), &Mode);
  if (Success) {
    Success = SetConsoleMode (
                GetStdHandle (STD_INPUT_HANDLE),
                Mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)
                );
  }
  return Success ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

UINTN
SecWriteStdOut (
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
  BOOL  Success;
  DWORD CharCount;

  CharCount = (DWORD)NumberOfBytes;
  Success = WriteFile (
    GetStdHandle (STD_OUTPUT_HANDLE),
    Buffer,
    CharCount,
    &CharCount,
    NULL
    );

  return Success ? CharCount : 0;
}

UINTN
SecReadStdIn (
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
  /*
  BOOL  Success;
  DWORD CharCount;

  CharCount = (DWORD)NumberOfBytes;
  Success = ReadFile (
    GetStdHandle (STD_INPUT_HANDLE),
    Buffer,
    CharCount,
    &CharCount,
    NULL
    );

  return Success ? CharCount : 0;
  */
  return 0;
}

BOOLEAN
SecPollStdIn (
  VOID
  )
{
  ASSERT (FALSE);
  return TRUE;
}


VOID *
SecAlloc (
  IN  UINTN Size
  )
{
  return malloc ((size_t)Size);
}

VOID *
SecAlignedAlloc (
  IN  UINTN Size,
  IN  UINTN Alignment
  )
{
  return _aligned_malloc ((size_t)Size, (size_t)Alignment);
}

BOOLEAN
SecFree (
  IN  VOID *Ptr
  )
{
  if (EfiSystemMemoryRange (Ptr)) {
    // If an address range is in the EFI memory map it was alloced via EFI.
    // So don't free those ranges and let the caller know.
    return FALSE;
  }

  free (Ptr);
  return TRUE;
}

BOOLEAN
SecAlignedFree (
  IN  VOID *Ptr
  )
{
  if (EfiSystemMemoryRange (Ptr)) {
    // If an address range is in the EFI memory map it was alloced via EFI.
    // So don't free those ranges and let the caller know.
    return FALSE;
  }

  _aligned_free (Ptr);
  return TRUE;
}

//
// Define a global that we can use to shut down the NT timer thread when
// the timer is canceled.
//
BOOLEAN                 mCancelTimerThread = FALSE;

//
// The notification function to call on every timer interrupt
//
EMU_SET_TIMER_CALLBACK  *mTimerNotifyFunction = NULL;

//
// The thread handle for this driver
//
HANDLE                  mNtMainThreadHandle;

//
// The timer value from the last timer interrupt
//
UINT32                  mNtLastTick;

//
// Critical section used to update varibles shared between the main thread and
// the timer interrupt thread.
//
CRITICAL_SECTION        mNtCriticalSection;

//
// Worker Functions
//
UINT                    mMMTimerThreadID = 0;

volatile BOOLEAN        mInterruptEnabled = FALSE;

VOID
CALLBACK
MMTimerThread (
  UINT  wTimerID,
  UINT  msg,
  DWORD dwUser,
  DWORD dw1,
  DWORD dw2
)
{
  UINT32            CurrentTick;
  UINT32            Delta;

  if (!mCancelTimerThread) {

    //
    // Suspend the main thread until we are done.
    // Enter the critical section before suspending
    // and leave the critical section after resuming
    // to avoid deadlock between main and timer thread.
    //
    EnterCriticalSection (&mNtCriticalSection);
    SuspendThread (mNtMainThreadHandle);

    //
    // If the timer thread is being canceled, then bail immediately.
    // We check again here because there's a small window of time from when
    // this thread was kicked off and when we suspended the main thread above.
    //
    if (mCancelTimerThread) {
      ResumeThread (mNtMainThreadHandle);
      LeaveCriticalSection (&mNtCriticalSection);
      timeKillEvent (wTimerID);
      mMMTimerThreadID = 0;
      return;
    }

    while (!mInterruptEnabled) {
      //
      //  Resume the main thread
      //
      ResumeThread (mNtMainThreadHandle);
      LeaveCriticalSection (&mNtCriticalSection);

      //
      //  Wait for interrupts to be enabled.
      //
      while (!mInterruptEnabled) {
        Sleep (1);
      }

      //
      //  Suspend the main thread until we are done
      //
      EnterCriticalSection (&mNtCriticalSection);
      SuspendThread (mNtMainThreadHandle);
    }

    //
    //  Get the current system tick
    //
    CurrentTick = GetTickCount ();
    Delta = CurrentTick - mNtLastTick;
    mNtLastTick = CurrentTick;

    //
    //  If delay was more then 1 second, ignore it (probably debugging case)
    //
    if (Delta < 1000) {

      //
      // Only invoke the callback function if a Non-NULL handler has been
      // registered. Assume all other handlers are legal.
      //
      if (mTimerNotifyFunction != NULL) {
        mTimerNotifyFunction (Delta);
      }
    }

    //
    //  Resume the main thread
    //
    ResumeThread (mNtMainThreadHandle);
    LeaveCriticalSection (&mNtCriticalSection);
  } else {
    timeKillEvent (wTimerID);
    mMMTimerThreadID = 0;
  }

}

VOID
SecSetTimer (
  IN  UINT64                  TimerPeriod,
  IN  EMU_SET_TIMER_CALLBACK  Callback
)
{
  //
// If TimerPeriod is 0, then the timer thread should be canceled
//
  if (TimerPeriod == 0) {
    //
    // Cancel the timer thread
    //
    EnterCriticalSection (&mNtCriticalSection);

    mCancelTimerThread = TRUE;

    LeaveCriticalSection (&mNtCriticalSection);

    //
    // Wait for the timer thread to exit
    //

    if (mMMTimerThreadID != 0) {
      timeKillEvent (mMMTimerThreadID);
      mMMTimerThreadID = 0;
    }
  } else {
    //
    // If the TimerPeriod is valid, then create and/or adjust the period of the timer thread
    //
    EnterCriticalSection (&mNtCriticalSection);

    mCancelTimerThread = FALSE;

    LeaveCriticalSection (&mNtCriticalSection);

    //
    //  Get the starting tick location if we are just starting the timer thread
    //
    mNtLastTick = GetTickCount ();

    if (mMMTimerThreadID) {
      timeKillEvent (mMMTimerThreadID);
    }

    SetThreadPriority (
      GetCurrentThread (),
      THREAD_PRIORITY_HIGHEST
    );

    mMMTimerThreadID = timeSetEvent (
      (UINT)TimerPeriod,
      0,
      MMTimerThread,
      (DWORD_PTR)NULL,
      TIME_PERIODIC | TIME_KILL_SYNCHRONOUS | TIME_CALLBACK_FUNCTION
    );
  }
  mTimerNotifyFunction = Callback;
}

VOID
SecInitializeThunk (
  VOID
)
{
  InitializeCriticalSection (&mNtCriticalSection);

  DuplicateHandle (
    GetCurrentProcess (),
    GetCurrentThread (),
    GetCurrentProcess (),
    &mNtMainThreadHandle,
    0,
    FALSE,
    DUPLICATE_SAME_ACCESS
  );
}

VOID
SecEnableInterrupt (
  VOID
  )
{
  mInterruptEnabled = TRUE;
}


VOID
SecDisableInterrupt (
  VOID
  )
{
  mInterruptEnabled = FALSE;
}


UINT64
SecQueryPerformanceFrequency (
  VOID
  )
{
  // Hard code to nanoseconds
  return 1000000000ULL;
}

UINT64
SecQueryPerformanceCounter (
  VOID
  )
{
  return 0;
}



VOID
SecSleep (
  IN  UINT64 Nanoseconds
  )
{
  Sleep ((DWORD)DivU64x32 (Nanoseconds, 1000000));
}


VOID
SecCpuSleep (
  VOID
  )
{
  Sleep (1);
}


VOID
SecExit (
  UINTN   Status
  )
{
  exit ((int)Status);
}


VOID
SecGetTime (
  OUT  EFI_TIME               *Time,
  OUT EFI_TIME_CAPABILITIES   *Capabilities OPTIONAL
  )
{
  SYSTEMTIME            SystemTime;
  TIME_ZONE_INFORMATION TimeZone;

  GetLocalTime (&SystemTime);
  GetTimeZoneInformation (&TimeZone);

  Time->Year = (UINT16)SystemTime.wYear;
  Time->Month = (UINT8)SystemTime.wMonth;
  Time->Day = (UINT8)SystemTime.wDay;
  Time->Hour = (UINT8)SystemTime.wHour;
  Time->Minute = (UINT8)SystemTime.wMinute;
  Time->Second = (UINT8)SystemTime.wSecond;
  Time->Nanosecond = (UINT32)(SystemTime.wMilliseconds * 1000000);
  Time->TimeZone = (INT16)TimeZone.Bias;

  if (Capabilities != NULL) {
    Capabilities->Resolution = 1;
    Capabilities->Accuracy = 50000000;
    Capabilities->SetsToZero = FALSE;
  }

  Time->Daylight = 0;
  if (TimeZone.StandardDate.wMonth) {
    Time->Daylight = (UINT8)TimeZone.StandardDate.wMonth;
  }
}

EFI_STATUS
SecSetTime (
  IN  EFI_TIME               *Time
  )
{
  TIME_ZONE_INFORMATION TimeZone;
  SYSTEMTIME            SystemTime;
  BOOL                  Flag;

  //
  // Set Daylight savings time information and Time Zone
  //
  GetTimeZoneInformation (&TimeZone);
  TimeZone.StandardDate.wMonth = Time->Daylight;
  TimeZone.Bias = Time->TimeZone;
  Flag = SetTimeZoneInformation (&TimeZone);
  if (!Flag) {
    return EFI_DEVICE_ERROR;
  }

  SystemTime.wYear = Time->Year;
  SystemTime.wMonth = Time->Month;
  SystemTime.wDay = Time->Day;
  SystemTime.wHour = Time->Hour;
  SystemTime.wMinute = Time->Minute;
  SystemTime.wSecond = Time->Second;
  SystemTime.wMilliseconds = (INT16)(Time->Nanosecond / 1000000);

  Flag = SetLocalTime (&SystemTime);

  if (!Flag) {
    return EFI_DEVICE_ERROR;
  } else {
    return EFI_SUCCESS;
  }
}

EMU_THUNK_PROTOCOL gEmuThunkProtocol = {
  SecWriteStdErr,
  SecConfigStdIn,
  SecWriteStdOut,
  SecReadStdIn,
  SecPollStdIn,
  SecAlloc,
  SecAlignedAlloc,
  SecFree,
  SecAlignedFree,
  SecPeCoffGetEntryPoint,
  PeCoffLoaderRelocateImageExtraAction,
  PeCoffLoaderUnloadImageExtraAction,
  SecEnableInterrupt,
  SecDisableInterrupt,
  SecQueryPerformanceFrequency,
  SecQueryPerformanceCounter,
  SecSleep,
  SecCpuSleep,
  SecExit,
  SecGetTime,
  SecSetTime,
  SecSetTimer,
  GetNextThunkProtocol
};


#pragma warning(default : 4996)
#pragma warning(default : 4232)

