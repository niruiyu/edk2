#ifndef _PEI_SEMAPHORE_LIB_H_
#define _PEI_SEMAPHORE_LIB_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/SemaphoreLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>

typedef struct {
  UINT32  Signature;
  UINT32  Count;
} SEMAPHORE_INSTANCE;

#define SEMAPHORE_SIGNATURE  SIGNATURE_32 ('_', 's', 'e', 'm')

#endif // _PEI_SEMAPHORE_LIB_H_