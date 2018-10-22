#ifndef _PEI_MUTEX_LIB_H_
#define _PEI_MUTEX_LIB_H_

#include <Uefi.h>
#include <Register/Msr.h>
#include <Library/BaseLib.h>
#include <Library/MutexLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>
#include <Library/LocalApicLib.h>

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

/**
  Return TRUE when current CPU is BSP.

  @retval TRUE  Current CPU is BSP.
  @retval FALSE Current CPU is AP.
**/
BOOLEAN
MutexLibIsBsp (
  VOID
  );
#endif // _PEI_MUTEX_LIB_H_
