/** @file
  This library retrieve the EFI_BOOT_SERVICES pointer from EFI system table in
  library's constructor.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include <Uefi.h>



extern VOID  *mHobList;


/**
  Reads a 64-bit value from memory that may be unaligned.

  This function returns the 64-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 64-bit value that may be unaligned.

  @return The 64-bit value read from Buffer.

**/
static
UINT64
EFIAPI
ReadUnaligned64 (
  IN CONST UINT64              *Buffer
  )
{

  return *Buffer;
}

static
BOOLEAN
EFIAPI
CompareGuid (
  IN CONST GUID  *Guid1,
  IN CONST GUID  *Guid2
  )
{
  UINT64  LowPartOfGuid1;
  UINT64  LowPartOfGuid2;
  UINT64  HighPartOfGuid1;
  UINT64  HighPartOfGuid2;

  LowPartOfGuid1  = ReadUnaligned64 ((CONST UINT64*) Guid1);
  LowPartOfGuid2  = ReadUnaligned64 ((CONST UINT64*) Guid2);
  HighPartOfGuid1 = ReadUnaligned64 ((CONST UINT64*) Guid1 + 1);
  HighPartOfGuid2 = ReadUnaligned64 ((CONST UINT64*) Guid2 + 1);

  return (BOOLEAN) (LowPartOfGuid1 == LowPartOfGuid2 && HighPartOfGuid1 == HighPartOfGuid2);
}


/**
  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiPayloadInitHobLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{

  //
  // Cache pointer to the EFI System Table
  //
  UINTN             Index;


  for (Index = 0; Index < SystemTable->NumberOfTableEntries; Index++) {
    if (CompareGuid (&gEfiHobListGuid, &(SystemTable->ConfigurationTable[Index].VendorGuid))) {
      mHobList = SystemTable->ConfigurationTable[Index].VendorTable;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}
