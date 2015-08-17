/** @file
  GopBltLib - Library to perform blt using the UEFI Graphics Output Protocol.

  Copyright (c) 2007 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php
  
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>

#include <Protocol/GraphicsOutput.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

/**
  Look for the GOP instance based on the FrameBuffer and the FrameBufferInfo.

  @param[in] FrameBuffer      Pointer to the start of the frame buffer
  @param[in] FrameBufferInfo  Describes the frame buffer characteristics

  @return The found GOP instance.
**/
EFI_GRAPHICS_OUTPUT_PROTOCOL *
BltLibFindGopInstance (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo
  )
{
  EFI_STATUS                      Status;
  EFI_HANDLE                      *HandleBuffer;
  UINTN                           HandleCount;
  UINTN                           Index;
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *Gop;

  Gop = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **) &Gop
                    );
    if (!EFI_ERROR (Status) &&
        (FrameBuffer == (VOID *) (UINTN) Gop->Mode->FrameBufferBase) &&
        (CompareMem (FrameBufferInfo, Gop->Mode->Info, sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) == 0)
       ) {
      break;
    }
  }

  FreePool (HandleBuffer);

  if (Index == HandleCount) {
    return NULL;
  } else {
    return Gop;
  }
}

/**
  Performs a UEFI Graphics Output Protocol Blt operation.

  @param[in]     FrameBuffer     Pointer to the start of the frame buffer
  @param[in]     FrameBufferInfo Describes the frame buffer characteristics
  @param[in,out] BltBuffer       The data to transfer to screen
  @param[in]     BltOperation    The operation to perform
  @param[in]     SourceX         The X coordinate of the source for BltOperation
  @param[in]     SourceY         The Y coordinate of the source for BltOperation
  @param[in]     DestinationX    The X coordinate of the destination for BltOperation
  @param[in]     DestinationY    The Y coordinate of the destination for BltOperation
  @param[in]     Width           The width of a rectangle in the blt rectangle in pixels
  @param[in]     Height          The height of a rectangle in the blt rectangle in pixels
  @param[in]     Delta           Not used for EfiBltVideoFill and EfiBltVideoToVideo operation.
                                 If a Delta of 0 is used, the entire BltBuffer will be operated on.
                                 If a subrectangle of the BltBuffer is used, then Delta represents
                                 the number of bytes in a row of the BltBuffer.

  @retval  EFI_DEVICE_ERROR      A hardware error occured
  @retval  EFI_INVALID_PARAMETER Invalid parameter passed in
  @retval  EFI_SUCCESS           Blt operation success

**/
EFI_STATUS
BltLibGopBltCommon (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *BltBuffer, OPTIONAL
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION    BltOperation,
  IN  UINTN                                SourceX,
  IN  UINTN                                SourceY,
  IN  UINTN                                DestinationX,
  IN  UINTN                                DestinationY,
  IN  UINTN                                Width,
  IN  UINTN                                Height,
  IN  UINTN                                Delta
  )
{
  EFI_GRAPHICS_OUTPUT_PROTOCOL             *Gop;

  Gop = BltLibFindGopInstance (FrameBuffer, FrameBufferInfo);
  if (Gop == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return Gop->Blt (
                Gop,
                BltBuffer,
                BltOperation,
                SourceX,
                SourceY,
                DestinationX,
                DestinationY,
                Width,
                Height,
                Delta
                );
}

/**
  Performs a UEFI Graphics Output Protocol Blt Video Fill.

  @param[in]  FrameBuffer     Pointer to the start of the frame buffer
  @param[in]  FrameBufferInfo Describes the frame buffer characteristics
  @param[in]  Color           Color to fill the region with
  @param[in]  DestinationX    X location to start fill operation
  @param[in]  DestinationY    Y location to start fill operation
  @param[in]  Width           Width (in pixels) to fill
  @param[in]  Height          Height to fill

  @retval  EFI_DEVICE_ERROR      A hardware error occured
  @retval  EFI_INVALID_PARAMETER Invalid parameter passed in
  @retval  EFI_SUCCESS           Blt operation success

**/
EFI_STATUS
EFIAPI
BltVideoFill (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *Color,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height
  )
{
  return BltLibGopBltCommon (
           FrameBuffer,
           FrameBufferInfo,
           Color,
           EfiBltVideoFill,
           0,
           0,
           DestinationX,
           DestinationY,
           Width,
           Height,
           0
           );
}

/**
  Performs a UEFI Graphics Output Protocol Blt Video to Buffer operation.

  @param[in]  FrameBuffer     Pointer to the start of the frame buffer
  @param[in]  FrameBufferInfo Describes the frame buffer characteristics
  @param[out] BltBuffer       Output buffer for pixel color data
  @param[in]  SourceX         X location within video
  @param[in]  SourceY         Y location within video
  @param[in]  DestinationX    X location within BltBuffer
  @param[in]  DestinationY    Y location within BltBuffer
  @param[in]  Width           Width (in pixels)
  @param[in]  Height          Height
  @param[in]  Delta           Number of bytes in a row of BltBuffer

  @retval  EFI_DEVICE_ERROR      A hardware error occured
  @retval  EFI_INVALID_PARAMETER Invalid parameter passed in
  @retval  EFI_SUCCESS           Blt operation success

**/
EFI_STATUS
EFIAPI
BltVideoToBuffer (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *BltBuffer,
  IN  UINTN                                SourceX,
  IN  UINTN                                SourceY,
  IN  UINTN                                DestinationX,
  IN  UINTN                                DestinationY,
  IN  UINTN                                Width,
  IN  UINTN                                Height,
  IN  UINTN                                Delta
  )
{
  return BltLibGopBltCommon (
           FrameBuffer,
           FrameBufferInfo,
           BltBuffer,
           EfiBltVideoToBltBuffer,
           SourceX,
           SourceY,
           DestinationX,
           DestinationY,
           Width,
           Height,
           Delta
           );
}

/**
  Performs a UEFI Graphics Output Protocol Blt Buffer to Video operation.

  @param[in]  FrameBuffer     Pointer to the start of the frame buffer
  @param[in]  FrameBufferInfo Describes the frame buffer characteristics
  @param[in]  BltBuffer       Output buffer for pixel color data
  @param[in]  SourceX         X location within BltBuffer
  @param[in]  SourceY         Y location within BltBuffer
  @param[in]  DestinationX    X location within video
  @param[in]  DestinationY    Y location within video
  @param[in]  Width           Width (in pixels)
  @param[in]  Height          Height
  @param[in]  Delta           Number of bytes in a row of BltBuffer

  @retval  EFI_DEVICE_ERROR      A hardware error occured
  @retval  EFI_INVALID_PARAMETER Invalid parameter passed in
  @retval  EFI_SUCCESS           Blt operation success

**/
EFI_STATUS
EFIAPI
BltBufferToVideo (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *BltBuffer,
  IN  UINTN                                SourceX,
  IN  UINTN                                SourceY,
  IN  UINTN                                DestinationX,
  IN  UINTN                                DestinationY,
  IN  UINTN                                Width,
  IN  UINTN                                Height,
  IN  UINTN                                Delta
  )
{
  return BltLibGopBltCommon (
           FrameBuffer,
           FrameBufferInfo,
           BltBuffer,
           EfiBltBufferToVideo,
           SourceX,
           SourceY,
           DestinationX,
           DestinationY,
           Width,
           Height,
           Delta
           );
}

/**
  Performs a UEFI Graphics Output Protocol Blt Video to Video operation.

  @param[in]  FrameBuffer     Pointer to the start of the frame buffer
  @param[in]  FrameBufferInfo Describes the frame buffer characteristics
  @param[in]  SourceX         X location within video
  @param[in]  SourceY         Y location within video
  @param[in]  DestinationX    X location within video
  @param[in]  DestinationY    Y location within video
  @param[in]  Width           Width (in pixels)
  @param[in]  Height          Height

  @retval  EFI_DEVICE_ERROR      A hardware error occured
  @retval  EFI_INVALID_PARAMETER Invalid parameter passed in
  @retval  EFI_SUCCESS           Blt operation success

**/
EFI_STATUS
EFIAPI
BltVideoToVideo (
  IN  VOID                                 *FrameBuffer,
  IN  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *FrameBufferInfo,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height
  )
{
  return BltLibGopBltCommon (
           FrameBuffer,
           FrameBufferInfo,
           NULL,
           EfiBltVideoToVideo,
           SourceX,
           SourceY,
           DestinationX,
           DestinationY,
           Width,
           Height,
           0
           );
}
