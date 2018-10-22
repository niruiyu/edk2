#ifndef _PEI_SEMAPHORE_LIB_H_
#define _PEI_SEMAPHORE_LIB_H_

#include <Uefi.h>
#include <Register/Msr.h>
#include <Library/BaseLib.h>
#include <Library/SemaphoreLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>
#include <Library/LocalApicLib.h>

#define SEMAPHORE_IDT_ENTRY_INDEX   34

typedef struct {
  UINT32  Signature;
  UINT32  Count;
} SEMAPHORE_INSTANCE;
#define SEMAPHORE_SIGNATURE  SIGNATURE_32 ('_', 's', 'e', 'm')

typedef struct {
  GUID               Name;
  SEMAPHORE_INSTANCE Instance;
} NAMED_SEMAPHORE_INSTANCE;

/**
  Return TRUE when current CPU is BSP.

  @retval TRUE  Current CPU is BSP.
  @retval FALSE Current CPU is AP.
**/
BOOLEAN
MutexLibIsBsp (
  VOID
  );
#endif // _PEI_SEMAPHORE_LIB_H_
