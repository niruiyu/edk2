#ifndef _DXE_SEMAPHORE_LIB_H_
#define _DXE_SEMAPHORE_LIB_H_

#include "PeiSemaphoreLib.h"

typedef struct {
  LIST_ENTRY               Link;
  BOOLEAN                  Allocated;
  NAMED_SEMAPHORE_INSTANCE NamedInstance;
} SEMAPHORE_LIST_ENTRY;
#define SEMAPHORE_LIST_ENTRY_FROM_LINK(a) BASE_CR (a, SEMAPHORE_LIST_ENTRY, Link)

typedef struct {
  LIST_ENTRY  List;
  SPIN_LOCK   Lock;
} SEMAPHORE_LIST;

#define SEMAPHORE_LIB_DATABASE_GUID { 0x42b28b9a, 0xf409, 0x494d,{ 0x8f, 0x71, 0x46, 0x54, 0x34, 0xec, 0xb1, 0x7f } }

#endif // _DXE_SEMAPHORE_LIB_H_
