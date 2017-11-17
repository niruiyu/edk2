/** @file
  Provides interface of shell parameters parsing functionality.

  (C) Copyright 2016 Hewlett Packard Enterprise Development LP<BR>
  Copyright 2016 Dell Inc.
  Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include <Uefi.h>

#include <Protocol/UnicodeCollation.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellParametersLib.h>
#include <Library/MemoryAllocationLib.h>

typedef struct {
  UINT32           Signature;
  LIST_ENTRY       Link;
  CHAR16           *Name;
  SHELL_FLAG_TYPE  Type;
  CHAR16           *Value;
  UINTN            Position;
} SHELL_PARAM_ENTRY;

#define SHELL_PARAM_ENTRY_SIGNATURE    SIGNATURE_32 ('_', 's', 'p', 'e')
#define SHELL_PARAM_ENTRY_FROM_LINK(l) CR (l, SHELL_PARAM_ENTRY, Link, SHELL_PARAM_ENTRY_SIGNATURE)

EFI_UNICODE_COLLATION_PROTOCOL  *mEnglish = NULL;

VOID
ShellParametersLibFreeParamEntry (
  IN SHELL_PARAM_ENTRY                  *Entry
  )
{
  ASSERT (Entry != NULL);
  if (Entry->Name != NULL) {
    FreePool (Entry->Name);
  }

  if (Entry->Value != NULL) {
    FreePool (Entry->Value);
  }

  FreePool (Entry);
}

/**
  Return Unicode Collation2 instance for the specified language.

  @param  Language             The language code in RFC 4646 format.

  @return                      The Unicode Collation2 instance.

**/
EFI_UNICODE_COLLATION_PROTOCOL *
ShellParametersLibGetUnicodeCollation (
  IN CONST CHAR8        *Language
  )
{
  EFI_STATUS                      Status;
  UINTN                           HandleCount;
  UINTN                           Index;
  EFI_HANDLE                      *Handles;
  EFI_UNICODE_COLLATION_PROTOCOL  *Uci;
  CHAR8                           *BestLanguage;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiUnicodeCollation2ProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  Uci = NULL;
  for (Index = 0; Index < HandleCount; Index++) {
    //
    // Open Unicode Collation Protocol
    //
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiUnicodeCollation2ProtocolGuid,
                    (VOID **) &Uci
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Find the best matching matching language from the supported languages
    // of Unicode Collation (2) protocol. 
    //
    BestLanguage = GetBestLanguage (
                     Uci->SupportedLanguages,
                     FALSE,
                     Language,
                     NULL
                     );
    if (BestLanguage != NULL) {
      FreePool (BestLanguage);
      break;
    }
  }

  FreePool (Handles);
  if (Index == HandleCount) {
    return NULL;
  }
  return Uci;
}


/**
  Function to compare 2 strings without regard to case of the characters.

  @param[in] Str1            Pointer to String to compare.
  @param[in] Str2            Pointer to second String to compare.

  @retval 0                     Str1 equal to Str2.
  @retval <0                    Str1 is less than Str2.
  @retval >0                    Str1 is greater than Str2.
**/
INTN
EFIAPI
ShellParametersLibStringNICompare (
  IN  CONST CHAR16             *Str1,
  IN  CONST CHAR16             *Str2,
  IN  UINTN                    Length
  )
{
  CHAR16                    *Str1Copy;
  CHAR16                    *Str2Copy;
  UINTN                     Str1Length;
  UINTN                     Str2Length;
  INTN                      Result;

  if (mEnglish == NULL) {
    mEnglish = ShellParametersLibGetUnicodeCollation ("en");
    ASSERT (mEnglish != NULL);
    if (mEnglish == NULL) {
      return -1;
    }
  }

  if (Length == 0) {
    return mEnglish->StriColl (mEnglish, (CHAR16 *)Str1, (CHAR16 *)Str2);
  } else {
    Str1Length = StrLen (Str1);
    Str2Length = StrLen (Str2);
    Str1Length = MIN (Str1Length, Length);
    Str2Length = MIN (Str2Length, Length);
    Str1Copy = AllocateCopyPool ((Str1Length + 1) * sizeof (CHAR16), Str1);
    if (Str1Copy == NULL) {
      return -1;
    }
    Str2Copy = AllocateCopyPool ((Str2Length + 1) * sizeof (CHAR16), Str2);
    if (Str2Copy == NULL) {
      FreePool (Str1Copy);
      return -1;
    }
    
    Str1Copy[Str1Length] = L'\0';
    Str2Copy[Str2Length] = L'\0';
    Result = mEnglish->StriColl (mEnglish, Str1Copy, Str2Copy);
    FreePool (Str1Copy);
    FreePool (Str2Copy);
    return Result;
  }
}

/**
  Checks the list of valid arguments and returns TRUE if the item was found.  If the
  return value is TRUE then the type parameter is set also.

  if CheckList is NULL then ASSERT();
  if Name is NULL then ASSERT();
  if Type is NULL then ASSERT();

  @param Name                   pointer to Name of parameter found
  @param CheckList              List to check against
  @param Type                   pointer to type of parameter if it was found

  @retval TRUE                  the Parameter was found.  Type is valid.
  @retval FALSE                 the Parameter was not found.  Type is not valid.
**/
BOOLEAN
ShellParametersLibInCheckList (
  IN CONST CHAR16               *Name,
  IN CONST SHELL_FLAG_ITEM      *CheckList,
  OUT SHELL_FLAG_TYPE           *Type
  )
{
  //
  // ASSERT that all 3 pointer parameters aren't NULL
  //
  ASSERT(CheckList  != NULL);
  ASSERT(Type       != NULL);
  ASSERT(Name       != NULL);

  //
  // question mark and page break mode are always supported
  //
  if ((StrCmp (Name, L"-?") == 0) ||
      (StrCmp (Name, L"-b") == 0) ||
      (StrCmp (Name, L"-B") == 0)
     ) {
     *Type = FlagTypeSwitch;
     return TRUE;
  }

  //
  // Enumerate through the list
  //
  for (; CheckList->Name != NULL; CheckList++) {
    //
    // If the Type is FlagTypeStart only check the first characters of the passed in param
    // If it matches set the type and return TRUE
    //
    if (CheckList->Type == FlagTypeStart) {
      if (ShellParametersLibStringNICompare (Name, CheckList->Name, StrLen(CheckList->Name)) == 0) {
        *Type = FlagTypeStart;
        return TRUE;
      }
    } else if (ShellParametersLibStringNICompare (Name, CheckList->Name, 0) == 0) {
      *Type = CheckList->Type;
      return TRUE;
    }
  }

  return FALSE;
}
/**
  Checks the string for indicators of "flag" status.  this is a leading '/', '-', or '+'

  @param[in] Name               pointer to Name of parameter found
  @param[in] AlwaysAllowNumbers TRUE to allow numbers, FALSE to not.
  @param[in] TimeNumbers        TRUE to allow numbers with ":", FALSE otherwise.

  @retval TRUE                  the Parameter is a flag.
  @retval FALSE                 the Parameter not a flag.
**/
BOOLEAN
ShellParametersLibIsFlag (
  IN CONST CHAR16         *Name,
  IN BOOLEAN              NumbersFirst,
  IN BOOLEAN              TimeNumbers
  )
{
  RETURN_STATUS           Status;
  UINT64                  Data;
  CHAR16                  *EndPointer;
  //
  // ASSERT that Name isn't NULL
  //
  ASSERT(Name != NULL);

  //
  // If we accept numbers then dont return TRUE. (they will be values)
  //
  if ((Name[0] == L'-') || (Name[0] == L'+') && NumbersFirst) {
    do {
      Name++;
      Status = StrHexToUint64S (Name, &EndPointer, &Data);
      if (RETURN_ERROR (Status)) {
        //
        // It's not a number, means it's a flag.
        //
        return TRUE;
      }

      if (*EndPointer == L'\0') {
        //
        // It's a number.
        //
        return FALSE;
      } else if (*EndPointer == L':' && TimeNumbers) {
        Name = EndPointer;
      } else {
        //
        // It's not a number, means it's a flag.
        //
        return TRUE;
      }
    } while (TimeNumbers);
  }

  //
  // If the Name has a /, +, or - as the first character return TRUE
  //
  return (BOOLEAN)((Name[0] == L'/') || (Name[0] == L'-') || (Name[0] == L'+'));
}

/**
  Checks the command line arguments passed against the list of valid ones.
  Optionally removes NULL values first.

  If no initialization is required, then return RETURN_SUCCESS.

  @param[in] CheckList          The pointer to list of parameters to check.
  @param[out] Parameters        The parameters list.
  @param[out] ProblemParam      Optional pointer to pointer to unicode string for
                                the paramater that caused failure.
  @param[in] AutoPageBreak      Will automatically set PageBreakEnabled.
  @param[in] AlwaysAllowNumbers Will never fail for number based flags.

  @retval EFI_SUCCESS           The operation completed sucessfully.
  @retval EFI_OUT_OF_RESOURCES  A memory allocation failed.
  @retval EFI_INVALID_PARAMETER A parameter was invalid.
  @retval EFI_VOLUME_CORRUPTED  The command line was corrupt.
  @retval EFI_DEVICE_ERROR      The commands contained 2 opposing arguments.  One
                                of the command line arguments was returned in
                                ProblemParam if provided.
  @retval EFI_NOT_FOUND         A argument required a value that was missing.
                                The invalid command line argument was returned in
                                ProblemParam if provided.
**/
EFI_STATUS
EFIAPI
ShellParametersParse (
  IN CONST CHAR16               **Argv,
  IN UINTN                      Argc,
  IN CONST SHELL_FLAG_ITEM     *CheckList,
  OUT LIST_ENTRY                *Parameters,
  OUT CHAR16                    **ProblemParam, OPTIONAL
  IN  UINT32                    ParseOptions
  )
{
  UINTN                         Index;
  SHELL_FLAG_TYPE               Type;
  SHELL_PARAM_ENTRY             *Entry;
  UINTN                         ValueLeft;
  UINTN                         ValueSize;
  UINTN                         Count;
  CONST CHAR16                  *TempPointer;
  UINTN                         CurrentValueSize;
  CHAR16                        *NewValue;
  BOOLEAN                       NumbersFirst;

  ASSERT (CheckList != NULL);
  ASSERT (Argv != NULL);
  ASSERT (Parameters != NULL);

  for (Index = 0; CheckList[Index].Name != NULL; Index++) {
    if ((CheckList[Index].Type >= FlagTypeMax) || (CheckList[Index].Type == FlagTypePosition)) {
      return EFI_INVALID_PARAMETER;
    }
  }

  InitializeListHead (Parameters);
  //
  // If there is only 1 item we dont need to do anything
  //
  if (Argc < 1) {
    return EFI_SUCCESS;
  }

  Entry = NULL;
  ValueLeft = 0;
  ValueSize = 0;
  Count = 0;
  NumbersFirst = (BOOLEAN)((ParseOptions & SHELL_PARAMETERS_PARSE_NUMBERS_FIRST) != 0);

  //
  // loop through each of the arguments
  //
  for (Index = 0; Index < Argc; ++Index) {
    if (Argv[Index] == NULL) {
      continue;
    }

    if (ShellParametersLibInCheckList (Argv[Index], CheckList, &Type)) {
      //
      // Meet a flag
      //

      if (ValueLeft != 0) {
        //
        // The optional value for the flag might not exist.
        //
        ASSERT (Entry != NULL);
        ValueLeft = 0;
        InsertHeadList (Parameters, &Entry->Link);
      }
      Entry = AllocateZeroPool (sizeof (SHELL_PARAM_ENTRY));
      if (Entry == NULL) {
        ShellParametersFree (Parameters);
        return EFI_OUT_OF_RESOURCES;
      }
      Entry->Name = AllocateCopyPool (StrSize (Argv[Index]), Argv[Index]);
      if (Entry->Name == NULL) {
        ShellParametersFree (Parameters);
        FreePool (Entry);
        return EFI_OUT_OF_RESOURCES;
      }
      Entry->Signature = SHELL_PARAM_ENTRY_SIGNATURE;
      Entry->Type = Type;
      Entry->Position = (UINTN)-1;
      Entry->Value = NULL;
      ValueSize = 0;

      //
      // Does this flag require a value
      //
      switch (Entry->Type) {
      case FlagTypeValue:
      case FlagTypeTimeValue:
        ValueLeft = 1;
        break;
      case FlagTypeDoubleValue:
        ValueLeft = 2;
        break;
      case FlagTypeMaxValue:
        ValueLeft = (UINTN)(-1);
        break;
      default:
        ASSERT ((Entry->Type == FlagTypeSwitch) || (Entry->Type == FlagTypeStart));
        //
        // this item has no value expected; we are done
        //
        InsertHeadList (Parameters, &Entry->Link);
        Entry = NULL;
        break;
      }
    } else if (ValueLeft != 0 && Entry != NULL && !ShellParametersLibIsFlag (Argv[Index], NumbersFirst, (BOOLEAN)(Entry->Type == FlagTypeTimeValue))
      ) {
      //
      // 2. Meet a value for current flag
      //
      CurrentValueSize = ValueSize + StrSize (Argv[Index]) + sizeof (CHAR16);
      NewValue = ReallocatePool (ValueSize, CurrentValueSize, Entry->Value);
      if (NewValue == NULL) {
        if (Entry->Value != NULL) {
          FreePool (Entry->Value);
        }
        FreePool (Entry->Name);
        FreePool (Entry);
        ShellParametersFree (Parameters);
        return EFI_OUT_OF_RESOURCES;
      }
      Entry->Value = NewValue;
      if (ValueSize == 0) {
        StrCpyS (Entry->Value, CurrentValueSize / sizeof (CHAR16), Argv[Index]);
      } else {
        StrCatS (Entry->Value, CurrentValueSize / sizeof (CHAR16), L" ");
        StrCatS (Entry->Value, CurrentValueSize / sizeof (CHAR16), Argv[Index]);
      }
      ValueSize += StrSize (Argv[Index]) + sizeof (CHAR16);

      ValueLeft--;
      if (ValueLeft == 0) {
        InsertHeadList (Parameters, &Entry->Link);
        Entry = NULL;
      }
    } else if (!ShellParametersLibIsFlag (Argv[Index], NumbersFirst, FALSE)) {
      //
      // 3. Meet a position parameter
      //

      TempPointer = Argv[Index];
      if ((*TempPointer == L'^' && *(TempPointer + 1) == L'-')
        || (*TempPointer == L'^' && *(TempPointer + 1) == L'/')
        || (*TempPointer == L'^' && *(TempPointer + 1) == L'+')
        ) {
        TempPointer++;
      }
      Entry = AllocateZeroPool (sizeof (SHELL_PARAM_ENTRY));
      if (Entry == NULL) {
        ShellParametersFree (Parameters);
        return EFI_OUT_OF_RESOURCES;
      }
      Entry->Name = NULL;
      Entry->Type = FlagTypePosition;
      Entry->Value = AllocateCopyPool (StrSize (TempPointer), TempPointer);
      if (Entry->Value == NULL) {
        ShellParametersFree (Parameters);
        return EFI_OUT_OF_RESOURCES;
      }
      Entry->Signature = SHELL_PARAM_ENTRY_SIGNATURE;
      Entry->Position = Count++;
      InsertHeadList (Parameters, &Entry->Link);
      Entry = NULL;
    } else {
      //
      // this was a non-recognised flag... error!
      //
      if (ProblemParam != NULL) {
        *ProblemParam = (CHAR16 *)Argv[Index];
      }
      ShellParametersFree (Parameters);
      if (Entry != NULL) {
        ShellParametersLibFreeParamEntry (Entry);
      }
      return EFI_VOLUME_CORRUPTED;
    }
  }
  if (Entry != NULL) {
    //
    // Meet a flag without enough values.
    //
    ASSERT (ValueLeft != 0);
    InsertHeadList (Parameters, &Entry->Link);
  }
  return EFI_SUCCESS;
}

/**
  Frees shell variable list that was returned from ShellParametersParse.

  This function will free all the memory that was used for the Parameters
  list of postprocessed shell arguments.

  this function has no return value.

  if Parameters is NULL, then ASSERT().

  @param Parameters           the list to de-allocate
  **/
VOID
EFIAPI
ShellParametersFree (
  IN LIST_ENTRY                 *Parameters
  )
{
  LIST_ENTRY                    *Link;
  SHELL_PARAM_ENTRY             *Entry;

  ASSERT (Parameters != NULL);

  //
  // for each node in the list
  //
  Link = GetFirstNode (Parameters);
  while (!IsNull (Parameters, Link)) {
    Entry = SHELL_PARAM_ENTRY_FROM_LINK (Link);
    Link = RemoveEntryList (Link);

    ShellParametersLibFreeParamEntry (Entry);
  }
}

/**
  Return the entry for the specified param.

  flag arguments are in the form of "-<Key>" or "/<Key>", but do not have a value following the key

  if Parameters is NULL then ASSERT().
  if KeyString is NULL then ASSERT().

  @param Parameters             The package of parsed command line arguments
  @param KeyString              The Key of the command line argument to check for

  @return                       A pointer to SHELL_PARAM_ENTRY
  **/
CONST SHELL_PARAM_ENTRY *
EFIAPI
ShellParametersLibGetFlagEntry (
  IN CONST LIST_ENTRY         *Parameters,
  IN CONST CHAR16             *KeyString
  )
{
  LIST_ENTRY                    *Link;
  SHELL_PARAM_ENTRY             *Entry;

  ASSERT (Parameters != NULL);
  ASSERT (KeyString != NULL);

  //
  // enumerate through the list of parametrs
  //
  for ( Link = GetFirstNode (Parameters)
      ; !IsNull (Parameters, Link)
      ; Link = GetNextNode (Parameters, Link)
      ){
    Entry = SHELL_PARAM_ENTRY_FROM_LINK (Link);
    //
    // If the Name matches, return TRUE (and there may be NULL name)
    //
    if (Entry->Name != NULL) {
      //
      // If Type is FlagTypeStart then only compare the begining of the strings
      //
      if (Entry->Type == FlagTypeStart) {
        if (ShellParametersLibStringNICompare (KeyString, Entry->Name, StrLen (KeyString)) == 0) {
          return Entry;
        }
      } else if (ShellParametersLibStringNICompare (KeyString, Entry->Name, 0) == 0) {
        return Entry;
      }
    }
  }
  return NULL;
}

/**
  Checks for presence of a flag parameter

  flag arguments are in the form of "-<Key>" or "/<Key>", but do not have a value following the key

  if Parameters is NULL then ASSERT().
  if KeyString is NULL then ASSERT().

  @param Parameters             The package of parsed command line arguments
  @param KeyString              The Key of the command line argument to check for

  @retval TRUE                  The flag is on the command line
  @retval FALSE                 The flag is not on the command line
  **/
BOOLEAN
EFIAPI
ShellParametersGetFlag (
  IN CONST LIST_ENTRY         *Parameters,
  IN CONST CHAR16             *KeyString
  )
{
  return (BOOLEAN)(ShellParametersLibGetFlagEntry (Parameters, KeyString) != NULL);
}

/**
  Returns value from command line argument.

  Value parameters are in the form of "-<Key> value" or "/<Key> value".

  If Parameters is NULL, then return NULL.

  @param[in] Parameters         The package of parsed command line arguments.
  @param[in] KeyString          The Key of the command line argument to check for.

  @retval NULL                  The flag is not on the command line.
  @retval !=NULL                The pointer to unicode string of the value.
**/
CONST CHAR16*
EFIAPI
ShellParametersGetFlagValue (
  IN CONST LIST_ENTRY           *Parameters,
  IN CONST CHAR16                     *KeyString
  )
{
  CONST SHELL_PARAM_ENTRY             *Entry;

  Entry = ShellParametersLibGetFlagEntry (Parameters, KeyString);
  if (Entry == NULL) {
    return NULL;
  }

  if (Entry->Type == FlagTypeStart) {
    return Entry->Name + StrLen (KeyString);
  } else {
    return Entry->Value;
  }
}

/**
  Returns position value from command line argument.

  If Parameters is NULL, then ASSERT().

  @param[in] Parameters         The package of parsed command line arguments.
  @param[in] Position           The position of the value.

  @retval NULL                  There is no parameter in specified position.
  @retval !=NULL                The pointer to unicode string of the value.
  **/
CONST CHAR16*
EFIAPI
ShellParametersGetPositionValue (
  IN CONST LIST_ENTRY           *Parameters,
  IN UINTN                      Position
  )
{
  LIST_ENTRY                    *Link;
  SHELL_PARAM_ENTRY             *Entry;

  ASSERT (Parameters != NULL);

  //
  // enumerate through the list of parametrs
  //
  for ( Link = GetFirstNode(Parameters)
      ; !IsNull (Parameters, Link)
      ; Link = GetNextNode(Parameters, Link)
     ){
    Entry = SHELL_PARAM_ENTRY_FROM_LINK (Link);
    //
    // If the position matches, return the value
    //
    if (Entry->Position == Position) {
      return Entry->Value;
    }
  }
  return NULL;
}

/**
  returns the number of command line value parameters that were parsed.

  this will not include flags.

  If Parameters is NULL, then ASSERT().

  @param[in] Parameters       The package of parsed command line arguments.

  @retval (UINTN)-1     No parsing has ocurred
  @return other         The number of value parameters found
**/
UINTN
EFIAPI
ShellParametersGetPositionValueCount(
  IN CONST LIST_ENTRY           *Parameters
  )
{
  UINTN                         Count;
  LIST_ENTRY                    *Link;
  SHELL_PARAM_ENTRY             *Entry;

  ASSERT (Parameters != NULL);
  Count = 0;
  //
  // enumerate through the list of parametrs
  //
  for ( Link = GetFirstNode (Parameters)
      ; !IsNull (Parameters, Link)
      ; Link = GetNextNode (Parameters, Link)
      ) {
    Entry = SHELL_PARAM_ENTRY_FROM_LINK (Link);
    //
    // If the position matches, return the value
    //
    if (Entry->Name == NULL) {
      Count++;
    }
  }
  return Count;
}

/**
  Return the first found duplicate flag.

  If Parameters is NULL, then ASSERT().

  @param[in] Parameters         The package of parsed command line arguments.
  @param[out] Param             Upon finding one, a pointer to the duplicated parameter.

  @return Callee allocated duplicate flag, or NULL if no duplicate flag exists.
  **/
CONST CHAR16 *
EFIAPI
ShellParametersGetDuplicateFlag (
  IN CONST LIST_ENTRY              *Parameters
  )
{
  LIST_ENTRY                    *Link;
  LIST_ENTRY                    *Link2;
  SHELL_PARAM_ENTRY             *Entry;
  SHELL_PARAM_ENTRY             *Entry2;

  ASSERT(Parameters != NULL);

  for ( Link = GetFirstNode(Parameters)
      ; !IsNull (Parameters, Link)
      ; Link = GetNextNode(Parameters, Link)
      ) {
    Entry = SHELL_PARAM_ENTRY_FROM_LINK (Link);
    if (Entry->Name == NULL) {
      continue;
    }
    for ( Link2 = GetNextNode(Parameters, Link)
        ; !IsNull (Parameters, Link2)
        ; Link2 = GetNextNode(Parameters, Link2)
        ) {
      Entry2 = SHELL_PARAM_ENTRY_FROM_LINK (Link2);
      if (Entry2->Name == NULL) {
        continue;
      }
      if (StrCmp(Entry->Name, Entry2->Name) == 0) {
        return Entry->Name;
      }
    }
  }
  return NULL;
}