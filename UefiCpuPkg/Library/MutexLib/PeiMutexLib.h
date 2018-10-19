#ifndef _PEI_MUTEX_LIB_H_
#define _PEI_MUTEX_LIB_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/MutexLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>
#include <Library/LocalApicLib.h>

#define SEMAPHORE_LIB_DATABASE_GUID { 0x42b28b9a, 0xf409, 0x494d,{ 0x8f, 0x71, 0x46, 0x54, 0x34, 0xec, 0xb1, 0x7f } }

#define MUTEX_IDT_ENTRY_INDEX   35

typedef struct {
  UINT32    Signature;
  UINT64    OwnerAndCount;
} MUTEX_INSTANCE;

#define MUTEX_COUNT(x)  (UINT32)(x)
#define MUTEX_OWNER(x)  (UINT32)RShiftU64 (x, 32)

#define MUTEX_RELEASED   0xFFFFFFFF00000000ull
#define MUTEX_SIGNATURE  SIGNATURE_32 ('m', 'u', 't', 'x')

typedef struct {
  GUID               Name;
  MUTEX_INSTANCE     Instance;
} NAMED_MUTEX_INSTANCE;

#define MAX_INSTANCE 10

typedef struct {
  NAMED_MUTEX_INSTANCE  NamedInstance[MAX_INSTANCE];
} MUTEX_ARRAY;
#endif // _PEI_MUTEX_LIB_H_