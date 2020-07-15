/** @file
  Unit tests of the MtrrLib instance of the MtrrLib class

  Copyright (c) 2018 - 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MtrrLibUnitTest.h"

MTRR_MEMORY_CACHE_TYPE mMemoryCacheTypes[] = { CacheUncacheable, CacheWriteCombining, CacheWriteThrough, CacheWriteProtected, CacheWriteBack };

VOID
MtrrLibInitializeMtrrMask (
  OUT UINT64 *MtrrValidBitsMask,
  OUT UINT64 *MtrrValidAddressMask
  );
UINT32
MtrrLibGetRawVariableRanges (
  IN  MTRR_VARIABLE_SETTINGS  *VariableSettings,
  IN  UINTN                   VariableMtrrCount,
  IN  UINT64                  MtrrValidBitsMask,
  IN  UINT64                  MtrrValidAddressMask,
  OUT MTRR_MEMORY_RANGE       *VariableMtrr
  );
RETURN_STATUS
MtrrLibApplyVariableMtrrs (
  IN     CONST MTRR_MEMORY_RANGE *VariableMtrr,
  IN     UINT32                  VariableMtrrCount,
  IN OUT MTRR_MEMORY_RANGE       *Ranges,
  IN     UINTN                   RangeCapacity,
  IN OUT UINTN                   *RangeCount
  );
RETURN_STATUS
MtrrLibApplyFixedMtrrs (
  IN     MTRR_FIXED_SETTINGS  *Fixed,
  IN OUT MTRR_MEMORY_RANGE    *Ranges,
  IN     UINTN                RangeCapacity,
  IN OUT UINTN                *RangeCount
  );

UINT64                           gFixedMtrrsValue[MTRR_NUMBER_OF_FIXED_MTRR];
MSR_IA32_MTRR_PHYSBASE_REGISTER  gVariableMtrrsPhysBase[MTRR_NUMBER_OF_VARIABLE_MTRR];
MSR_IA32_MTRR_PHYSMASK_REGISTER  gVariableMtrrsPhysMask[MTRR_NUMBER_OF_VARIABLE_MTRR];
MSR_IA32_MTRR_DEF_TYPE_REGISTER  gDefTypeMsr;
MSR_IA32_MTRRCAP_REGISTER        gMtrrCapMsr;
CPUID_VERSION_INFO_EDX           gCpuidVersionInfoEdx;
CPUID_VIR_PHY_ADDRESS_SIZE_EAX   gCpuidVirPhyAddressSizeEax;

/**
  Retrieves CPUID information.

  Executes the CPUID instruction with EAX set to the value specified by Index.
  This function always returns Index.
  If Eax is not NULL, then the value of EAX after CPUID is returned in Eax.
  If Ebx is not NULL, then the value of EBX after CPUID is returned in Ebx.
  If Ecx is not NULL, then the value of ECX after CPUID is returned in Ecx.
  If Edx is not NULL, then the value of EDX after CPUID is returned in Edx.
  This function is only available on IA-32 and x64.

  @param  Index The 32-bit value to load into EAX prior to invoking the CPUID
                instruction.
  @param  Eax   The pointer to the 32-bit EAX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Ebx   The pointer to the 32-bit EBX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Ecx   The pointer to the 32-bit ECX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Edx   The pointer to the 32-bit EDX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.

  @return Index.

**/
UINT32
EFIAPI
UnitTestMtrrLibAsmCpuid (
  IN      UINT32                    Index,
  OUT     UINT32                    *RegisterEax,  OPTIONAL
  OUT     UINT32                    *RegisterEbx,  OPTIONAL
  OUT     UINT32                    *RegisterEcx,  OPTIONAL
  OUT     UINT32                    *RegisterEdx   OPTIONAL
  )
{
  switch (Index) {
  case CPUID_VERSION_INFO:
    if (RegisterEdx != NULL) {
      *RegisterEdx = gCpuidVersionInfoEdx.Uint32;
    }
    return Index;
    break;
  case CPUID_EXTENDED_FUNCTION:
    if (RegisterEax != NULL) {
      *RegisterEax = CPUID_VIR_PHY_ADDRESS_SIZE;
    }
    return Index;
    break;
  case CPUID_VIR_PHY_ADDRESS_SIZE:
    if (RegisterEax != NULL) {
      *RegisterEax = gCpuidVirPhyAddressSizeEax.Uint32;
    }
    return Index;
    break;
  }

  //
  // Should never fall through to here
  //
  ASSERT(FALSE);
  return Index;
}

/**
  Returns a 64-bit Machine Specific Register(MSR).

  Reads and returns the 64-bit MSR specified by Index. No parameter checking is
  performed on Index, and some Index values may cause CPU exceptions. The
  caller must either guarantee that Index is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and x64.

  @param  MsrIndex The 32-bit MSR index to read.

  @return The value of the MSR identified by MsrIndex.

**/
UINT64
EFIAPI
UnitTestMtrrLibAsmReadMsr64(
  IN UINT32  MsrIndex
  )
{
  UINT32 Index;

  for (Index = 0; Index < ARRAY_SIZE (gFixedMtrrsValue); Index++) {
    if (MsrIndex == gFixedMtrrsIndex[Index]) {
      return gFixedMtrrsValue[Index];
    }
  }

  if ((MsrIndex >= MSR_IA32_MTRR_PHYSBASE0) &&
      (MsrIndex <= MSR_IA32_MTRR_PHYSMASK0 + (MTRR_NUMBER_OF_VARIABLE_MTRR << 1))) {
    if (MsrIndex % 2 == 0) {
      Index = (MsrIndex - MSR_IA32_MTRR_PHYSBASE0) >> 1;
      return gVariableMtrrsPhysBase[Index].Uint64;
    } else {
      Index = (MsrIndex - MSR_IA32_MTRR_PHYSMASK0) >> 1;
      return gVariableMtrrsPhysMask[Index].Uint64;
    }
  }

  if (MsrIndex == MSR_IA32_MTRR_DEF_TYPE) {
    return gDefTypeMsr.Uint64;
  }

  if (MsrIndex == MSR_IA32_MTRRCAP) {
    return gMtrrCapMsr.Uint64;
  }

  //
  // Should never fall through to here
  //
  ASSERT(FALSE);
  return 0;
}

/**
  Writes a 64-bit value to a Machine Specific Register(MSR), and returns the
  value.

  Writes the 64-bit value specified by Value to the MSR specified by Index. The
  64-bit value written to the MSR is returned. No parameter checking is
  performed on Index or Value, and some of these may cause CPU exceptions. The
  caller must either guarantee that Index and Value are valid, or the caller
  must establish proper exception handlers. This function is only available on
  IA-32 and x64.

  @param  MsrIndex The 32-bit MSR index to write.
  @param  Value The 64-bit value to write to the MSR.

  @return Value

**/
UINT64
EFIAPI
UnitTestMtrrLibAsmWriteMsr64(
  IN      UINT32                    MsrIndex,
  IN      UINT64                    Value
  )
{
  UINT32 Index;

  for (Index = 0; Index < ARRAY_SIZE (gFixedMtrrsValue); Index++) {
    if (MsrIndex == gFixedMtrrsIndex[Index]) {
      gFixedMtrrsValue[Index] = Value;
      return Value;
    }
  }

  if ((MsrIndex >= MSR_IA32_MTRR_PHYSBASE0) &&
      (MsrIndex <= MSR_IA32_MTRR_PHYSMASK0 + (MTRR_NUMBER_OF_VARIABLE_MTRR << 1))) {
    if (MsrIndex % 2 == 0) {
      Index = (MsrIndex - MSR_IA32_MTRR_PHYSBASE0) >> 1;
      gVariableMtrrsPhysBase[Index].Uint64 = Value;
      return Value;
    } else {
      Index = (MsrIndex - MSR_IA32_MTRR_PHYSMASK0) >> 1;
      gVariableMtrrsPhysMask[Index].Uint64 = Value;
      return Value;
    }
  }

  if (MsrIndex == MSR_IA32_MTRR_DEF_TYPE) {
    gDefTypeMsr.Uint64 = Value;
    return Value;
  }

  if (MsrIndex == MSR_IA32_MTRRCAP) {
    gMtrrCapMsr.Uint64 = Value;
    return Value;
  }

  //
  // Should never fall through to here
  //
  ASSERT(FALSE);
  return 0;
}

/**
  Initialize MTRR registers.
**/
UNIT_TEST_STATUS
EFIAPI
InitializeMtrrRegs (
  IN MTRR_LIB_SYSTEM_PARAMETER  *SystemParameter
  )
{
  UINT32                    Index;

  SetMem (gFixedMtrrsValue, sizeof (gFixedMtrrsValue), SystemParameter->DefaultCacheType);

  for (Index = 0; Index < ARRAY_SIZE (gVariableMtrrsPhysBase); Index++) {
    gVariableMtrrsPhysBase[Index].Uint64         = 0;
    gVariableMtrrsPhysBase[Index].Bits.Type      = SystemParameter->DefaultCacheType;
    gVariableMtrrsPhysBase[Index].Bits.Reserved1 = 0;

    gVariableMtrrsPhysMask[Index].Uint64         = 0;
    gVariableMtrrsPhysMask[Index].Bits.V         = 0;
    gVariableMtrrsPhysMask[Index].Bits.Reserved1 = 0;
  }

  gDefTypeMsr.Bits.E         = 1;
  gDefTypeMsr.Bits.FE        = 1;
  gDefTypeMsr.Bits.Type      = SystemParameter->DefaultCacheType;
  gDefTypeMsr.Bits.Reserved1 = 0;
  gDefTypeMsr.Bits.Reserved2 = 0;
  gDefTypeMsr.Bits.Reserved3 = 0;

  gMtrrCapMsr.Bits.SMRR      = 0;
  gMtrrCapMsr.Bits.WC        = 0;
  gMtrrCapMsr.Bits.VCNT      = SystemParameter->VariableMtrrCount;
  gMtrrCapMsr.Bits.FIX       = SystemParameter->FixedMtrrSupported;
  gMtrrCapMsr.Bits.Reserved1 = 0;
  gMtrrCapMsr.Bits.Reserved2 = 0;
  gMtrrCapMsr.Bits.Reserved3 = 0;

  gCpuidVersionInfoEdx.Bits.MTRR                      = SystemParameter->MtrrSupported;
  gCpuidVirPhyAddressSizeEax.Bits.PhysicalAddressBits = SystemParameter->PhysicalAddressBits;

  //
  // Hook BaseLib functions used by MtrrLib that require some emulation.
  //
  gUnitTestHostBaseLib.X86->AsmCpuid      = UnitTestMtrrLibAsmCpuid;
  gUnitTestHostBaseLib.X86->AsmReadMsr64  = UnitTestMtrrLibAsmReadMsr64;
  gUnitTestHostBaseLib.X86->AsmWriteMsr64 = UnitTestMtrrLibAsmWriteMsr64;

  return UNIT_TEST_PASSED;
}

/**
  Collect the test result.
**/
VOID
CollectTestResult (
  MTRR_SETTINGS     *Mtrrs,
  MTRR_MEMORY_RANGE *Ranges,
  UINTN             *RangeCount,
  UINT32            *MtrrCount
  )
{
  UINTN             Index;
  UINTN             RangeCapacity;
  UINT64            MtrrValidBitsMask;
  UINT64            MtrrValidAddressMask;
  UINT32            VariableMtrrCount;
  MTRR_MEMORY_RANGE RawVariableRanges[ARRAY_SIZE (Mtrrs->Variables.Mtrr)];

  ASSERT (Mtrrs != NULL);
  VariableMtrrCount = GetVariableMtrrCount ();

  *MtrrCount = 0;
  for (Index = 0; Index < VariableMtrrCount; Index++) {
    if (((MSR_IA32_MTRR_PHYSMASK_REGISTER *) &Mtrrs->Variables.Mtrr[Index].Mask)->Bits.V == 1) {
      *MtrrCount++;
    }
  }

  RangeCapacity = *RangeCount;
  MtrrLibInitializeMtrrMask (&MtrrValidBitsMask, &MtrrValidAddressMask);
  ASSERT (RangeCapacity > 1);
  Ranges[0].BaseAddress = 0;
  Ranges[0].Length = MtrrValidBitsMask + 1;
  Ranges[0].Type = MtrrGetDefaultMemoryType ();
  *RangeCount = 1;

  MtrrLibGetRawVariableRanges (
    &Mtrrs->Variables, VariableMtrrCount,
    MtrrValidBitsMask, MtrrValidAddressMask, RawVariableRanges);
  MtrrLibApplyVariableMtrrs (
    RawVariableRanges, VariableMtrrCount,
    Ranges, RangeCapacity, RangeCount
    );

  MtrrLibApplyFixedMtrrs (&Mtrrs->Fixed, Ranges, RangeCapacity, RangeCount);
}

/**
  Generate random MTRR BASE/MASK for a specified type.
**/
VOID
GenerateRandomMtrrPair (
  IN  UINT32                 PhysicalAddressBits,
  IN  MTRR_MEMORY_CACHE_TYPE CacheType,
  OUT MTRR_VARIABLE_SETTING  *MtrrPair,       OPTIONAL
  OUT MTRR_MEMORY_RANGE      *MtrrMemoryRange OPTIONAL
  )
{
  MSR_IA32_MTRR_PHYSBASE_REGISTER PhysBase;
  MSR_IA32_MTRR_PHYSMASK_REGISTER PhysMask;
  UINTN                           RandomRangeSizeIn4KPageUnitAsPowerOf2;
  UINTN                           RandomRangeSizeAsPowerOf2;
  UINTN                           M;
  UINT64                          BoundarySegmentCount;
  UINT64                          RandomBoundary;
  UINT64                          MaxPhysicalAddress;
  UINT64                          RangeSize;
  UINT64                          RangeBase;
  UINT64                          PhysBasePhyMaskValidBitsMask;

  MaxPhysicalAddress = 1ull << PhysicalAddressBits;
  do {
    RandomRangeSizeIn4KPageUnitAsPowerOf2 = rand () % (PhysicalAddressBits - 12 + 1);
    RandomRangeSizeAsPowerOf2 = RandomRangeSizeIn4KPageUnitAsPowerOf2 + 12;
    RangeSize = 1ull << RandomRangeSizeAsPowerOf2;

    M = rand () % (PhysicalAddressBits - RandomRangeSizeAsPowerOf2 + 1) + RandomRangeSizeAsPowerOf2;

    BoundarySegmentCount = 1ull << (PhysicalAddressBits - M);

    RandomBoundary = (UINT64) rand () % BoundarySegmentCount;
    RangeBase = RandomBoundary << M;
  } while (RangeBase < SIZE_1MB || RangeBase > MaxPhysicalAddress - 1);

  PhysBasePhyMaskValidBitsMask = (MaxPhysicalAddress - 1) & 0xfffffffffffff000ULL;

  PhysBase.Uint64    = 0;
  PhysBase.Bits.Type = CacheType;
  PhysBase.Uint64   |= RangeBase & PhysBasePhyMaskValidBitsMask;
  PhysMask.Uint64    = 0;
  PhysMask.Bits.V    = 1;
  PhysMask.Uint64   |= ((~RangeSize) + 1) & PhysBasePhyMaskValidBitsMask;

  if (MtrrPair != NULL) {
    MtrrPair->Base = PhysBase.Uint64;
    MtrrPair->Mask = PhysMask.Uint64;
  }

  if (MtrrMemoryRange != NULL) {
    MtrrMemoryRange->BaseAddress = RangeBase;
    MtrrMemoryRange->Length      = RangeSize;
    MtrrMemoryRange->Type        = CacheType;
  }
}


/**
  Return TRUE when Range overlaps with any one in Ranges
**/
BOOLEAN
RangesOverlap (
  IN MTRR_MEMORY_RANGE *Range,
  IN MTRR_MEMORY_RANGE *Ranges,
  IN UINTN             Count
  )
{
  while (Count-- != 0) {
    //
    // Two ranges overlap when:
    // 1. range#2.base is in the middle of range#1
    // 2. range#1.base is in the middle of ragne#2
    //
    if ((Range->BaseAddress <= Ranges[Count].BaseAddress && Ranges[Count].BaseAddress < Range->BaseAddress + Range->Length)
     || (Ranges[Count].BaseAddress <= Range->BaseAddress && Range->BaseAddress < Ranges[Count].BaseAddress + Ranges[Count].Length)) {
      return TRUE;
    }
  }
  return FALSE;
}

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
  )
{
  UINT32                          Index;

  //
  // Order of types is: UC, WT, WB, WP, WC
  //

  //1. Generate UC, WT, WB at will.
  for (Index = 0; Index < UcCount; Index++) {
    GenerateRandomMtrrPair (PhysicalAddressBits, CacheUncacheable, NULL, &RawMemoryRanges[Index]);
  }

  for (Index = UcCount; Index < UcCount + WtCount; Index++) {
    GenerateRandomMtrrPair (PhysicalAddressBits, CacheWriteThrough, NULL, &RawMemoryRanges[Index]);
  }

  for (Index = UcCount + WtCount; Index < UcCount + WtCount + WbCount; Index++) {
    GenerateRandomMtrrPair (PhysicalAddressBits, CacheWriteBack, NULL, &RawMemoryRanges[Index]);
  }

  //2. Generate WP MTRR and DO NOT overlap with WT, WB.
  for (Index = UcCount + WtCount + WbCount; Index < UcCount + WtCount + WbCount + WpCount; Index++) {
    GenerateRandomMtrrPair (PhysicalAddressBits, CacheWriteProtected, NULL, &RawMemoryRanges[Index]);
    while (RangesOverlap (&RawMemoryRanges[Index], &RawMemoryRanges[UcCount], WtCount + WbCount)) {
      GenerateRandomMtrrPair (PhysicalAddressBits, CacheWriteProtected, NULL, &RawMemoryRanges[Index]);
    }
  }

  //3. Generate WC MTRR and DO NOT overlap with WT, WB, WP.
  for (Index = UcCount + WtCount + WbCount + WpCount; Index < UcCount + WtCount + WbCount + WpCount + WcCount; Index++) {
    GenerateRandomMtrrPair (PhysicalAddressBits, CacheWriteCombining, NULL, &RawMemoryRanges[Index]);
    while (RangesOverlap (&RawMemoryRanges[Index], &RawMemoryRanges[UcCount], WtCount + WbCount + WpCount)) {
      GenerateRandomMtrrPair (PhysicalAddressBits, CacheWriteCombining, NULL, &RawMemoryRanges[Index]);
    }
  }
}

/**
  Return a random memory cache type.
**/
MTRR_MEMORY_CACHE_TYPE
GenerateRandomCacheType (
  VOID
  )
{
    return mMemoryCacheTypes[rand() % ARRAY_SIZE (mMemoryCacheTypes)];
}


INT32
CompareFuncUint64 (
  const void * a,
  const void * b
  )
{
    INT64 diff = (*(UINT64*)a - *(UINT64*)b);
    if (diff > 0)return 1;
    if (diff == 0)return 0;
    return -1;
}

/**
  Determin the memory cache type for the Range.
**/
VOID
DetermineMemoryCacheType (
  IN     MTRR_MEMORY_CACHE_TYPE DefaultType,
  IN OUT MTRR_MEMORY_RANGE      *Range,
  IN     MTRR_MEMORY_RANGE      *Ranges,
  IN     UINT32                 RangeCount
  )
{
  UINT32 Index;
  Range->Type = CacheInvalid;
  for (Index = 0; Index < RangeCount; Index++) {
    if (RangesOverlap (Range, &Ranges[Index], 1)) {
      if (Ranges[Index].Type < Range->Type) {
        Range->Type = Ranges[Index].Type;
      }
    }
  }

  if (Range->Type == CacheInvalid) {
    Range->Type = DefaultType;
  }
}


/**
  Get the index of the element that does NOT equals to Array[Index].
**/
UINT32
GetNextDifferentElementInSortedArray (
  IN OUT UINT64 *Array,
  IN UINT32 Index,
  IN UINT32 Count
  )
{
    UINT64 CurrentElement;
    CurrentElement = Array[Index];
    while (CurrentElement == Array[Index] && Index < Count) {
        Index++;
    }
    return Index;
}

/**
  Count: On input, the original count, on ouput, the count after removing duplicates.
**/
VOID
RemoveDuplicatesInSortedArray (
  IN OUT UINT64 *Array,
  IN OUT UINT32 *Count
  )
{
  UINT32 Index;
  UINT32 NewCount;

  Index    = 0;
  NewCount = 0;
  while (Index < *Count) {
    Array[NewCount] = Array[Index];
    NewCount++;
    Index = GetNextDifferentElementInSortedArray (Array, Index, *Count);
  }
  *Count = NewCount;
}

BOOLEAN
AddressInRange (
  IN UINT64 Address,
  IN MTRR_MEMORY_RANGE Range
  )
{
    return (Address >= Range.BaseAddress) && (Address <= Range.BaseAddress + Range.Length - 1);
}

UINT64
GetOverlapBitFlag (
  IN MTRR_MEMORY_RANGE *RawMtrrMemoryRanges,
  IN UINT32 RawMtrrMemoryRangeCount,
  IN UINT64 Address
  )
{
    UINT64 OverlapBitFlag = 0;
    UINT32 i;
    for (i = 0; i < RawMtrrMemoryRangeCount; i++)
    {
        if (AddressInRange(Address, RawMtrrMemoryRanges[i]))
        {
            OverlapBitFlag |= (((UINT64)1) << i);
        }
    }

    return OverlapBitFlag;
}

/*
return:
    0: Flag1 == Flag2
    1: Flag1 is a subset of Flag2
    2: Flag2 is a subset of Flag1
    3: No subset relations between Flag1 and Flag2.
*/
UINT32
CheckOverlapBitFlagsRelation (
  IN UINT64 Flag1,
  IN UINT64 Flag2
  )
{
    if (Flag1 == Flag2) return 0;
    if ((Flag1 | Flag2) == Flag2) return 1;
    if ((Flag1 | Flag2) == Flag1) return 2;
    return 3;
}

BOOLEAN
IsLeftSkippedEndpointAlreadyCollected (
  IN UINT64 SkippedEndpoint,
  IN MTRR_MEMORY_RANGE *AllRangePieces,
  IN UINTN AllRangePiecesCountActual
  )
{
    UINT32 i;
    for (i = 0; i < AllRangePiecesCountActual; i++)
    {
        if (AddressInRange(SkippedEndpoint, AllRangePieces[i]))
            return TRUE;
    }
    return FALSE;
}


/**
  Compact adjacent ranges of the same type.
**/
VOID
CompactAndExtendEffectiveMtrrMemoryRanges (
  IN     MTRR_MEMORY_CACHE_TYPE DefaultType,
  IN     UINT32                 PhysicalAddressBits,
  IN OUT MTRR_MEMORY_RANGE      **EffectiveMtrrMemoryRanges,
  IN OUT UINTN                  *EffectiveMtrrMemoryRangesCount
  )
{
  UINTN NewRangesCountAtMost = *EffectiveMtrrMemoryRangesCount + 2;   // At most with 2 more range entries.
  MTRR_MEMORY_RANGE *NewRanges = (MTRR_MEMORY_RANGE *) calloc (NewRangesCountAtMost, sizeof (MTRR_MEMORY_RANGE));
  UINTN NewRangesCountActual = 0;
  MTRR_MEMORY_RANGE *CurrentRangeInNewRanges;

  MTRR_MEMORY_RANGE *OldRanges = *EffectiveMtrrMemoryRanges;
  
  if (OldRanges[0].BaseAddress > 0) {
    NewRanges[NewRangesCountActual].BaseAddress = 0;
    NewRanges[NewRangesCountActual].Length = OldRanges[0].BaseAddress;
    NewRanges[NewRangesCountActual].Type = DefaultType;
    NewRangesCountActual++;
  }

  UINTN OldRangesIndex = 0;
  while (OldRangesIndex < *EffectiveMtrrMemoryRangesCount) {
    MTRR_MEMORY_CACHE_TYPE CurrentRangeTypeInOldRanges = OldRanges[OldRangesIndex].Type;
    CurrentRangeInNewRanges = NULL;
    if (NewRangesCountActual > 0)   // We need to check CurrentNewRange first before generate a new NewRange.
    {
      CurrentRangeInNewRanges = &NewRanges[NewRangesCountActual - 1];
    }
    if (CurrentRangeInNewRanges != NULL && CurrentRangeInNewRanges->Type == CurrentRangeTypeInOldRanges) {
      CurrentRangeInNewRanges->Length += OldRanges[OldRangesIndex].Length;
    } else {
      NewRanges[NewRangesCountActual].BaseAddress = OldRanges[OldRangesIndex].BaseAddress;
      NewRanges[NewRangesCountActual].Length += OldRanges[OldRangesIndex].Length;
      NewRanges[NewRangesCountActual].Type = CurrentRangeTypeInOldRanges;
      while (OldRangesIndex + 1 < *EffectiveMtrrMemoryRangesCount && OldRanges[OldRangesIndex + 1].Type == CurrentRangeTypeInOldRanges)   // TODO add index limit check
      {
        OldRangesIndex++;
        NewRanges[NewRangesCountActual].Length += OldRanges[OldRangesIndex].Length;
      }
      NewRangesCountActual++;
    }

    OldRangesIndex++;
  }

  UINT64 MaxAddress = (1ull << PhysicalAddressBits) - 1;
  MTRR_MEMORY_RANGE OldLastRange = OldRanges[(*EffectiveMtrrMemoryRangesCount) - 1];
  CurrentRangeInNewRanges = &NewRanges[NewRangesCountActual - 1];
  if (OldLastRange.BaseAddress + OldLastRange.Length - 1 < MaxAddress) {
    if (CurrentRangeInNewRanges->Type == DefaultType) {
      CurrentRangeInNewRanges->Length = MaxAddress - CurrentRangeInNewRanges->BaseAddress + 1;
    } else {
      NewRanges[NewRangesCountActual].BaseAddress = OldLastRange.BaseAddress + OldLastRange.Length;
      NewRanges[NewRangesCountActual].Length = MaxAddress - NewRanges[NewRangesCountActual].BaseAddress + 1;
      NewRanges[NewRangesCountActual].Type = DefaultType;
      NewRangesCountActual++;
    }
  }

  free (*EffectiveMtrrMemoryRanges);
  *EffectiveMtrrMemoryRanges = NewRanges;
  *EffectiveMtrrMemoryRangesCount = NewRangesCountActual;
}

/**
  Collect the memory ranges.
**/
VOID
CollectRawMtrrRangesEndpointsAndSortAndRemoveDuplicates (
  IN UINT64            *AllEndPointsInclusive,
  IN UINT32            *AllEndPointsCount,
  IN MTRR_MEMORY_RANGE *RawMtrrMemoryRanges,
  IN UINT32            RawMtrrMemoryRangesCount
  )
{
  UINT32 Index;

  ASSERT ((RawMtrrMemoryRangesCount << 1) == *AllEndPointsCount);

  for (Index = 0; Index < *AllEndPointsCount; Index += 2) {
    UINT32 RawRangeIndex = Index >> 1;
    AllEndPointsInclusive[Index] = RawMtrrMemoryRanges[RawRangeIndex].BaseAddress;
    AllEndPointsInclusive[Index + 1] = RawMtrrMemoryRanges[RawRangeIndex].BaseAddress + RawMtrrMemoryRanges[RawRangeIndex].Length - 1;
  }

  qsort (AllEndPointsInclusive, *AllEndPointsCount, sizeof (UINT64), CompareFuncUint64);
  RemoveDuplicatesInSortedArray (AllEndPointsInclusive, AllEndPointsCount);
}

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
  )
{
  UINTN                 Index;
  UINT32                AllEndPointsCount;
  UINT64                *AllEndPointsInclusive;
  UINT32                AllRangePiecesCountMax;
  MTRR_MEMORY_RANGE     *AllRangePieces;
  UINTN                 AllRangePiecesCountActual;
  UINT64                OverlapBitFlag1;
  UINT64                OverlapBitFlag2;
  INT32                 OverlapFlagRelation;

  AllEndPointsCount         = RawMtrrMemoryRangesCount << 1;
  AllEndPointsInclusive     = calloc (AllEndPointsCount, sizeof (UINT64));
  AllRangePiecesCountMax    = RawMtrrMemoryRangesCount * 3 + 1;
  AllRangePieces            = calloc (AllRangePiecesCountMax, sizeof (MTRR_MEMORY_RANGE));
  CollectRawMtrrRangesEndpointsAndSortAndRemoveDuplicates (AllEndPointsInclusive, &AllEndPointsCount, RawMtrrMemoryRanges, RawMtrrMemoryRangesCount);

  for (Index = 0, AllRangePiecesCountActual = 0; Index < AllEndPointsCount - 1; Index++) {
    OverlapBitFlag1 = GetOverlapBitFlag (RawMtrrMemoryRanges, RawMtrrMemoryRangesCount, AllEndPointsInclusive[Index]);
    OverlapBitFlag2 = GetOverlapBitFlag (RawMtrrMemoryRanges, RawMtrrMemoryRangesCount, AllEndPointsInclusive[Index + 1]);
    OverlapFlagRelation = CheckOverlapBitFlagsRelation (OverlapBitFlag1, OverlapBitFlag2);
    switch (OverlapFlagRelation) {
      case 0:   // [1, 2]
        AllRangePieces[AllRangePiecesCountActual].BaseAddress = AllEndPointsInclusive[Index];
        AllRangePieces[AllRangePiecesCountActual].Length      = AllEndPointsInclusive[Index + 1] - AllEndPointsInclusive[Index] + 1;
        AllRangePiecesCountActual++;
        break;
      case 1:   // [1, 2)
        AllRangePieces[AllRangePiecesCountActual].BaseAddress = AllEndPointsInclusive[Index];
        AllRangePieces[AllRangePiecesCountActual].Length      = (AllEndPointsInclusive[Index + 1] - 1) - AllEndPointsInclusive[Index] + 1;
        AllRangePiecesCountActual++;
        break;
      case 2:   // (1, 2]
        AllRangePieces[AllRangePiecesCountActual].BaseAddress = AllEndPointsInclusive[Index] + 1;
        AllRangePieces[AllRangePiecesCountActual].Length      = AllEndPointsInclusive[Index + 1] - (AllEndPointsInclusive[Index] + 1) + 1;
        AllRangePiecesCountActual++;

        if (!IsLeftSkippedEndpointAlreadyCollected (AllEndPointsInclusive[Index], AllRangePieces, AllRangePiecesCountActual)) {
          AllRangePieces[AllRangePiecesCountActual].BaseAddress = AllEndPointsInclusive[Index];
          AllRangePieces[AllRangePiecesCountActual].Length      = 1;
          AllRangePiecesCountActual++;
        }
        break;
      case 3:   // (1, 2)
        AllRangePieces[AllRangePiecesCountActual].BaseAddress = AllEndPointsInclusive[Index] + 1;
        AllRangePieces[AllRangePiecesCountActual].Length      = (AllEndPointsInclusive[Index + 1] - 1) - (AllEndPointsInclusive[Index] + 1) + 1;
        if (AllRangePieces[AllRangePiecesCountActual].Length == 0)   // Only in case 3 can exists Length=0, we should skip such "segment".
          break;
        AllRangePiecesCountActual++;
        if (!IsLeftSkippedEndpointAlreadyCollected (AllEndPointsInclusive[Index], AllRangePieces, AllRangePiecesCountActual)) {
          AllRangePieces[AllRangePiecesCountActual].BaseAddress = AllEndPointsInclusive[Index];
          AllRangePieces[AllRangePiecesCountActual].Length      = 1;
          AllRangePiecesCountActual++;
        }
        break;
      default:
        ASSERT (FALSE);
    }
  }

  for (Index = 0; Index < AllRangePiecesCountActual; Index++) {
    DetermineMemoryCacheType (DefaultType, &AllRangePieces[Index], RawMtrrMemoryRanges, RawMtrrMemoryRangesCount);
  }

  CompactAndExtendEffectiveMtrrMemoryRanges (DefaultType, PhysicalAddressBits, &AllRangePieces, &AllRangePiecesCountActual);
  ASSERT (*EffectiveMtrrMemoryRangesCount >= AllRangePiecesCountActual);
  memcpy (EffectiveMtrrMemoryRanges, AllRangePieces, AllRangePiecesCountActual * sizeof (MTRR_MEMORY_RANGE));
  *EffectiveMtrrMemoryRangesCount = AllRangePiecesCountActual;

  free (AllEndPointsInclusive);
  free (AllRangePieces);
}
