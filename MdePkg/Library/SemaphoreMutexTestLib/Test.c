#include <Uefi.h>
#include <PiPei.h>
#include <PiDxe.h>
#include <Library/SemaphoreLib.h>
#include <Library/MutexLib.h>
#include <Library/DebugLib.h>
#ifdef MDE_CPU_IA32
#include <Ppi/MpServices.h>
#include <Library/PeiServicesLib.h>
#else
#include <Protocol/MpService.h>
#include <Library/UefiBootServicesTableLib.h>
#endif
#include <Library/SerialPortLib.h>
#include <Library/PrintLib.h>
#include <Library/LocalApicLib.h>
#include <Library/TimerLib.h>

static const GUID mMutexName =
{ 0xb158cce6, 0x857, 0x4452,{ 0x94, 0xee, 0xf5, 0xc4, 0x41, 0x6b, 0xb8, 0x50 } };

static const GUID mSemaphoreName =
{ 0x9a440c03, 0xe6b3, 0x4845,{ 0xa5, 0xce, 0x59, 0xe, 0x6, 0x8c, 0xbd, 0x69 } };

static const GUID x =
{ 0xe3b7e708, 0xb486, 0x4d0f,{ 0x96, 0x4a, 0x99, 0x16, 0x18, 0x38, 0x98, 0x86 } };

typedef struct {
  SEMAPHORE s;
  MUTEX     m;
} AP_CONTEXT;

VOID
EFIAPI
ApProcedure (
  IN OUT VOID  *Buffer
)
{
  RETURN_STATUS RStatus;
  MUTEX m;
  SEMAPHORE s;
  CHAR8 B[100];
  AP_CONTEXT  *ApContext;

  ApContext = (AP_CONTEXT *)Buffer;

  RStatus = MutexCreate (&mMutexName, &m);
  ASSERT_RETURN_ERROR (RStatus);

  RStatus = SemaphoreCreate (&mSemaphoreName, &s, 0);
  ASSERT_RETURN_ERROR (RStatus);

  ASSERT (ApContext->m == m);
  ASSERT (ApContext->s == s);

  MutexAcquire (m, 0);
  MutexAcquire (m, 0);
  SerialPortWrite (B,
    AsciiSPrint (B, sizeof (B), "[%x]: I am here!\n", GetApicId ())
  );
  MutexRelease (m);

  SerialPortWrite (B,
    AsciiSPrint (B, sizeof (B), "[%x]: I am sleeping...!\n", GetApicId ())
  );
  MicroSecondDelay (5000000);
  SemaphoreRelease (s);
  MutexRelease (m);
}


RETURN_STATUS
EFIAPI
SemaphoreMutexTestLibConstructor (
  VOID
)
{
  RETURN_STATUS RStatus;
  MUTEX   m, m1;
  SEMAPHORE s, s1;
  UINTN i;
  CHAR8 B[100];
  AP_CONTEXT ApContext;

  //
  // Test creation
  //
  RStatus = MutexCreate (&mMutexName, &m);
  ASSERT_RETURN_ERROR (RStatus);

  RStatus = MutexCreate (&x, &m1);
  ASSERT_RETURN_ERROR (RStatus);
  ASSERT (m != m1);
  RStatus = MutexDestroy (m1);
  ASSERT_RETURN_ERROR (RStatus);

  RStatus = MutexCreate (&mMutexName, &m1);
  ASSERT_RETURN_ERROR (RStatus);
  ASSERT (m == m1);

  RStatus = SemaphoreCreate (&mSemaphoreName, &s, 7);
  ASSERT_RETURN_ERROR (RStatus);

  RStatus = SemaphoreCreate (&mSemaphoreName, &s1, 3);
  ASSERT_RETURN_ERROR (RStatus);
  ASSERT (s == s1);

  for (i = 0; i < 7; i++) {
    RStatus = SemaphoreAcquireOrFail (s);
    ASSERT_RETURN_ERROR (RStatus);
  }
  RStatus = SemaphoreAcquireOrFail (s);
  ASSERT (RStatus == RETURN_NOT_READY);
  for (i = 0; i < 7; i++) {
    RStatus = SemaphoreRelease (s);
    ASSERT_RETURN_ERROR (RStatus);
  }

  SerialPortWrite (B,
    AsciiSPrint (B, sizeof (B), "Call MP services\n")
  );

  ApContext.m = m;
  ApContext.s = s;

  {
    EFI_STATUS                Status;
#ifdef MDE_CPU_IA32
    EFI_PEI_MP_SERVICES_PPI   *Mp;
    Status = PeiServicesLocatePpi (&gEfiPeiMpServicesPpiGuid, 0, NULL, (VOID **)&Mp);
    ASSERT_EFI_ERROR (Status);
    SerialPortWrite (B,
      AsciiSPrint (B, sizeof (B), "Call PEI AP procedures\n")
    );
    Status = Mp->StartupAllAPs (NULL, Mp, ApProcedure, FALSE, 0, &ApContext);
    ASSERT_EFI_ERROR (Status);
#else
    EFI_MP_SERVICES_PROTOCOL  *Mp;
    Status = gBS->LocateProtocol (&gEfiMpServiceProtocolGuid, NULL, (VOID **)&Mp);
    ASSERT_EFI_ERROR (Status);
    SerialPortWrite (B,
      AsciiSPrint (B, sizeof (B), "Call DXE AP procedures\n")
    );
    Status = Mp->StartupAllAPs (Mp, ApProcedure, FALSE, NULL, 0, &ApContext, NULL);
    ASSERT_EFI_ERROR (Status);
#endif
  }

  SerialPortWrite (B,
    AsciiSPrint (B, sizeof (B), "BSP: Wait AP.....\n")
  );
  for (i = 0; i < 7; i++) {
    RStatus = SemaphoreAcquire (s, 0);
    ASSERT_RETURN_ERROR (RStatus);
  }

  SerialPortWrite (B,
    AsciiSPrint (B, sizeof (B), "BSP: Byebye\n")
  );
  return RETURN_SUCCESS;
}