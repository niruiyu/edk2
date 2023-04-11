/** @file
  Unit tests of the CpuPageTableLib instance of the CpuPageTableLib class

  Copyright (c) 2022 - 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuPageTableLibUnitTest.h"

// ----------------------------------------------------------------------- PageMode--TestCount-TestRangeCount---RandomOptions
// static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging4Level    = { Paging4Level, 30, 20, USE_RANDOM_ARRAY };
// static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging4Level1GB = { Paging4Level1GB, 30, 20, USE_RANDOM_ARRAY };
// static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging5Level    = { Paging5Level, 30, 20, USE_RANDOM_ARRAY };
// static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging5Level1GB = { Paging5Level1GB, 30, 20, USE_RANDOM_ARRAY };
// static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPagingPae       = { PagingPae, 30, 20, USE_RANDOM_ARRAY };

/**
  Initialize the unit test framework, suite, and unit tests for the
  sample unit tests and run the unit tests.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      RandomTestCase;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  //
  // Start setting up the test framework for running the tests.
  //
  Status = InitUnitTestFramework (&Framework, UNIT_TEST_APP_NAME, gEfiCallerBaseName, UNIT_TEST_APP_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    goto EXIT;
  }

  //
  // Populate the Random Test Cases.
  //
  Status = CreateUnitTestSuite (&RandomTestCase, Framework, "Random Test Cases", "CpuPageTableLib.Random", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Random Test Cases\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // AddTestCase (RandomTestCase, "Random Test for Paging4Level", "Random Test Case1", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging4Level);
  // AddTestCase (RandomTestCase, "Random Test for Paging4Level1G", "Random Test Case2", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging4Level1GB);
  // AddTestCase (RandomTestCase, "Random Test for Paging5Level", "Random Test Case3", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging5Level);
  // AddTestCase (RandomTestCase, "Random Test for Paging5Level1G", "Random Test Case4", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging5Level1GB);
  // AddTestCase (RandomTestCase, "Random Test for PagingPae", "Random Test Case5", TestCaseforRandomTest, NULL, NULL, &mTestContextPagingPae);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

/**
  Standard POSIX C entry point for host based unit test execution.

  @param Argc  Number of arguments.
  @param Argv  Array of arguments.

  @return Test application exit code.
**/
INT32
main (
  INT32  Argc,
  CHAR8  *Argv[]
  )
{
  InitGlobalData (52);
  return UefiTestMain ();
}
