/** @file

  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MTRR_SUPPORT_H_
#define _MTRR_SUPPORT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <time.h>

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UnitTestLib.h>
#include <Library/MtrrLib.h>
#include <HostTest/UnitTestHostBaseLib.h>

#include <Register/ArchitecturalMsr.h>
#include <Register/Cpuid.h>
#include <Register/Msr.h>

#define UNIT_TEST_APP_NAME        "MtrrLib Unit Tests"
#define UNIT_TEST_APP_VERSION     "1.0"

#define SCRATCH_BUFFER_SIZE           SIZE_16KB

typedef struct {
  UINT8                  PhysicalAddressBits;
  BOOLEAN                MtrrSupported;
  BOOLEAN                FixedMtrrSupported;
  MTRR_MEMORY_CACHE_TYPE DefaultCacheType;
  UINT32                 VariableMtrrCount;
} MTRR_LIB_SYSTEM_PARAMETER;

extern UINT32                           gFixedMtrrsIndex[];
/**
  Initialize MTRR registers.
**/
UNIT_TEST_STATUS
EFIAPI
InitializeMtrrRegs (
  IN MTRR_LIB_SYSTEM_PARAMETER  *SystemParameter
  );

/**
  Return a random memory cache type.
**/
MTRR_MEMORY_CACHE_TYPE
GenerateRandomCacheType (
  VOID
  );

/**
  The generated ranges must be above 1M. Because Variable MTRR are only used for >1M.

  Not all overlappings are valid. The valid ones are: (Refer to: Intel SDM 11.11.4.1)
    UC: UC, WT, WB, WC, WP
    WT: UC, WB
    WB: UC, WT
    WC: UC
    WP: UC
**/
VOID
GenerateValidAndConfigurableMtrrPairs (
  IN     UINT32                    PhysicalAddressBits,
  IN OUT MTRR_MEMORY_RANGE         *RawMemoryRanges,
  IN     UINT32                    UcCount,
  IN     UINT32                    WtCount,
  IN     UINT32                    WbCount,
  IN     UINT32                    WpCount,
  IN     UINT32                    WcCount
  );

/**
  Convert the MTRR BASE/MASK array to memory ranges.
**/
VOID
GetEffectiveMemoryRanges (
  IN MTRR_MEMORY_CACHE_TYPE DefaultType,
  IN UINT32                 PhysicalAddressBits,
  IN MTRR_MEMORY_RANGE      *RawMtrrMemoryRanges,
  IN UINT32                 RawMtrrMemoryRangesCount,
  OUT MTRR_MEMORY_RANGE     *EffectiveMtrrMemoryRanges,
  OUT UINTN                 *EffectiveMtrrMemoryRangesCount
  );

/**
  Generate random MTRR BASE/MASK for a specified type.
**/
VOID
GenerateRandomMtrrPair (
  IN  UINT32                 PhysicalAddressBits,
  IN  MTRR_MEMORY_CACHE_TYPE CacheType,
  OUT MTRR_VARIABLE_SETTING  *MtrrPair,       OPTIONAL
  OUT MTRR_MEMORY_RANGE      *MtrrMemoryRange OPTIONAL
  );

/**
  Collect the test result.
**/
VOID
CollectTestResult (
  MTRR_SETTINGS     *Mtrrs,
  MTRR_MEMORY_RANGE *Ranges,
  UINTN             *RangeCount,
  UINT32            *MtrrCount
  );

#endif
