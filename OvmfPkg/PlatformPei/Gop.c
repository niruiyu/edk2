#include <PiPei.h>
#include <IndustryStandard/Pci30.h>
#include <Guid/GraphicsInfoHob.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/PciLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>

#define VBE_DISPI_INDEX_ID               0x0
#define VBE_DISPI_INDEX_XRES             0x1
#define VBE_DISPI_INDEX_YRES             0x2
#define VBE_DISPI_INDEX_BPP              0x3
#define VBE_DISPI_INDEX_ENABLE           0x4
#define VBE_DISPI_INDEX_BANK             0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH       0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT      0x7
#define VBE_DISPI_INDEX_X_OFFSET         0x8
#define VBE_DISPI_INDEX_Y_OFFSET         0x9
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0xa

#define VBE_DISPI_ID0                    0xB0C0
#define VBE_DISPI_ID1                    0xB0C1
#define VBE_DISPI_ID2                    0xB0C2
#define VBE_DISPI_ID3                    0xB0C3
#define VBE_DISPI_ID4                    0xB0C4
#define VBE_DISPI_ID5                    0xB0C5

#define VBE_DISPI_DISABLED               0x00
#define VBE_DISPI_ENABLED                0x01
#define VBE_DISPI_GETCAPS                0x02
#define VBE_DISPI_8BIT_DAC               0x20
#define VBE_DISPI_LFB_ENABLED            0x40
#define VBE_DISPI_NOCLEARMEM             0x80

//
// Io Registers defined by VGA
//
#define CRTC_ADDRESS_REGISTER   0x3d4
#define CRTC_DATA_REGISTER      0x3d5
#define SEQ_ADDRESS_REGISTER    0x3c4
#define SEQ_DATA_REGISTER       0x3c5
#define GRAPH_ADDRESS_REGISTER  0x3ce
#define GRAPH_DATA_REGISTER     0x3cf
#define ATT_ADDRESS_REGISTER    0x3c0
#define MISC_OUTPUT_REGISTER    0x3c2
#define INPUT_STATUS_1_REGISTER 0x3da
#define DAC_PIXEL_MASK_REGISTER 0x3c6
#define PALETTE_INDEX_REGISTER  0x3c8
#define PALETTE_DATA_REGISTER   0x3c9

typedef enum {
  QEMU_VIDEO_CIRRUS_5430 = 1,
  QEMU_VIDEO_CIRRUS_5446,
  QEMU_VIDEO_BOCHS,
  QEMU_VIDEO_BOCHS_MMIO,
} QEMU_VIDEO_VARIANT;

typedef struct {
  UINT16                                VendorId;
  UINT16                                DeviceId;
  QEMU_VIDEO_VARIANT                    Variant;
  CHAR16                                *Name;
} QEMU_VIDEO_CARD;

typedef struct {
  UINT32  Width;
  UINT32  Height;
  UINT32  ColorDepth;
} QEMU_VIDEO_BOCHS_MODES;

typedef struct {
  UINT32  InternalModeIndex; // points into card-specific mode table
  UINT32  HorizontalResolution;
  UINT32  VerticalResolution;
  UINT32  ColorDepth;
  UINT32  RefreshRate;
  UINT32  FrameBufferSize;
} QEMU_VIDEO_MODE_DATA;

//
// QEMU Video PCI Configuration Header values
//
#define CIRRUS_LOGIC_VENDOR_ID                0x1013
#define CIRRUS_LOGIC_5430_DEVICE_ID           0x00a8
#define CIRRUS_LOGIC_5430_ALTERNATE_DEVICE_ID 0x00a0
#define CIRRUS_LOGIC_5446_DEVICE_ID           0x00b8

QEMU_VIDEO_CARD gQemuVideoCardList[] = {
  {
    CIRRUS_LOGIC_VENDOR_ID,
    CIRRUS_LOGIC_5430_DEVICE_ID,
    QEMU_VIDEO_CIRRUS_5430,
    L"Cirrus 5430"
  }, {
    CIRRUS_LOGIC_VENDOR_ID,
    CIRRUS_LOGIC_5430_ALTERNATE_DEVICE_ID,
    QEMU_VIDEO_CIRRUS_5430,
    L"Cirrus 5430"
  }, {
    CIRRUS_LOGIC_VENDOR_ID,
    CIRRUS_LOGIC_5446_DEVICE_ID,
    QEMU_VIDEO_CIRRUS_5446,
    L"Cirrus 5446"
  }, {
    0x1234,
    0x1111,
    QEMU_VIDEO_BOCHS_MMIO,
    L"QEMU Standard VGA"
  }, {
    0x1b36,
    0x0100,
    QEMU_VIDEO_BOCHS,
    L"QEMU QXL VGA"
  }, {
    0x1af4,
    0x1050,
    QEMU_VIDEO_BOCHS_MMIO,
    L"QEMU VirtIO VGA"
  }, {
    0 /* end of list */
  }
};

///
/// Table of supported video modes
///
QEMU_VIDEO_BOCHS_MODES  QemuVideoBochsModes[] = {
  { 640,  480, 32 },
  { 800,  480, 32 },
  { 800,  600, 32 },
  { 832,  624, 32 },
  { 960,  640, 32 },
  { 1024,  600, 32 },
  { 1024,  768, 32 },
  { 1152,  864, 32 },
  { 1152,  870, 32 },
  { 1280,  720, 32 },
  { 1280,  760, 32 },
  { 1280,  768, 32 },
  { 1280,  800, 32 },
  { 1280,  960, 32 },
  { 1280, 1024, 32 },
  { 1360,  768, 32 },
  { 1366,  768, 32 },
  { 1400, 1050, 32 },
  { 1440,  900, 32 },
  { 1600,  900, 32 },
  { 1600, 1200, 32 },
  { 1680, 1050, 32 },
  { 1920, 1080, 32 },
  { 1920, 1200, 32 },
  { 1920, 1440, 32 },
  { 2000, 2000, 32 },
  { 2048, 1536, 32 },
  { 2048, 2048, 32 },
  { 2560, 1440, 32 },
  { 2560, 1600, 32 },
  { 2560, 2048, 32 },
  { 2800, 2100, 32 },
  { 3200, 2400, 32 },
  { 3840, 2160, 32 },
  { 4096, 2160, 32 },
  { 7680, 4320, 32 },
  { 8192, 4320, 32 }
};

#define QEMU_VIDEO_BOCHS_MODE_COUNT \
  (sizeof (QemuVideoBochsModes) / sizeof (QemuVideoBochsModes[0]))


VOID
BochsWrite (
  UINT16                   Reg,
  UINT16                   Data
)
{
  UINT32       Bar2;
  Bar2 = PciRead32 (PCI_LIB_ADDRESS (0, 2, 0, 0x18));
  MmioWrite16 (Bar2 + 0x500 + (Reg << 1), Data);
}

UINT16
BochsRead (
  UINT16                   Reg
)
{
  UINT32       Bar2;
  Bar2 = PciRead32 (PCI_LIB_ADDRESS (0, 2, 0, 0x18));
  return MmioRead16 (Bar2 + 0x500 + (Reg << 1));
}

VOID
VgaOutb (
  UINTN                    Reg,
  UINT8                    Data
)
{
  UINT32       Bar2;
  Bar2 = PciRead32 (PCI_LIB_ADDRESS (0, 2, 0, 0x18));
  MmioWrite8 (Bar2 + 0x400 - 0x3c0 + Reg, Data);
}

VOID
QemuVideoBochsModeSetup (
  QEMU_VIDEO_MODE_DATA                   *ModeData
  )
{
  UINT32                                 AvailableFbSize;
  UINT32                                 Index;
  QEMU_VIDEO_BOCHS_MODES                 *VideoMode;

  //
  // Fetch the available framebuffer size.
  //
  // VBE_DISPI_INDEX_VIDEO_MEMORY_64K is expected to return the size of the
  // drawable framebuffer. Up to and including qemu-2.1 however it used to
  // return the size of PCI BAR 0 (ie. the full video RAM size).
  //
  // On stdvga the two concepts coincide with each other; the full memory size
  // is usable for drawing.
  //
  // On QXL however, only a leading segment, "surface 0", can be used for
  // drawing; the rest of the video memory is used for the QXL guest-host
  // protocol. VBE_DISPI_INDEX_VIDEO_MEMORY_64K should report the size of
  // "surface 0", but since it doesn't (up to and including qemu-2.1), we
  // retrieve the size of the drawable portion from a field in the QXL ROM BAR,
  // where it is also available.
  //
  AvailableFbSize = BochsRead (VBE_DISPI_INDEX_VIDEO_MEMORY_64K);
  AvailableFbSize *= SIZE_64KB;
  DEBUG ((EFI_D_ERROR, "%a: AvailableFbSize=0x%x\n", __FUNCTION__,
          AvailableFbSize));

  //
  // Setup Video Modes
  //
  VideoMode = &QemuVideoBochsModes[0];
  for (Index = 0; Index < QEMU_VIDEO_BOCHS_MODE_COUNT; Index++) {
    UINTN RequiredFbSize;
    DEBUG ((EFI_D_INFO, "Width/Height = %d/%d\n", VideoMode->Width, VideoMode->Height));
    if (VideoMode->Width != 800 || VideoMode->Height != 600) {
      VideoMode++;
      continue;
    }

    ASSERT (VideoMode->ColorDepth % 8 == 0);
    RequiredFbSize = VideoMode->Width * VideoMode->Height *
      (VideoMode->ColorDepth / 8);
    DEBUG ((EFI_D_INFO, "Required/Available = %x/%x\n", RequiredFbSize, AvailableFbSize));
    if (RequiredFbSize <= AvailableFbSize) {
      ModeData->InternalModeIndex = Index;
      ModeData->HorizontalResolution = VideoMode->Width;
      ModeData->VerticalResolution = VideoMode->Height;
      ModeData->ColorDepth = VideoMode->ColorDepth;
      ModeData->RefreshRate = 60;
      ModeData->FrameBufferSize = (UINT32) RequiredFbSize;
      DEBUG ((EFI_D_ERROR,
              "Adding Mode as Bochs Internal Mode %d: %dx%d, %d-bit, %d Hz, FB Size %x\n",
              ModeData->InternalModeIndex,
              ModeData->HorizontalResolution,
              ModeData->VerticalResolution,
              ModeData->ColorDepth,
              ModeData->RefreshRate,
              ModeData->FrameBufferSize
              ));
      break;
    }
    VideoMode++;
  }
}

static QEMU_VIDEO_CARD*
QemuVideoDetect (
  IN UINT16 VendorId,
  IN UINT16 DeviceId
)
{
  UINTN Index = 0;

  while (gQemuVideoCardList[Index].VendorId != 0) {
    if (gQemuVideoCardList[Index].VendorId == VendorId &&
        gQemuVideoCardList[Index].DeviceId == DeviceId) {
      return gQemuVideoCardList + Index;
    }
    Index++;
  }
  return NULL;
}

VOID
SetPaletteColor (
  UINTN                           Index,
  UINT8                           Red,
  UINT8                           Green,
  UINT8                           Blue
)
{
  VgaOutb (PALETTE_INDEX_REGISTER, (UINT8) Index);
  VgaOutb (PALETTE_DATA_REGISTER, (UINT8) (Red >> 2));
  VgaOutb (PALETTE_DATA_REGISTER, (UINT8) (Green >> 2));
  VgaOutb (PALETTE_DATA_REGISTER, (UINT8) (Blue >> 2));
}

VOID
SetDefaultPalette (
  VOID
)
{
  UINTN Index;
  UINTN RedIndex;
  UINTN GreenIndex;
  UINTN BlueIndex;

  Index = 0;
  for (RedIndex = 0; RedIndex < 8; RedIndex++) {
    for (GreenIndex = 0; GreenIndex < 8; GreenIndex++) {
      for (BlueIndex = 0; BlueIndex < 4; BlueIndex++) {
        SetPaletteColor (Index, (UINT8) (RedIndex << 5), (UINT8) (GreenIndex << 5), (UINT8) (BlueIndex << 6));
        Index++;
      }
    }
  }
}

VOID
InitializeBochsGraphicsMode (
  QEMU_VIDEO_BOCHS_MODES  *ModeData
)
{
  DEBUG ((EFI_D_INFO, "InitializeBochsGraphicsMode: %dx%d @ %d\n",
          ModeData->Width, ModeData->Height, ModeData->ColorDepth));

  /* unblank */
  VgaOutb (ATT_ADDRESS_REGISTER, 0x20);

  BochsWrite (VBE_DISPI_INDEX_ENABLE, 0);
  BochsWrite (VBE_DISPI_INDEX_BANK, 0);
  BochsWrite (VBE_DISPI_INDEX_X_OFFSET, 0);
  BochsWrite (VBE_DISPI_INDEX_Y_OFFSET, 0);

  BochsWrite (VBE_DISPI_INDEX_BPP, (UINT16) ModeData->ColorDepth);
  BochsWrite (VBE_DISPI_INDEX_XRES, (UINT16) ModeData->Width);
  BochsWrite (VBE_DISPI_INDEX_VIRT_WIDTH, (UINT16) ModeData->Width);
  BochsWrite (VBE_DISPI_INDEX_YRES, (UINT16) ModeData->Height);
  BochsWrite (VBE_DISPI_INDEX_VIRT_HEIGHT, (UINT16) ModeData->Height);

  BochsWrite (VBE_DISPI_INDEX_ENABLE,
              VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

  SetDefaultPalette ();
}

VOID GopInitialization ()
{
  UINT16                            VendorId;
  UINT16                            DeviceId;
  QEMU_VIDEO_CARD                   *Card;
  UINTN                             Device;
  UINT16                            BochsId;
  QEMU_VIDEO_MODE_DATA              Mode;

  for (Device = 0; Device < PCI_MAX_DEVICE; Device++) {
    VendorId = PciRead16 (PCI_LIB_ADDRESS (0, Device, 0, 0));
    DeviceId = PciRead16 (PCI_LIB_ADDRESS (0, Device, 0, 2));
    //
    // Determine card variant.
    //
    Card = QemuVideoDetect (VendorId, DeviceId);
    if (Card != NULL) {
      break;
    }
  }

  ASSERT (Device == 2);

  //
  // Check whenever the qemu stdvga mmio bar is present (qemu 1.3+).
  //
  ASSERT (Card->Variant == QEMU_VIDEO_BOCHS_MMIO);
  //
  // Enable Gfx
  //
  PciWrite32 (PCI_LIB_ADDRESS (0, 2, 0, 0x10), 0x90000000);
  PciWrite32 (PCI_LIB_ADDRESS (0, 2, 0, 0x18), 0x92000000);
  PciWrite16 (PCI_LIB_ADDRESS (0, 2, 0, 4), 0x3);

  //
  // Check if accessing the bochs interface works.
  //
  BochsId = BochsRead (VBE_DISPI_INDEX_ID);
  ASSERT ((BochsId & 0xFFF0) == VBE_DISPI_ID0);


  //
  // Construct video mode buffer
  //
  QemuVideoBochsModeSetup (&Mode);

  InitializeBochsGraphicsMode (&QemuVideoBochsModes[Mode.InternalModeIndex]);
  {
    EFI_PEI_GRAPHICS_INFO_HOB  GraphicsInfo;
    EFI_PEI_GRAPHICS_DEVICE_INFO_HOB DeviceInfo;
    GraphicsInfo.FrameBufferBase = PciRead32 (PCI_LIB_ADDRESS (0, 2, 0, 0x10)) & 0xFFFFFFF0;
    GraphicsInfo.FrameBufferSize = Mode.FrameBufferSize;
    GraphicsInfo.GraphicsMode.Version = 0;
    GraphicsInfo.GraphicsMode.HorizontalResolution = Mode.HorizontalResolution;
    GraphicsInfo.GraphicsMode.VerticalResolution = Mode.VerticalResolution;
    GraphicsInfo.GraphicsMode.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
    GraphicsInfo.GraphicsMode.PixelsPerScanLine = Mode.HorizontalResolution;
    BuildGuidDataHob (&gEfiGraphicsInfoHobGuid, &GraphicsInfo, sizeof (GraphicsInfo));

    DeviceInfo.VendorId = 0x1234;
    DeviceInfo.DeviceId = 0x1111;
    DeviceInfo.SubsystemId = 0xFFFF;
    DeviceInfo.SubsystemVendorId = 0xFFFF;
    DeviceInfo.RevisionId = 0xFF;
    DeviceInfo.BarIndex = 0;
    BuildGuidDataHob (&gEfiGraphicsDeviceInfoHobGuid, &DeviceInfo, sizeof (DeviceInfo));
  }
}