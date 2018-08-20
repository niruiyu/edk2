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

#include "SecMain.h"

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


VOID
SecSetTimer (
  IN  UINT64                  PeriodMs,
  IN  EMU_SET_TIMER_CALLBACK  CallBack
  )
{
}


VOID
SecEnableInterrupt (
  VOID
  )
{
}


VOID
SecDisableInterrupt (
  VOID
  )
{
}


BOOLEAN
SecInterruptEanbled (void)
{
  return FALSE;
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

}


VOID
SecCpuSleep (
  VOID
  )
{
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
  /*
  struct tm *tm;
  time_t t;

  t = time (NULL);
  tm = localtime (&t);

  Time->Year = 1900 + tm->tm_year;
  Time->Month = tm->tm_mon + 1;
  Time->Day = tm->tm_mday;
  Time->Hour = tm->tm_hour;
  Time->Minute = tm->tm_min;
  Time->Second = tm->tm_sec;
  Time->Nanosecond = 0;
  Time->TimeZone = timezone;
  Time->Daylight = (daylight ? EFI_TIME_ADJUST_DAYLIGHT : 0)
    | (tm->tm_isdst > 0 ? EFI_TIME_IN_DAYLIGHT : 0);

  if (Capabilities != NULL) {
    Capabilities->Resolution  = 1;
    Capabilities->Accuracy    = 50000000;
    Capabilities->SetsToZero  = FALSE;
  }*/
}



VOID
SecSetTime (
  IN  EFI_TIME               *Time
  )
{
  // Don't change the time on the system
  // We could save delta to localtime() and have SecGetTime adjust return values?
  return;
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

