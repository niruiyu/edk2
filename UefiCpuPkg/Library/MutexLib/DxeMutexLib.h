#ifndef _DXE_MUTEX_LIB_H_
#define _DXE_MUTEX_LIB_H_

#include "PeiMutexLib.h"

#define MUTEX_LIB_DATABASE_GUID \
  { \
    0x4aa0f850, 0x929, 0x42f5, { 0xa4, 0x34, 0x6, 0xb7, 0x84, 0xd3, 0xc1, 0x36 } \
  }

typedef struct {
  LIST_ENTRY     Link;
  BOOLEAN        Allocated;
  GUID           Name;
  MUTEX_INSTANCE Instance;
} MUTEX_LIST_ENTRY;

#define MUTEX_LIST_ENTRY_FROM_LINK(a) BASE_CR(a, MUTEX_LIST_ENTRY, Link)

typedef struct {
  LIST_ENTRY     List;
  SPIN_LOCK      Lock;
} MUTEX_LIST;

#endif // _DXE_MUTEX_LIB_H_
