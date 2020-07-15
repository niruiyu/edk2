/** @file
  Unit tests of the MtrrLib instance of the MtrrLib class

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MtrrLibUnitTest.h"

MTRR_LIB_SYSTEM_PARAMETER mDefaultSystemParameter = {42, TRUE, TRUE, CacheUncacheable, 12};

UINT32                           gFixedMtrrsIndex[] = {
  MSR_IA32_MTRR_FIX64K_00000,
  MSR_IA32_MTRR_FIX16K_80000,
  MSR_IA32_MTRR_FIX16K_A0000,
  MSR_IA32_MTRR_FIX4K_C0000,
  MSR_IA32_MTRR_FIX4K_C8000,
  MSR_IA32_MTRR_FIX4K_D0000,
  MSR_IA32_MTRR_FIX4K_D8000,
  MSR_IA32_MTRR_FIX4K_E0000,
  MSR_IA32_MTRR_FIX4K_E8000,
  MSR_IA32_MTRR_FIX4K_F0000,
  MSR_IA32_MTRR_FIX4K_F8000
};
STATIC_ASSERT (
  (ARRAY_SIZE (gFixedMtrrsIndex) == MTRR_NUMBER_OF_FIXED_MTRR),
  "gFixedMtrrIndex does NOT contain all the fixed MTRRs!"
  );


CHAR8 *mCacheDescription[] = { "UC", "WC", "N/A", "N/A", "WT", "WP", "WB" };

/**
  Compare the actual memory ranges against expected memory ranges and return PASS when they match.
**/
UNIT_TEST_STATUS
VerifyMemoryRanges (
  IN MTRR_MEMORY_RANGE  *ExpectedMemoryRanges,
  IN UINTN              ExpectedMemoryRangeCount,
  IN MTRR_MEMORY_RANGE  *ActualRanges,
  IN UINTN              ActualRangeCount
  )
{
  UINTN  Index;
  UT_ASSERT_EQUAL (ExpectedMemoryRangeCount, ActualRangeCount);
  for (Index = 0; Index < ExpectedMemoryRangeCount; Index++) {
    UT_ASSERT_EQUAL (ExpectedMemoryRanges[Index].BaseAddress, ActualRanges[Index].BaseAddress);
    UT_ASSERT_EQUAL (ExpectedMemoryRanges[Index].Length, ActualRanges[Index].Length);
    UT_ASSERT_EQUAL (ExpectedMemoryRanges[Index].Type, ActualRanges[Index].Type);
  }

  return UNIT_TEST_PASSED;
}

/**
  Dump the memory ranges.
**/
VOID
DumpMemoryRanges (
  MTRR_MEMORY_RANGE    *Ranges,
  UINTN                RangeCount
  )
{
  UINTN     Index;
  for (Index = 0; Index < RangeCount; Index++) {
    UT_LOG_INFO ("\t{ 0x%016llx, 0x%016llx, %a },\n", Ranges[Index].BaseAddress, Ranges[Index].Length, mCacheDescription[Ranges[Index].Type]);
  }
}

/**
  Generate random count of MTRRs for each cache type.
**/
VOID
GenerateRandomMemoryTypeCombination (
  IN  UINT32 TotalCount,
  OUT UINT32 *UcCount,
  OUT UINT32 *WtCount,
  OUT UINT32 *WbCount,
  OUT UINT32 *WpCount,
  OUT UINT32 *WcCount
  )
{
  UINTN  Index;
  UINT32 TotalMtrrCount;
  UINT32 *CountPerType[5];
  
  CountPerType[0] = UcCount;
  CountPerType[1] = WtCount;
  CountPerType[2] = WbCount;
  CountPerType[3] = WpCount;
  CountPerType[4] = WcCount;

  //
  // Initialize the count of each cache type to 0.
  //
  for (Index = 0; Index < ARRAY_SIZE (CountPerType); Index++) {
    *(CountPerType[Index]) = 0;
  }

  //
  // Pick a random count of MTRRs
  //
  TotalMtrrCount = (rand() % TotalCount) + 1;
  for (Index = 0; Index < TotalMtrrCount; Index++) {
    //
    // For each of them, pick a random cache type.
    //
    (*(CountPerType[rand() % ARRAY_SIZE (CountPerType)]))++;
  }
}

/**
  Unit test of MtrrLib service MtrrSetMemoryAttribute()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrSetMemoryAttributesInMtrrSettings (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MTRR_LIB_SYSTEM_PARAMETER *SystemParameter;
  RETURN_STATUS             Status;
  UINT32                    UcCount;
  UINT32                    WtCount;
  UINT32                    WbCount;
  UINT32                    WpCount;
  UINT32                    WcCount;

  UINT32                    MtrrIndex;
  UINT8                     *Scratch;
  UINTN                     ScratchSize;
  MTRR_SETTINGS             LocalMtrrs;

  MTRR_MEMORY_RANGE         RawMtrrRange[MTRR_NUMBER_OF_VARIABLE_MTRR];
  MTRR_MEMORY_RANGE         ExpectedMemoryRanges[MTRR_NUMBER_OF_FIXED_MTRR * sizeof (UINT64) + 2 * MTRR_NUMBER_OF_VARIABLE_MTRR + 1];
  UINT32                    ExpectedVariableMtrrUsage;
  UINTN                     ExpectedMemoryRangesCount;

  MTRR_MEMORY_RANGE         ActualMemoryRanges[MTRR_NUMBER_OF_FIXED_MTRR   * sizeof (UINT64) + 2 * MTRR_NUMBER_OF_VARIABLE_MTRR + 1];
  UINT32                    ActualVariableMtrrUsage;
  UINTN                     ActualMemoryRangesCount;

  MTRR_SETTINGS             *Mtrrs[2];

  SystemParameter = (MTRR_LIB_SYSTEM_PARAMETER *) Context;
  GenerateRandomMemoryTypeCombination (
    SystemParameter->VariableMtrrCount - PatchPcdGet32 (PcdCpuNumberOfReservedVariableMtrrs),
    &UcCount, &WtCount, &WbCount, &WpCount, &WcCount
    );
  GenerateValidAndConfigurableMtrrPairs (
    SystemParameter->PhysicalAddressBits, RawMtrrRange,
    UcCount, WtCount, WbCount, WpCount, WcCount
    );

  ExpectedVariableMtrrUsage = UcCount + WtCount + WbCount + WpCount + WcCount;
  ExpectedMemoryRangesCount = ARRAY_SIZE (ExpectedMemoryRanges);
  GetEffectiveMemoryRanges (
    SystemParameter->DefaultCacheType,
    SystemParameter->PhysicalAddressBits,
    RawMtrrRange, ExpectedVariableMtrrUsage,
    ExpectedMemoryRanges, &ExpectedMemoryRangesCount
    );

  UT_LOG_INFO (
    "Total MTRR [%d]: UC=%d, WT=%d, WB=%d, WP=%d, WC=%d\n",
    ExpectedVariableMtrrUsage, UcCount, WtCount, WbCount, WpCount, WcCount
    );
  UT_LOG_INFO ("--- Expected Memory Ranges [%d] ---\n", ExpectedMemoryRangesCount);
  DumpMemoryRanges (ExpectedMemoryRanges, ExpectedMemoryRangesCount);

  //
  // Default cache type is always an INPUT
  //
  ZeroMem (&LocalMtrrs, sizeof (LocalMtrrs));
  LocalMtrrs.MtrrDefType = MtrrGetDefaultMemoryType ();
  ScratchSize            = SCRATCH_BUFFER_SIZE;
  Mtrrs[0]               = &LocalMtrrs;
  Mtrrs[1]               = NULL;

  for (MtrrIndex = 0; MtrrIndex < ARRAY_SIZE (Mtrrs); MtrrIndex++) {
    Scratch = calloc (ScratchSize, sizeof (UINT8));
    Status = MtrrSetMemoryAttributesInMtrrSettings (Mtrrs[MtrrIndex], Scratch, &ScratchSize, ExpectedMemoryRanges, ExpectedMemoryRangesCount);
    if (Status == RETURN_BUFFER_TOO_SMALL) {
      Scratch = realloc (Scratch, ScratchSize);
      Status = MtrrSetMemoryAttributesInMtrrSettings (Mtrrs[MtrrIndex], Scratch, &ScratchSize, ExpectedMemoryRanges, ExpectedMemoryRangesCount);
    }
    UT_ASSERT_STATUS_EQUAL (Status, RETURN_SUCCESS);

    if (Mtrrs[MtrrIndex] == NULL) {
      ZeroMem (&LocalMtrrs, sizeof (LocalMtrrs));
      MtrrGetAllMtrrs (&LocalMtrrs);
    }
    ActualMemoryRangesCount = ARRAY_SIZE (ActualMemoryRanges);
    CollectTestResult (&LocalMtrrs, ActualMemoryRanges, &ActualMemoryRangesCount, &ActualVariableMtrrUsage);
    
    UT_LOG_INFO ("--- Actual Memory Ranges [%d] ---\n", ActualMemoryRangesCount);
    DumpMemoryRanges (ActualMemoryRanges, ActualMemoryRangesCount);
    VerifyMemoryRanges (ExpectedMemoryRanges, ExpectedMemoryRangesCount, ActualMemoryRanges, ActualMemoryRangesCount);
    UT_ASSERT_TRUE (ExpectedVariableMtrrUsage >= ActualVariableMtrrUsage);

    ZeroMem (&LocalMtrrs, sizeof (LocalMtrrs));
  }

  free (Scratch);

  return UNIT_TEST_PASSED;
}

/**
  Generate a random 64bit number.
**/
UINT64
GenerateRandomUint64 (
  VOID
  )
{
  //
  // rand() only returns RAND_MAX 0x7fff according to stdlib.h. This is to generate a 64-bit random number from it.
  //
  return ((UINT64)(rand() & 0x0fff) << 52 |
          (UINT64)(rand() & 0x0fff) << 40 |
          (UINT64)(rand() & 0x0fff) << 28 |
          (UINT64)(rand() & 0x0fff) << 16 |
          (UINT64)(rand() & 0x0fff) << 4  |
          (UINT64)(rand() & 0x000f));
}

/**
  Test routine to check whether invalid base/size can be rejected.
**/
UNIT_TEST_STATUS
EFIAPI
UnitTestInvalidMemoryLayouts (
  IN UNIT_TEST_CONTEXT          Context
  )
{
  MTRR_LIB_SYSTEM_PARAMETER     *SystemParameter;
  MTRR_MEMORY_RANGE             Ranges[MTRR_NUMBER_OF_VARIABLE_MTRR * 2 + 1];
  UINTN                         RangeCount;
  UINT64                        MaxAddress;
  UINT32                        Index;
  UINT64                        BaseAddress;
  UINT64                        Length;
  RETURN_STATUS                 Status;
  UINTN                         ScratchSize;

  SystemParameter = (MTRR_LIB_SYSTEM_PARAMETER *) Context;

  RangeCount = rand () % ARRAY_SIZE (Ranges);
  MaxAddress = 1ull << SystemParameter->PhysicalAddressBits;

  for (Index = 0; Index < RangeCount; Index++) {
    do {
      BaseAddress = GenerateRandomUint64 () % MaxAddress;
      Length      = GenerateRandomUint64 () % (MaxAddress - BaseAddress) + 1;
    } while (((BaseAddress & 0xFFF) == 0) || ((Length & 0xFFF) == 0));

    Ranges[Index].BaseAddress = BaseAddress;
    Ranges[Index].Length      = Length;
    Ranges[Index].Type        = GenerateRandomCacheType ();

    Status = MtrrSetMemoryAttribute (
      Ranges[Index].BaseAddress, Ranges[Index].Length, Ranges[Index].Type
      );
    UT_ASSERT_TRUE (RETURN_ERROR (Status));
  }

  ScratchSize = 0;
  Status = MtrrSetMemoryAttributesInMtrrSettings (NULL, NULL, &ScratchSize, Ranges, RangeCount);
  UT_ASSERT_TRUE (RETURN_ERROR (Status));

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service IsMtrrSupported()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestIsMtrrSupported (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MTRR_LIB_SYSTEM_PARAMETER  SystemParameter;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));
  //
  // MTRR capability off in CPUID leaf.
  //
  SystemParameter.MtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  UT_ASSERT_FALSE (IsMtrrSupported ());

  //
  // MTRR capability on in CPUID leaf, but no variable or fixed MTRRs.
  //
  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = 0;
  SystemParameter.FixedMtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  UT_ASSERT_FALSE (IsMtrrSupported ());

  //
  // MTRR capability on in CPUID leaf, but no variable MTRRs.
  //
  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = 0;
  SystemParameter.FixedMtrrSupported = TRUE;
  InitializeMtrrRegs (&SystemParameter);
  UT_ASSERT_FALSE (IsMtrrSupported ());

  //
  // MTRR capability on in CPUID leaf, but no fixed MTRRs.
  //
  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = 7;
  SystemParameter.FixedMtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  UT_ASSERT_FALSE (IsMtrrSupported ());

  //
  // MTRR capability on in CPUID leaf with both variable and fixed MTRRs.
  //
  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = 7;
  SystemParameter.FixedMtrrSupported = TRUE;
  InitializeMtrrRegs (&SystemParameter);
  UT_ASSERT_TRUE (IsMtrrSupported ());

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service GetVariableMtrrCount()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestGetVariableMtrrCount (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32                    Result;
  MTRR_LIB_SYSTEM_PARAMETER SystemParameter;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));
  //
  // If MTRR capability off in CPUID leaf, then the count is always 0.
  //
  SystemParameter.MtrrSupported = FALSE;
  for (SystemParameter.VariableMtrrCount = 1; SystemParameter.VariableMtrrCount <= MTRR_NUMBER_OF_VARIABLE_MTRR; SystemParameter.VariableMtrrCount++) {
    InitializeMtrrRegs (&SystemParameter);
    Result = GetVariableMtrrCount ();
    UT_ASSERT_EQUAL (Result, 0);
  }

  //
  // Try all supported variable MTRR counts.
  // If variable MTRR count is > MTRR_NUMBER_OF_VARIABLE_MTRR, then an ASSERT()
  // is generated.
  //
  SystemParameter.MtrrSupported = TRUE;
  for (SystemParameter.VariableMtrrCount = 1; SystemParameter.VariableMtrrCount <= MTRR_NUMBER_OF_VARIABLE_MTRR; SystemParameter.VariableMtrrCount++) {
    InitializeMtrrRegs (&SystemParameter);
    Result = GetVariableMtrrCount ();
    UT_ASSERT_EQUAL (Result, SystemParameter.VariableMtrrCount);
  }

  //
  // Expect ASSERT() if variable MTRR count is > MTRR_NUMBER_OF_VARIABLE_MTRR
  //
  SystemParameter.VariableMtrrCount = MTRR_NUMBER_OF_VARIABLE_MTRR + 1;
  InitializeMtrrRegs (&SystemParameter);
  UT_EXPECT_ASSERT_FAILURE (GetVariableMtrrCount (), NULL);

  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = MAX_UINT8;
  InitializeMtrrRegs (&SystemParameter);
  UT_EXPECT_ASSERT_FAILURE (GetVariableMtrrCount (), NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service GetFirmwareVariableMtrrCount()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestGetFirmwareVariableMtrrCount (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32                    Result;
  UINT32                    ReservedMtrrs;
  MTRR_LIB_SYSTEM_PARAMETER SystemParameter;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));

  InitializeMtrrRegs (&SystemParameter);
  //
  // Positive test cases for VCNT = 10 and Reserved PCD in range 0..10
  //
  for (ReservedMtrrs = 0; ReservedMtrrs <= SystemParameter.VariableMtrrCount; ReservedMtrrs++) {
    PatchPcdSet32 (PcdCpuNumberOfReservedVariableMtrrs, ReservedMtrrs);
    Result = GetFirmwareVariableMtrrCount ();
    UT_ASSERT_EQUAL (Result, SystemParameter.VariableMtrrCount - ReservedMtrrs);
  }

  //
  // Negative test cases when Reserved PCD is larger than VCNT
  //
  for (ReservedMtrrs = SystemParameter.VariableMtrrCount + 1; ReservedMtrrs <= 255; ReservedMtrrs++) {
    PatchPcdSet32 (PcdCpuNumberOfReservedVariableMtrrs, ReservedMtrrs);
    Result = GetFirmwareVariableMtrrCount ();
    UT_ASSERT_EQUAL (Result, 0);
  }

  //
  // Negative test cases when Reserved PCD is larger than VCNT
  //
  PatchPcdSet32 (PcdCpuNumberOfReservedVariableMtrrs, MAX_UINT32);
  Result = GetFirmwareVariableMtrrCount ();
  UT_ASSERT_EQUAL (Result, 0);

  //
  // Negative test case when MTRRs are not supported
  //
  SystemParameter.MtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  PatchPcdSet32 (PcdCpuNumberOfReservedVariableMtrrs, 2);
  Result = GetFirmwareVariableMtrrCount ();
  UT_ASSERT_EQUAL (Result, 0);

  //
  // Negative test case when Fixed MTRRs are not supported
  //
  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.FixedMtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  PatchPcdSet32 (PcdCpuNumberOfReservedVariableMtrrs, 2);
  Result = GetFirmwareVariableMtrrCount ();
  UT_ASSERT_EQUAL (Result, 0);

  //
  // Expect ASSERT() if variable MTRR count is > MTRR_NUMBER_OF_VARIABLE_MTRR
  //
  SystemParameter.FixedMtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = MTRR_NUMBER_OF_VARIABLE_MTRR + 1;
  InitializeMtrrRegs (&SystemParameter);
  UT_EXPECT_ASSERT_FAILURE (GetFirmwareVariableMtrrCount (), NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrGetMemoryAttribute()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrGetMemoryAttribute (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrGetFixedMtrr()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrGetFixedMtrr (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MTRR_FIXED_SETTINGS       *Result;
  MTRR_FIXED_SETTINGS       ExpectedFixedSettings;
  MTRR_FIXED_SETTINGS       FixedSettings;
  UINTN                     Index;
  UINTN                     MsrIndex;
  UINTN                     ByteIndex;
  UINT64                    MsrValue;
  MTRR_LIB_SYSTEM_PARAMETER SystemParameter;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));
  InitializeMtrrRegs (&SystemParameter);
  //
  // Set random cache type to different ranges under 1MB and make sure
  // the fixed MTRR settings are expected.
  // Try 100 times.
  //
  for (Index = 0; Index < 100; Index++) {
    for (MsrIndex = 0; MsrIndex < ARRAY_SIZE (gFixedMtrrsIndex); MsrIndex++) {
      MsrValue = 0;
      for (ByteIndex = 0; ByteIndex < sizeof (UINT64); ByteIndex++) {
        MsrValue = MsrValue | LShiftU64 (GenerateRandomCacheType (), ByteIndex * 8);
      }
      ExpectedFixedSettings.Mtrr[MsrIndex] = MsrValue;
      AsmWriteMsr64 (gFixedMtrrsIndex[MsrIndex], MsrValue);
    }

    Result = MtrrGetFixedMtrr (&FixedSettings);
    UT_ASSERT_EQUAL (Result, &FixedSettings);
    UT_ASSERT_MEM_EQUAL (&FixedSettings, &ExpectedFixedSettings, sizeof (FixedSettings));
  }

  //
  // Negative test case when MTRRs are not supported
  //
  SystemParameter.MtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);

  ZeroMem (&FixedSettings, sizeof (FixedSettings));
  ZeroMem (&ExpectedFixedSettings, sizeof (ExpectedFixedSettings));
  Result = MtrrGetFixedMtrr (&FixedSettings);
  UT_ASSERT_EQUAL (Result, &FixedSettings);
  UT_ASSERT_MEM_EQUAL (&ExpectedFixedSettings, &FixedSettings, sizeof (ExpectedFixedSettings));

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrGetAllMtrrs()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrGetAllMtrrs (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MTRR_SETTINGS  *Result;
  MTRR_SETTINGS  Mtrrs;
  MTRR_SETTINGS  ExpectedMtrrs;
  MTRR_VARIABLE_SETTING VariableMtrr[MTRR_NUMBER_OF_VARIABLE_MTRR];
  UINT32         Index;

  MTRR_LIB_SYSTEM_PARAMETER SystemParameter;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));
  InitializeMtrrRegs (&SystemParameter);

  for (Index = 0; Index < SystemParameter.VariableMtrrCount; Index++) {
    GenerateRandomMtrrPair (SystemParameter.PhysicalAddressBits, GenerateRandomCacheType (), &VariableMtrr[Index], NULL);
    AsmWriteMsr64 (MSR_IA32_MTRR_PHYSBASE0 + (Index << 1), VariableMtrr[Index].Base);
    AsmWriteMsr64 (MSR_IA32_MTRR_PHYSMASK0 + (Index << 1), VariableMtrr[Index].Mask);
  }
  Result = MtrrGetAllMtrrs (&Mtrrs);
  UT_ASSERT_EQUAL (Result, &Mtrrs);
  UT_ASSERT_MEM_EQUAL (Mtrrs.Variables.Mtrr, VariableMtrr, sizeof (MTRR_VARIABLE_SETTING) * SystemParameter.VariableMtrrCount);

  //
  // Negative test case when MTRRs are not supported
  //
  ZeroMem (&ExpectedMtrrs, sizeof (ExpectedMtrrs));
  ZeroMem (&Mtrrs, sizeof (Mtrrs));

  SystemParameter.MtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  Result = MtrrGetAllMtrrs (&Mtrrs);
  UT_ASSERT_EQUAL (Result, &Mtrrs);
  UT_ASSERT_MEM_EQUAL (&ExpectedMtrrs, &Mtrrs, sizeof (ExpectedMtrrs));
  
  //
  // Expect ASSERT() if variable MTRR count is > MTRR_NUMBER_OF_VARIABLE_MTRR
  //
  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = MTRR_NUMBER_OF_VARIABLE_MTRR + 1;
  InitializeMtrrRegs (&SystemParameter);  
  UT_EXPECT_ASSERT_FAILURE (MtrrGetAllMtrrs (&Mtrrs), NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrSetAllMtrrs()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrSetAllMtrrs (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MTRR_SETTINGS                   *Result;
  MTRR_SETTINGS                   Mtrrs;
  UINT32                          Index;
  MSR_IA32_MTRR_DEF_TYPE_REGISTER Default;

  MTRR_LIB_SYSTEM_PARAMETER SystemParameter;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));
  InitializeMtrrRegs (&SystemParameter);

  Default.Uint64 = 0;
  Default.Bits.E = 1;
  Default.Bits.FE = 1;
  Default.Bits.Type = GenerateRandomCacheType ();
  
  ZeroMem (&Mtrrs, sizeof (Mtrrs));
  Mtrrs.MtrrDefType = Default.Uint64;
  for (Index = 0; Index < SystemParameter.VariableMtrrCount; Index++) {
    GenerateRandomMtrrPair (SystemParameter.PhysicalAddressBits, GenerateRandomCacheType (), &Mtrrs.Variables.Mtrr[Index], NULL);
  }
  Result = MtrrSetAllMtrrs (&Mtrrs);
  UT_ASSERT_EQUAL (Result, &Mtrrs);

  UT_ASSERT_EQUAL (AsmReadMsr64 (MSR_IA32_MTRR_DEF_TYPE), Mtrrs.MtrrDefType);
  for (Index = 0; Index < SystemParameter.VariableMtrrCount; Index++) {
    UT_ASSERT_EQUAL (AsmReadMsr64 (MSR_IA32_MTRR_PHYSBASE0 + (Index << 1)), Mtrrs.Variables.Mtrr[Index].Base);
    UT_ASSERT_EQUAL (AsmReadMsr64 (MSR_IA32_MTRR_PHYSMASK0 + (Index << 1)), Mtrrs.Variables.Mtrr[Index].Mask);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrGetMemoryAttributeInVariableMtrr()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrGetMemoryAttributeInVariableMtrr (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MTRR_LIB_SYSTEM_PARAMETER       SystemParameter;
  UINT32                          Result;
  MTRR_VARIABLE_SETTING           VariableSetting[MTRR_NUMBER_OF_VARIABLE_MTRR];
  VARIABLE_MTRR                   VariableMtrr[MTRR_NUMBER_OF_VARIABLE_MTRR];
  UINT64                          ValidMtrrBitsMask;
  UINT64                          ValidMtrrAddressMask;
  UINT32                          Index;
  MSR_IA32_MTRR_PHYSBASE_REGISTER Base;
  MSR_IA32_MTRR_PHYSMASK_REGISTER Mask;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));

  InitializeMtrrRegs (&SystemParameter);

  ValidMtrrBitsMask    = (1ull << SystemParameter.PhysicalAddressBits) - 1;
  ValidMtrrAddressMask = ValidMtrrBitsMask & 0xfffffffffffff000ULL;

  for (Index = 0; Index < SystemParameter.VariableMtrrCount; Index++) {
    GenerateRandomMtrrPair (SystemParameter.PhysicalAddressBits, GenerateRandomCacheType (), &VariableSetting[Index], NULL);
    AsmWriteMsr64 (MSR_IA32_MTRR_PHYSBASE0 + (Index << 1), VariableSetting[Index].Base);
    AsmWriteMsr64 (MSR_IA32_MTRR_PHYSMASK0 + (Index << 1), VariableSetting[Index].Mask);
  }
  Result = MtrrGetMemoryAttributeInVariableMtrr (ValidMtrrBitsMask, ValidMtrrAddressMask, VariableMtrr);
  UT_ASSERT_EQUAL (Result, SystemParameter.VariableMtrrCount);

  for (Index = 0; Index < SystemParameter.VariableMtrrCount; Index++) {
    Base.Uint64    = VariableMtrr[Index].BaseAddress;
    Base.Bits.Type = (UINT32) VariableMtrr[Index].Type;
    UT_ASSERT_EQUAL (Base.Uint64, VariableSetting[Index].Base);

    Mask.Uint64    = ~(VariableMtrr[Index].Length - 1) & ValidMtrrBitsMask;
    Mask.Bits.V    = 1;
    UT_ASSERT_EQUAL (Mask.Uint64, VariableSetting[Index].Mask);
  }

  //
  // Negative test case when MTRRs are not supported
  //
  SystemParameter.MtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  Result = MtrrGetMemoryAttributeInVariableMtrr (ValidMtrrBitsMask, ValidMtrrAddressMask, VariableMtrr);
  UT_ASSERT_EQUAL (Result, 0);
  
  //
  // Expect ASSERT() if variable MTRR count is > MTRR_NUMBER_OF_VARIABLE_MTRR
  //
  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = MTRR_NUMBER_OF_VARIABLE_MTRR + 1;
  InitializeMtrrRegs (&SystemParameter);
  UT_EXPECT_ASSERT_FAILURE (MtrrGetMemoryAttributeInVariableMtrr (ValidMtrrBitsMask, ValidMtrrAddressMask, VariableMtrr), NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrDebugPrintAllMtrrs()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrDebugPrintAllMtrrs (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrGetDefaultMemoryType()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrGetDefaultMemoryType (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN                     Index;
  MTRR_MEMORY_CACHE_TYPE    Result;
  MTRR_LIB_SYSTEM_PARAMETER SystemParameter;
  MTRR_MEMORY_CACHE_TYPE    CacheType[5];

  CacheType[0] = CacheUncacheable;
  CacheType[1] = CacheWriteCombining;
  CacheType[2] = CacheWriteThrough;
  CacheType[3] = CacheWriteProtected;
  CacheType[4] = CacheWriteBack;

  CopyMem (&SystemParameter, &mDefaultSystemParameter, sizeof (SystemParameter));
  //
  // If MTRRs are supported, then always return the cache type in the MSR
  // MSR_IA32_MTRR_DEF_TYPE
  //
  for (Index = 0; Index < ARRAY_SIZE (CacheType); Index++) {
    SystemParameter.DefaultCacheType = CacheType[Index];
    InitializeMtrrRegs (&SystemParameter);
    Result = MtrrGetDefaultMemoryType ();
    UT_ASSERT_EQUAL (Result, SystemParameter.DefaultCacheType);
  }

  //
  // If MTRRs are not supported, then always return CacheUncacheable
  //
  SystemParameter.MtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  Result = MtrrGetDefaultMemoryType ();
  UT_ASSERT_EQUAL (Result, CacheUncacheable);

  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.FixedMtrrSupported = FALSE;
  InitializeMtrrRegs (&SystemParameter);
  Result = MtrrGetDefaultMemoryType ();
  UT_ASSERT_EQUAL (Result, CacheUncacheable);

  SystemParameter.MtrrSupported = TRUE;
  SystemParameter.FixedMtrrSupported = TRUE;
  SystemParameter.VariableMtrrCount = 0;
  InitializeMtrrRegs (&SystemParameter);
  Result = MtrrGetDefaultMemoryType ();
  UT_ASSERT_EQUAL (Result, CacheUncacheable);

  return UNIT_TEST_PASSED;
}

/**
  Unit test of MtrrLib service MtrrSetMemoryAttributeInMtrrSettings()

  @param[in]  Context    Ignored

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.

**/
UNIT_TEST_STATUS
EFIAPI
UnitTestMtrrSetMemoryAttributeInMtrrSettings (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MTRR_LIB_SYSTEM_PARAMETER *SystemParameter;
  RETURN_STATUS             Status;
  UINT32                    UcCount;
  UINT32                    WtCount;
  UINT32                    WbCount;
  UINT32                    WpCount;
  UINT32                    WcCount;

  UINTN                     MtrrIndex;
  UINTN                     Index;
  MTRR_SETTINGS             LocalMtrrs;

  MTRR_MEMORY_RANGE         RawMtrrRange[MTRR_NUMBER_OF_VARIABLE_MTRR];
  MTRR_MEMORY_RANGE         ExpectedMemoryRanges[MTRR_NUMBER_OF_FIXED_MTRR * sizeof (UINT64) + 2 * MTRR_NUMBER_OF_VARIABLE_MTRR + 1];
  UINT32                    ExpectedVariableMtrrUsage;
  UINTN                     ExpectedMemoryRangesCount;

  MTRR_MEMORY_RANGE         ActualMemoryRanges[MTRR_NUMBER_OF_FIXED_MTRR * sizeof (UINT64) + 2 * MTRR_NUMBER_OF_VARIABLE_MTRR + 1];
  UINT32                    ActualVariableMtrrUsage;
  UINTN                     ActualMemoryRangesCount;

  MTRR_SETTINGS             *Mtrrs[2];

  SystemParameter = (MTRR_LIB_SYSTEM_PARAMETER *) Context;
  GenerateRandomMemoryTypeCombination (
    SystemParameter->VariableMtrrCount - PatchPcdGet32 (PcdCpuNumberOfReservedVariableMtrrs),
    &UcCount, &WtCount, &WbCount, &WpCount, &WcCount
    );
  GenerateValidAndConfigurableMtrrPairs (
    SystemParameter->PhysicalAddressBits, RawMtrrRange,
    UcCount, WtCount, WbCount, WpCount, WcCount
    );

  ExpectedVariableMtrrUsage = UcCount + WtCount + WbCount + WpCount + WcCount;
  ExpectedMemoryRangesCount = ARRAY_SIZE (ExpectedMemoryRanges);
  GetEffectiveMemoryRanges (
    SystemParameter->DefaultCacheType,
    SystemParameter->PhysicalAddressBits,
    RawMtrrRange, ExpectedVariableMtrrUsage,
    ExpectedMemoryRanges, &ExpectedMemoryRangesCount
    );

  UT_LOG_INFO ("--- Expected Memory Ranges [%d] ---\n", ExpectedMemoryRangesCount);
  DumpMemoryRanges (ExpectedMemoryRanges, ExpectedMemoryRangesCount);
  //
  // Default cache type is always an INPUT
  //
  ZeroMem (&LocalMtrrs, sizeof (LocalMtrrs));
  LocalMtrrs.MtrrDefType = MtrrGetDefaultMemoryType ();
  Mtrrs[0]               = &LocalMtrrs;
  Mtrrs[1]               = NULL;
  
  for (MtrrIndex = 0; MtrrIndex < ARRAY_SIZE (Mtrrs); MtrrIndex++) {
    for (Index = 0; Index < ExpectedMemoryRangesCount; Index++) {
      Status = MtrrSetMemoryAttributeInMtrrSettings (
                 Mtrrs[MtrrIndex],
                 ExpectedMemoryRanges[Index].BaseAddress,
                 ExpectedMemoryRanges[Index].Length,
                 ExpectedMemoryRanges[Index].Type
                 );
      UT_ASSERT_TRUE (Status == RETURN_SUCCESS || Status == RETURN_OUT_OF_RESOURCES || Status == RETURN_BUFFER_TOO_SMALL);
      if (Status == RETURN_OUT_OF_RESOURCES || Status == RETURN_BUFFER_TOO_SMALL) {
        return UNIT_TEST_SKIPPED;
      }
    }

    if (Mtrrs[MtrrIndex] == NULL) {
      ZeroMem (&LocalMtrrs, sizeof (LocalMtrrs));
      MtrrGetAllMtrrs (&LocalMtrrs);
    }
    ActualMemoryRangesCount = ARRAY_SIZE (ActualMemoryRanges);
    CollectTestResult (&LocalMtrrs, ActualMemoryRanges, &ActualMemoryRangesCount, &ActualVariableMtrrUsage);    
    UT_LOG_INFO ("--- Actual Memory Ranges [%d] ---\n", ActualMemoryRangesCount);
    DumpMemoryRanges (ActualMemoryRanges, ActualMemoryRangesCount);
    VerifyMemoryRanges (ExpectedMemoryRanges, ExpectedMemoryRangesCount, ActualMemoryRanges, ActualMemoryRangesCount);
    UT_ASSERT_TRUE (ExpectedVariableMtrrUsage >= ActualVariableMtrrUsage);

    ZeroMem (&LocalMtrrs, sizeof (LocalMtrrs));
  }

  return UNIT_TEST_PASSED;
}

UINT32        mOriginalPcdValue;

/**
  Prep routine for UnitTestGetFirmwareVariableMtrrCount()
**/
UNIT_TEST_STATUS
EFIAPI
SavePcdValue (
  UNIT_TEST_CONTEXT  Context
  )
{
  mOriginalPcdValue = PatchPcdGet32 (PcdCpuNumberOfReservedVariableMtrrs);
  return UNIT_TEST_PASSED;
}

/**
  Clean up routine for UnitTestGetFirmwareVariableMtrrCount()
**/
UNIT_TEST_STATUS
EFIAPI
RestorePcdValue (
  UNIT_TEST_CONTEXT  Context
  )
{
  PatchPcdSet32 (PcdCpuNumberOfReservedVariableMtrrs, mOriginalPcdValue);
  return UNIT_TEST_PASSED;
}
/**
  Initialize the unit test framework, suite, and unit tests for the
  ResetSystemLib and run the ResetSystemLib unit test.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
STATIC
EFI_STATUS
EFIAPI
UnitTestingEntry (
  UINTN                       Iteration
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      MtrrApiTests;

  Framework = NULL;

  DEBUG(( DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION ));

  //
  // Setup the test framework for running the tests.
  //
  Status = InitUnitTestFramework (&Framework, UNIT_TEST_APP_NAME, gEfiCallerBaseName, UNIT_TEST_APP_VERSION);
  if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
      goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //

  //
  // Populate the MtrrLib API Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&MtrrApiTests, Framework, "MtrrLib API Tests", "MtrrLib.MtrrLib", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for MtrrLib API Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }
  AddTestCase (MtrrApiTests, "Test IsMtrrSupported",                       "MtrrSupported",                         UnitTestIsMtrrSupported,                           NULL, NULL, NULL);
  AddTestCase (MtrrApiTests, "Test GetVariableMtrrCount",                  "GetVariableMtrrCount",                  UnitTestGetVariableMtrrCount,                      NULL, NULL, NULL);
  AddTestCase (MtrrApiTests, "Test GetFirmwareVariableMtrrCount",          "GetFirmwareVariableMtrrCount",          UnitTestGetFirmwareVariableMtrrCount,              SavePcdValue, RestorePcdValue, NULL);
  AddTestCase (MtrrApiTests, "Test MtrrGetMemoryAttribute",                "MtrrGetMemoryAttribute",                UnitTestMtrrGetMemoryAttribute,                    NULL, NULL, NULL);
  AddTestCase (MtrrApiTests, "Test MtrrGetFixedMtrr",                      "MtrrGetFixedMtrr",                      UnitTestMtrrGetFixedMtrr,                          NULL, NULL, NULL);
  AddTestCase (MtrrApiTests, "Test MtrrGetAllMtrrs",                       "MtrrGetAllMtrrs",                       UnitTestMtrrGetAllMtrrs,                           NULL, NULL, NULL);
  AddTestCase (MtrrApiTests, "Test MtrrSetAllMtrrs",                       "MtrrSetAllMtrrs",                       UnitTestMtrrSetAllMtrrs,                           NULL, NULL, NULL);
  AddTestCase (MtrrApiTests, "Test MtrrGetMemoryAttributeInVariableMtrr",  "MtrrGetMemoryAttributeInVariableMtrr",  UnitTestMtrrGetMemoryAttributeInVariableMtrr,      NULL, NULL, NULL);
  AddTestCase (MtrrApiTests, "Test MtrrDebugPrintAllMtrrs",                "MtrrDebugPrintAllMtrrs",                UnitTestMtrrDebugPrintAllMtrrs,                    NULL, NULL, NULL);

  AddTestCase (MtrrApiTests, "Test MtrrGetDefaultMemoryType",              "MtrrGetDefaultMemoryType",              UnitTestMtrrGetDefaultMemoryType,                  NULL, NULL, NULL);

  AddTestCase (MtrrApiTests, "Test InvalidMemoryLayouts",                  "InvalidMemoryLayouts",                  UnitTestInvalidMemoryLayouts,                      InitializeMtrrRegs, NULL, &mDefaultSystemParameter);

  for (UINTN Index = 0; Index < Iteration; Index++) {
    AddTestCase (MtrrApiTests, "Test MtrrSetMemoryAttributeInMtrrSettings",  "MtrrSetMemoryAttributeInMtrrSettings",  UnitTestMtrrSetMemoryAttributeInMtrrSettings,      InitializeMtrrRegs, NULL, &mDefaultSystemParameter);
    AddTestCase (MtrrApiTests, "Test MtrrSetMemoryAttributesInMtrrSettings", "MtrrSetMemoryAttributesInMtrrSettings", UnitTestMtrrSetMemoryAttributesInMtrrSettings,     InitializeMtrrRegs, NULL, &mDefaultSystemParameter);
  }
  //
  // Execute the tests.
  //
  srand ((unsigned int) time (NULL));
  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

/**
  Standard POSIX C entry point for host based unit test execution.
**/
int
main (
  int argc,
  char *argv[]
  )
{
  UINTN    Iteration;

  //
  // First parameter specifies the test iterations.
  // Default is 10.
  //
  Iteration = 10;
  if (argc == 2) {
    Iteration = atoi (argv[1]);
  }
  return UnitTestingEntry (Iteration);
}
