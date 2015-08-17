/** @file
  Example program using BltLib

  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/BltLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>


UINT64
ReadTimestamp (
  VOID
  )
{
#if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64)
  return AsmReadTsc ();
#elif defined (MDE_CPU_IPF)
  return AsmReadItc ();
#else
#error ReadTimestamp not supported for this architecture!
#endif
}

UINT32
Rand32 (
  VOID
  )
{
  UINTN    Found;
  INTN     Bits;
  UINT64   Tsc1;
  UINT64   Tsc2;
  UINT64   TscBits;
  UINT32   R32;

  R32 = 0;
  Found = 0;
  Tsc1 = ReadTimestamp ();
  Tsc2 = ReadTimestamp ();
  do {
    Tsc2 = ReadTimestamp ();
    TscBits = Tsc2 ^ Tsc1;
    Bits = HighBitSet64 (TscBits);
    if (Bits > 0) {
      Bits = Bits - 1;
    }
    R32 = (UINT32)((R32 << Bits) |
                   RShiftU64 (LShiftU64 (TscBits, (UINTN) (64 - Bits)), (UINTN) (64 - Bits)));
    Found = Found + Bits;
  } while (Found < 32);

  return R32;
}


VOID
TestFills (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color;
  UINTN                          Loop;
  UINTN                          X;
  UINTN                          Y;
  UINTN                          W;
  UINTN                          H;

  for (Loop = 0; Loop < 1000; Loop++) {
    W = FrameBufferInfo->HorizontalResolution - (Rand32 () % FrameBufferInfo->HorizontalResolution);
    H = FrameBufferInfo->VerticalResolution - (Rand32 () % FrameBufferInfo->VerticalResolution);
    if (W != FrameBufferInfo->HorizontalResolution) {
      X = Rand32 () % (FrameBufferInfo->HorizontalResolution - W);
    } else {
      X = 0;
    }
    if (H != FrameBufferInfo->VerticalResolution) {
      Y = Rand32 () % (FrameBufferInfo->VerticalResolution - H);
    } else {
      Y = 0;
    }
    *(UINT32*) (&Color) = Rand32 () & 0xffffff;
    BltVideoFill (FrameBuffer, FrameBufferInfo, &Color, X, Y, W, H);
  }
}


VOID
TestColor1 (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *BltBuffer;
  UINTN                          X;
  UINTN                          Y;

  BltBuffer = AllocatePool (sizeof (*BltBuffer) * FrameBufferInfo->HorizontalResolution);
  ASSERT (BltBuffer != NULL);

  for (Y = 0; Y < FrameBufferInfo->VerticalResolution; Y++) {
    for (X = 0; X < FrameBufferInfo->HorizontalResolution; X++) {
      BltBuffer[X].Red =   (UINT8) ((X * 0x100) / FrameBufferInfo->HorizontalResolution);
      BltBuffer[X].Green = (UINT8) ((Y * 0x100) / FrameBufferInfo->VerticalResolution);
      BltBuffer[X].Blue =  (UINT8) ((Y * 0x100) / FrameBufferInfo->VerticalResolution);
    }
    BltBufferToVideo (FrameBuffer, FrameBufferInfo, BltBuffer, 0, 0, 0, Y, FrameBufferInfo->HorizontalResolution, 1, 0);
  }
}


UINT32
Uint32SqRt (
  IN  UINT32  Uint32
  )
{
  UINT32 Mask;
  UINT32 SqRt;
  UINT32 SqRtMask;
  UINT32 Squared;

  if (Uint32 == 0) {
    return 0;
  }

  for (SqRt = 0, Mask = (UINT32) (1 << (HighBitSet32 (Uint32) / 2));
       Mask != 0;
       Mask = Mask >> 1
      ) {
    SqRtMask = SqRt | Mask;
    //DEBUG ((EFI_D_INFO, "Uint32=0x%x SqRtMask=0x%x\n", Uint32, SqRtMask));
    Squared = (UINT32) (SqRtMask * SqRtMask);
    if (Squared > Uint32) {
      continue;
    } else if (Squared < Uint32) {
      SqRt = SqRtMask;
    } else {
      return SqRtMask;
    }
  }

  return SqRt;
}


UINT32
Uint32Dist (
  IN UINTN X,
  IN UINTN Y
  )
{
  return Uint32SqRt ((UINT32) ((X * X) + (Y * Y)));
}

UINT8
GetTriColor (
  IN UINTN ColorDist,
  IN UINTN TriWidth
  )
{
  return (UINT8) (((TriWidth - ColorDist) * 0x100) / TriWidth);
  //return (((TriWidth * TriWidth - ColorDist * ColorDist) * 0x100) / (TriWidth * TriWidth));
}

VOID
TestColor (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color;
  UINTN                          X, Y;
  UINTN                          X1, X2, X3;
  UINTN                          Y1, Y2;
  UINTN                          LineWidth, TriWidth;
  UINTN                          TriHeight;
  UINT32                         ColorDist;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *BltBuffer;

  *(UINT32*) (&Color) = 0;
  BltVideoFill (FrameBuffer, FrameBufferInfo, &Color, 0, 0, FrameBufferInfo->HorizontalResolution, FrameBufferInfo->VerticalResolution);

  TriWidth = (UINTN) DivU64x32 (
                       MultU64x32 (11547005, FrameBufferInfo->HorizontalResolution),
                       10000000
                       );
  TriHeight = (UINTN) DivU64x32 (
                        MultU64x32 (8660254, FrameBufferInfo->VerticalResolution),
                        10000000
                        );
  if (TriWidth > FrameBufferInfo->HorizontalResolution) {
    DEBUG ((EFI_D_INFO, "TriWidth at %d was too big\n", TriWidth));
    TriWidth = FrameBufferInfo->HorizontalResolution;
  } else if (TriHeight > FrameBufferInfo->VerticalResolution) {
    DEBUG ((EFI_D_INFO, "TriHeight at %d was too big\n", TriHeight));
    TriHeight = FrameBufferInfo->VerticalResolution;
  }

  DEBUG ((EFI_D_INFO, "Triangle Width: %d; Height: %d\n", TriWidth, TriHeight));

  X1 = (FrameBufferInfo->HorizontalResolution - TriWidth) / 2;
  X3 = X1 + TriWidth - 1;
  X2 = (X1 + X3) / 2;
  Y2 = (FrameBufferInfo->VerticalResolution - TriHeight) / 2;
  Y1 = Y2 + TriHeight - 1;

  BltBuffer = AllocatePool (sizeof (*BltBuffer) * FrameBufferInfo->HorizontalResolution);
  ASSERT (BltBuffer != NULL);

  for (Y = Y2; Y <= Y1; Y++) {
    LineWidth =
      (UINTN) DivU64x32 (
                MultU64x32 (11547005, (UINT32) (Y - Y2)),
                20000000
                );
    for (X = X2 - LineWidth; X < (X2 + LineWidth); X++) {
      ColorDist = Uint32Dist(X - X1, Y1 - Y);
      BltBuffer[X - (X2 - LineWidth)].Red = GetTriColor (ColorDist, TriWidth);

      ColorDist = Uint32Dist((X < X2) ? X2 - X : X - X2, Y - Y2);
      BltBuffer[X - (X2 - LineWidth)].Green = GetTriColor (ColorDist, TriWidth);

      ColorDist = Uint32Dist(X3 - X, Y1 - Y);
      BltBuffer[X - (X2 - LineWidth)].Blue = GetTriColor (ColorDist, TriWidth);
    }
    BltBufferToVideo (FrameBuffer, FrameBufferInfo, BltBuffer, 0, 0, X2 - LineWidth, Y, LineWidth * 2, 1, 0);
  }
  FreePool (BltBuffer);
  gBS->Stall (1000 * 1000);
}


VOID
TestMove (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Blue;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;
  UINTN                          X, Y;
  UINTN                          Width;
  UINTN                          Height;

  Width = 100;
  Height = 20;

  *(UINT32 *) &Black = 0;
  BltVideoFill (FrameBuffer, FrameBufferInfo, &Black, 0, 0, FrameBufferInfo->HorizontalResolution, FrameBufferInfo->VerticalResolution);

  *(UINT32 *) &Blue = 0;
  Blue.Blue = 0xff;
  BltVideoFill (FrameBuffer, FrameBufferInfo, &Blue, 0, 0, Width, Height);

  for (X = 1, Y = 1; X < FrameBufferInfo->HorizontalResolution && Y < FrameBufferInfo->VerticalResolution; X++, Y++) {
    BltVideoToVideo (FrameBuffer, FrameBufferInfo, X - 1, Y - 1, X, Y, Width, Height);
    gBS->Stall (100);
  }
  gBS->Stall (1000 * 1000 * 2);
}

/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *Gop;

  Status = gBS->HandleProtocol (
                  gST->ConsoleOutHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **) &Gop
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  TestMove (
    (VOID *) (UINTN) Gop->Mode->FrameBufferBase,
    Gop->Mode->Info
    );

  TestFills (
    (VOID *) (UINTN) Gop->Mode->FrameBufferBase,
    Gop->Mode->Info
    );

  TestColor (
    (VOID *) (UINTN) Gop->Mode->FrameBufferBase,
    Gop->Mode->Info
    );
  gBS->Stall (3 * 1000 * 1000);

  TestColor1 (
    (VOID *) (UINTN) Gop->Mode->FrameBufferBase,
    Gop->Mode->Info
    );
  return EFI_SUCCESS;
}
