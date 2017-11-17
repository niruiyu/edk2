/** @file
  Provides interface to shell functionality for shell commands and applications.

  This library is derived from the parameter parsing APIs defined in ShellPkg/ShellLib.

  This library APIs separate the parameters as two categories:
  1. Flag parameter.
     The parameter comsists of a flag and the value of flag.
     Retrieving the value needs to specify the flag as the key.
     The value type for the flag is defined by SHELL_PARAM_TYPE.
  2. Position parameter.
     The parameter isn't associated with a flag key, but associated with the position.
     Retrieving the value needs to specify the position as the key.

  The library APIs don't support duplicate flags. E.g.: for "-a hello -a world", "hello"
  can be retrieved as the value of flag "-a", but "world" cannot be retrieved.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __SHELL_PARAMETERS_LIB__
#define __SHELL_PARAMETERS_LIB__

typedef enum {
  FlagTypeSwitch,       ///< A flag that is present or not present only (IE "-a").
  FlagTypeValue,        ///< A flag that has some data following it with a space (IE "-a 1").
  FlagTypePosition,     ///< Some data that did not follow a flag (IE "filename.txt").
  FlagTypeStart,        ///< A flag that has variable value appended to the end (IE "-ad", "-afd", "-adf", etc...).
  FlagTypeDoubleValue,  ///< A flag that has 2 space seperated value data following it (IE "-a 1 2").
  FlagTypeMaxValue,     ///< A flag followed by all the command line data before the next flag.
  FlagTypeTimeValue,    ///< A flag that has a time value following it (IE "-a -5:00").
  FlagTypeMax,
} SHELL_FLAG_TYPE;

typedef struct {
  CHAR16            *Name;
  SHELL_FLAG_TYPE   Type;
} SHELL_FLAG_ITEM;

#define SHELL_PARAMETERS_PARSE_NUMBERS_FIRST  BIT0

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
  );

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
  );

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
  );

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
  IN CONST CHAR16               *KeyString
  );

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
  );

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
  );

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
  );

#endif // __SHELL_PARAMETERS_LIB__

