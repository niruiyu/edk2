#ifndef _PEI_SEMAPHORE_LIB_H_
#define _PEI_SEMAPHORE_LIB_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/SemaphoreLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>

#define SEMAPHORE_LIB_DATABASE_GUID { 0x42b28b9a, 0xf409, 0x494d,{ 0x8f, 0x71, 0x46, 0x54, 0x34, 0xec, 0xb1, 0x7f } }

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


#define MAX_INSTANCE 10

typedef struct {
  NAMED_SEMAPHORE_INSTANCE  NamedInstance[MAX_INSTANCE];
} SEMAPHORE_ARRAY;

#endif // _PEI_SEMAPHORE_LIB_H_