/** @file

  Provide NVDIMM Label parsing functions.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "NvdimmBlockIoDxe.h"
#include "InternalBtt.h"

#define BLOCK_NAMESPACE_INIT_LABEL_COUNT 32
#define AUTO_CREATE_BTT
PMEM mPmem = {
  FALSE,
  { NULL },
  { 0 },
  INITIALIZE_LIST_HEAD_VARIABLE (mPmem.Nvdimms),
  INITIALIZE_LIST_HEAD_VARIABLE (mPmem.Namespaces)
};

NVDIMM_NAMESPACE_DEVICE_PATH  mNamespaceNodeTemplate = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_NVDIMM_NAMESPACE_DP,
    {sizeof (NVDIMM_NAMESPACE_DEVICE_PATH), 0}
  }
};

/**
  Return TRUE when the bit in the position Index is set.

  @param Index  The bit position to check.
  @param Bytes  The bit mask.

  @retval TRUE  The bit in the position Index is set.
  @retval FALSE The bit in the position Index is clear.
**/
BOOLEAN
IsBitSet (
  UINTN   Index,
  UINT8   *Bytes
  )
{
  return (BOOLEAN)((Bytes[Index / 8] & (1 << (Index % 8))) != 0);
}

/**
  Return TRUE when the Label Index Block is valid.

  @param LabelIndexBlock  The Label Index Block to check.

  @retval TRUE  The Label Index Block is valid.
  @retval FALSE The Label Index Block is invalid.
**/
BOOLEAN
IsLabelIndexValid (
  EFI_NVDIMM_LABEL_INDEX_BLOCK *LabelIndexBlock
  )
{
  UINT32 Remainder;
  if (CompareMem (LabelIndexBlock->Sig, EFI_NVDIMM_LABEL_INDEX_SIGNATURE, sizeof (LabelIndexBlock->Sig)) != 0) {
    return FALSE;
  }
  if (LabelIndexBlock->LabelSize == 0) {
    return FALSE;
  }
  if ((LabelIndexBlock->Seq > 3) || (LabelIndexBlock->Seq == 0)) {
    return FALSE;
  }
  DivU64x32Remainder (LabelIndexBlock->MySize, EFI_NVDIMM_LABEL_INDEX_ALIGN, &Remainder);
  if (Remainder != 0) {
    return FALSE;
  }

  if ((LabelIndexBlock->Major != 1) || (LabelIndexBlock->Minor != 2)) {
    return FALSE;
  }

  if (OFFSET_OF (EFI_NVDIMM_LABEL_INDEX_BLOCK, Free) + (LabelIndexBlock->NSlot + 7) / 8 > LabelIndexBlock->MySize) {
    return FALSE;
  }

  return IsFletcher64Valid (
    (UINT32 *)LabelIndexBlock,
    (UINTN)LabelIndexBlock->MySize / sizeof (UINT32),
    &LabelIndexBlock->Checksum
  );
}

/**
  Return the namespace type.

  @param  Flags   The namespace flags stored in the label storage.

  @retval NamespaceTypeBlock  The namespace is of NVDIMM_BLK_REGION type.
  @retval NamespaceTypePmem   The namespace is of PMEM type.
**/
NAMESPACE_TYPE
GetNamespaceType (
  UINT32   Flags
  )
{
  if ((Flags & EFI_NVDIMM_LABEL_FLAGS_LOCAL) != 0) {
    return NamespaceTypeBlock;
  } else {
    return NamespaceTypePmem;
  }
}

/**
  Return TRUE when the NVDIMM Label is valid.

  @param Label The Label to check.
  @param Slot  The Label position.

  @retval TRUE  The Label is valid.
  @retval FALSE The Label is invalid.
**/
BOOLEAN
IsLabelValid (
  EFI_NVDIMM_LABEL                *Label,
  UINT32                          Slot
  )
{
  NAMESPACE_TYPE                  Type;
  ASSERT (Label != NULL);
  if ((Label->RawSize == 0) || (Label->Slot != Slot)) {
    return FALSE;
  }

  if (GetPowerOfTwo32 (Label->Alignment) != Label->Alignment) {
    return FALSE;
  }

  Type = GetNamespaceType (Label->Flags);
  //
  // Block namespace check
  //
  if (Type == NamespaceTypeBlock) {
    if (!CompareGuid (&Label->TypeGuid, &gNvdimmBlockDataWindowRegionGuid)) {
      return FALSE;
    }
    if (Label->LbaSize == 0) {
      return FALSE;
    }
    if (((Label->Position == 0xFF) && (Label->NLabel == 0xFF)) ||
      ((Label->Position == 0) && (Label->NLabel != 0))) {
      //
      // The first label, the label with the lowest Dpa value, shall have Position 0 and non-zero NLabel value.
      // All labels other than the first have Position and NLabel set to 0xff.
      // TODO: Rough check here. Will check first label later after assembling.
      //
    } else {
      return FALSE;
    }
  }

  //
  // Pmem namespace check
  //
  if (Type == NamespaceTypePmem) {
    if (!CompareGuid (&Label->TypeGuid, &gNvdimmPersistentMemoryRegionGuid)) {
      return FALSE;
    }
    if (Label->Position >= Label->NLabel) {
      return FALSE;
    }
    if (Label->LbaSize != 0) {
      return FALSE;
    }
  }

  return IsFletcher64Valid ((UINT32 *)Label, sizeof (*Label) / sizeof (UINT32), &Label->Checksum);
}

/**
  Load all labels from Label Storage of the NVDIMM.

  @param Nvdimm The NVDIMM to load the labels.

  @retval EFI_SUCCESS           All the lables are loaded successfully.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource for label loading.
  @retval EFI_INVALID_PARAMETER The labels are invalid.
**/
EFI_STATUS
LoadNvdimmLabels (
  NVDIMM                          *Nvdimm
  )
{
  EFI_STATUS                      Status;
  UINT32                          SizeOfLabelStorageArea;
  UINT32                          MaxTransferLength;
  UINT32                          Offset;
  UINT32                          TransferLength;
  UINTN                           Index;
  EFI_NVDIMM_LABEL_PROTOCOL       *NvdimmLabel;
  EFI_NVDIMM_LABEL_INDEX_BLOCK    *LabelIndexBlock[2];

  //
  // Read the label storage.
  //
  Status = gBS->HandleProtocol (Nvdimm->Handle, &gEfiNvdimmLabelProtocolGuid, (VOID **)&NvdimmLabel);
  ASSERT_EFI_ERROR (Status);

  Status = NvdimmLabel->LabelStorageInformation (NvdimmLabel, &SizeOfLabelStorageArea, &MaxTransferLength);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Nvdimm->LabelStorageData = AllocatePool (SizeOfLabelStorageArea);
  if (Nvdimm->LabelStorageData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  for (Offset = 0; Offset < SizeOfLabelStorageArea; Offset += TransferLength) {
    TransferLength = MIN (MaxTransferLength, SizeOfLabelStorageArea - Offset);
    Status = NvdimmLabel->LabelStorageRead (NvdimmLabel, Offset, TransferLength, &Nvdimm->LabelStorageData[Offset]);
    if (EFI_ERROR (Status)) {
      FreePool (Nvdimm->LabelStorageData);
      return Status;
    }
  }
  ASSERT (Offset == SizeOfLabelStorageArea);

  LabelIndexBlock[0] = LabelIndexBlock[1] = NULL;
  //
  // Since the Label Index Block contains MyOff and OtherOff,
  // which implies that it may not start from 0, or not be adjacent to each other.
  //
  for (Index = 0, Offset = 0; (Index < 2) && (Offset < SizeOfLabelStorageArea); ) {
    if (CompareMem (
      &Nvdimm->LabelStorageData[Offset], EFI_NVDIMM_LABEL_INDEX_SIGNATURE, EFI_NVDIMM_LABEL_INDEX_SIG_LEN
    ) == 0) {
      LabelIndexBlock[Index] = (EFI_NVDIMM_LABEL_INDEX_BLOCK *)&Nvdimm->LabelStorageData[Offset];
      if (IsLabelIndexValid (LabelIndexBlock[Index]) && (LabelIndexBlock[Index]->MyOff == Offset)) {
        if (Index == 0) {
          Offset = (UINT32)LabelIndexBlock[Index]->OtherOff;
        }
        Index++;
      } else {
        LabelIndexBlock[Index] = NULL;
        Offset += EFI_NVDIMM_LABEL_INDEX_SIG_LEN;
      }
    } else {
      Offset++;
    }
  }

  if (LabelIndexBlock[0] == NULL) {
    Nvdimm->LabelIndexBlock = LabelIndexBlock[1];
    DEBUG ((DEBUG_WARN, "Label Index #0 is not valid!\n"));
  }

  if (LabelIndexBlock[1] == NULL) {
    Nvdimm->LabelIndexBlock = LabelIndexBlock[0];
    DEBUG ((DEBUG_WARN, " Label Index #1 is not valid!\n"));

    //
    // Both LabelIndexBlock are invalid.
    //
    if (Nvdimm->LabelIndexBlock == NULL) {
      FreePool (Nvdimm->LabelStorageData);
      return EFI_INVALID_PARAMETER;
    }
  }

  if ((LabelIndexBlock[0] != NULL) && (LabelIndexBlock[1] != NULL)) {

    if ((LabelIndexBlock[0]->OtherOff != LabelIndexBlock[1]->MyOff) ||
      (LabelIndexBlock[1]->OtherOff != LabelIndexBlock[0]->MyOff) ||
      (LabelIndexBlock[0]->MySize != LabelIndexBlock[1]->MySize) ||
      (LabelIndexBlock[0]->LabelSize != LabelIndexBlock[1]->LabelSize) ||
      (LabelIndexBlock[0]->LabelOff != LabelIndexBlock[1]->LabelOff) ||
      (LabelIndexBlock[0]->NSlot != LabelIndexBlock[0]->NSlot)
      ) {
      DEBUG ((DEBUG_WARN, "Label Index x-reference check fails!\n"));
      FreePool (Nvdimm->LabelStorageData);
      return EFI_INVALID_PARAMETER;
    }

    DEBUG ((DEBUG_INFO, "Label Index sequence number = #0(%x) / #1(%x).\n",
      LabelIndexBlock[0]->Seq, LabelIndexBlock[1]->Seq));;
    if (LabelIndexBlock[0]->Seq == LabelIndexBlock[1]->Seq) {
      //
      // If two Index Blocks with identical sequence numbers are found,
      // software shall treat the Index Block at the higher offset as the valid Index Block.
      //
      Nvdimm->LabelIndexBlock = LabelIndexBlock[1];
    } else {
      Nvdimm->LabelIndexBlock = LabelIndexBlock[SequenceHigher (LabelIndexBlock[0]->Seq, LabelIndexBlock[1]->Seq)];
    }
  }

  ASSERT (Nvdimm->LabelIndexBlock != NULL);
  Nvdimm->Labels = (EFI_NVDIMM_LABEL *)(Nvdimm->LabelStorageData + Nvdimm->LabelIndexBlock->LabelOff);
  return EFI_SUCCESS;
}

/**
  Return TRUE when the namespace is readonly.

  @param  Flags   The namespace flags stored in the label storage.

  @retval TRUE  The namespace is readonly.
  @retval FALSE The namespace is not readonly.
**/
BOOLEAN
IsNamespaceReadOnly (
  UINT32   Flags
  )
{
  if ((Flags & EFI_NVDIMM_LABEL_FLAGS_ROLABEL) == 0) {
    return FALSE;
  } else {
    return TRUE;
  }
}

/**
  Locate the namespace using the Uuid stored in label.
  A new namespace is created if the namespace cannot be found and Create is TRUE.

  @param Label  The label to be used for locating the namespace.
  @param Create TRUE to create a new namespace when unable to locate.

  @return  The found namespace or a new namespace.
**/
NVDIMM_NAMESPACE *
LocateNamespace (
  EFI_NVDIMM_LABEL *Label,
  BOOLEAN          Create
  )
{
  LIST_ENTRY         *Link;
  NVDIMM_NAMESPACE   *Namespace;
  for (Link = GetFirstNode (&mPmem.Namespaces)
    ; !IsNull (&mPmem.Namespaces, Link)
    ; Link = GetNextNode (&mPmem.Namespaces, Link)
    ) {
    Namespace = NVDIMM_NAMESPACE_FROM_LINK (Link);
    if (CompareGuid (&Namespace->Uuid, &Label->Uuid)) {
      return Namespace;
    }
  }

  if (!Create) {
    return NULL;
  }

  Namespace = AllocateZeroPool (sizeof (*Namespace));
  if (Namespace == NULL) {
    return NULL;
  }
  Namespace->Signature = NVDIMM_NAMESPACE_SIGNATURE;
  Namespace->Type      = GetNamespaceType (Label->Flags);
  Namespace->ReadOnly  = IsNamespaceReadOnly (Label->Flags);
  Namespace->LbaSize   = (UINT32)Label->LbaSize;
  Namespace->SetCookie = Label->SetCookie;
  CopyGuid (&Namespace->Uuid, &Label->Uuid);
  CopyMem (Namespace->Name, Label->Name, sizeof (Namespace->Name));
  CopyGuid (&Namespace->AddressAbstractionGuid, &Label->AddressAbstractionGuid);
  if (Namespace->Type == NamespaceTypePmem) {
    //
    // Number of labels equals to NLabel for PMEM namespaces.
    //
    Namespace->LabelCapacity = Label->NLabel;
    Namespace->DevicePath = AllocatePool (
      sizeof (ACPI_ADR_DEVICE_PATH) * Label->NLabel +
      sizeof (NVDIMM_NAMESPACE_DEVICE_PATH) +
      END_DEVICE_PATH_LENGTH
    );
  } else {
    //
    // Number of labels is unknown until assembling is completed for Local (Block) namespaces.
    //
    Namespace->LabelCapacity = BLOCK_NAMESPACE_INIT_LABEL_COUNT;
    Namespace->DevicePath = AllocatePool (
      sizeof (ACPI_ADR_DEVICE_PATH) +
      sizeof (NVDIMM_NAMESPACE_DEVICE_PATH) +
      END_DEVICE_PATH_LENGTH
    );
  }

  if (Namespace->DevicePath == NULL) {
    FreeNamespace (Namespace);
    return NULL;
  }
  Namespace->Labels = AllocateZeroPool (Namespace->LabelCapacity * sizeof (NVDIMM_LABEL));
  if (Namespace->Labels == NULL) {
    FreeNamespace (Namespace);
    return NULL;
  }
  InsertTailList (&mPmem.Namespaces, &Namespace->Link);
  return Namespace;
}

/**
  Free all resources occupied by a namespace.

  @param Namespace  The namespace to free.
**/
VOID
FreeNamespace (
  NVDIMM_NAMESPACE                 *Namespace
  )
{
  ASSERT (Namespace != NULL);
  if (Namespace->ControllerNameTable != NULL) {
    FreeUnicodeStringTable (Namespace->ControllerNameTable);
  }
  if (Namespace->DevicePath != NULL) {
    FreePool (Namespace->DevicePath);
  }
  if (Namespace->Labels != NULL) {
    FreePool (Namespace->Labels);
  }
  if (Namespace->BttHandle != NULL) {
    BttRelease (Namespace->BttHandle);
  }
  FreePool (Namespace);
}

/**
  Locate the NVDIMM using device handle.
  A new NVDIMM is created when unable to find and Create is TRUE.

  @param List         The NVDIMM list.
  @param DeviceHandle The NVDIMM device handle which uniquely identify a NVDIMM.
  @param Create       TRUE to create a new NVDIMM when unable to find.

  @return The found NVDIMM or a new one.
**/
NVDIMM *
LocateNvdimm (
  LIST_ENTRY                       *List,
  EFI_ACPI_6_0_NFIT_DEVICE_HANDLE  *DeviceHandle,
  BOOLEAN                          Create
  )
{
  LIST_ENTRY      *Link;
  NVDIMM          *Nvdimm;

  for (Link = GetFirstNode (List)
    ; !IsNull (List, Link)
    ; Link = GetNextNode (List, Link)
    ) {
    Nvdimm = NVDIMM_FROM_LINK (Link);
    if (CompareMem (&Nvdimm->DeviceHandle, DeviceHandle, sizeof (*DeviceHandle)) == 0) {
      return Nvdimm;
    }
  }

  if (!Create) {
    return NULL;
  }
  Nvdimm = AllocateZeroPool (sizeof (*Nvdimm));
  if (Nvdimm != NULL) {
    Nvdimm->PmRegion = AllocateZeroPool (
      sizeof (NVDIMM_REGION) *
      mPmem.NfitStrucCount[EFI_ACPI_6_0_NFIT_MEMORY_DEVICE_TO_SYSTEM_ADDRESS_RANGE_MAP_STRUCTURE_TYPE]
    );
    if (Nvdimm->PmRegion == NULL) {
      FreePool (Nvdimm);
      return NULL;
    }
    Nvdimm->Signature = NVDIMM_SIGNATURE;
    CopyMem (&Nvdimm->DeviceHandle, DeviceHandle, sizeof (EFI_ACPI_6_0_NFIT_DEVICE_HANDLE));
    InsertTailList (&mPmem.Nvdimms, &Nvdimm->Link);
  }
  return Nvdimm;
}

/**
  Free the resources occupied by a NVDIMM.

  @param Nvdimm The NVDIMM to free.
**/
VOID
FreeNvdimm (
  NVDIMM        *Nvdimm
  )
{
  ASSERT (Nvdimm != NULL);
  if (Nvdimm->LabelStorageData != NULL) {
    FreePool (Nvdimm->LabelStorageData);
  }

  if (Nvdimm->PmRegion != NULL) {
    FreePool (Nvdimm->PmRegion);
  }
  FreePool (Nvdimm);
}

/**
  Free the resources occupied by a list of NVDIMMs.

  @param List The NVDIMM list to free.
**/
VOID
FreeNvdimms (
  LIST_ENTRY    *List
  )
{
  LIST_ENTRY      *Link;
  NVDIMM          *Nvdimm;

  for (Link = GetFirstNode (List); !IsNull (List, Link); ) {
    Nvdimm = NVDIMM_FROM_LINK (Link);
    Link = RemoveEntryList (Link);
    FreeNvdimm (Nvdimm);
  }
}

/**
  Comparator of the NVDIMM label which is based on the device physical address.

  @param Left  The NVDIMM label to compare.
  @param Right The NVDIMM label to compare.

  @retval -1 Left < Right.
  @retval 0  Left == Right.
  @retval 1  Left > Right.
**/
INTN
EFIAPI
CompareLabelDpa (
  CONST NVDIMM_LABEL     *Left,
  CONST NVDIMM_LABEL     *Right
  )
{
  if (Left->Label->Dpa < Right->Label->Dpa) {
    return -1;
  } else if (Left->Label->Dpa == Right->Label->Dpa) {
    return 0;
  } else {
    return 1;
  }
}

/**
  Load the labels for all NVDIMMs identified by the handles array.

  @param Handles    NVDIMM handles array.
  @param HandleNum  Number of handles in the NVDIMM handles array.

  @retval EFI_SUCCESS All labels are loaded successfully.
**/
EFI_STATUS
LoadAllNvdimmLabels (
  IN EFI_HANDLE                  *Handles,
  IN UINTN                       HandleNum
  )
{
  UINTN                       Index;
  EFI_STATUS                  Status;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  NVDIMM                      *Nvdimm;
  ACPI_ADR_DEVICE_PATH        *AcpiAdr;


  for (Index = 0; Index < HandleNum; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    ASSERT_EFI_ERROR (Status);

    AcpiAdr = NULL;
    while (!IsDevicePathEnd (DevicePath)) {
      if ((DevicePathType (DevicePath) == ACPI_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == ACPI_ADR_DP) &&
        (DevicePathNodeLength (DevicePath) == sizeof (ACPI_ADR_DEVICE_PATH))
        ) {
        AcpiAdr = (ACPI_ADR_DEVICE_PATH *)DevicePath;
      }
      DevicePath = NextDevicePathNode (DevicePath);
    }

    //
    // ACPI_ADR node should be the last node before END.
    //
    ASSERT ((AcpiAdr != NULL) && (NextDevicePathNode (AcpiAdr) == DevicePath));

    Nvdimm = LocateNvdimm (&mPmem.Nvdimms, (EFI_ACPI_6_0_NFIT_DEVICE_HANDLE *)&AcpiAdr->ADR, FALSE);
    if (Nvdimm == NULL) {
      //
      // The NVDIMM isn't referenced in NFIT table.
      //
      DEBUG ((DEBUG_WARN, "NVDIMM[%08x] doesn't exist in NFIT table!\n", AcpiAdr->ADR));
      continue;
    }

    if (Nvdimm->Handle == NULL) {
      Nvdimm->Handle = Handles[Index];
      //
      // Read out all label information from the NVDIMM
      //
      Status = LoadNvdimmLabels (Nvdimm);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "NVDIMM[%08x] contains invalid label storage! Remove it!!\n", AcpiAdr->ADR));
        RemoveEntryList (&Nvdimm->Link);
        FreeNvdimm (Nvdimm);
      }
    } else {
      //
      // Label information has already been loaded by previous DriverBindingStart().
      //
      DEBUG ((DEBUG_INFO, "NVDIMM[%08x] label storage is already loaded!\n", AcpiAdr->ADR));
      ASSERT (Nvdimm->Handle == Handles[Index]);
    }
  }
  return EFI_SUCCESS;
}

/**
  Dump a label.

  @param Label The NVDIMM label to dump.
**/
VOID
DumpLabel (
  EFI_NVDIMM_LABEL       *Label
  )
{
  DEBUG ((DEBUG_INFO,
    "  Uuid/Name: %g/%a\n"
    "  Flags: %04x\n"
    "  NLabel/Position: %d/%d\n"
    "  SetCookie: %016lx\n",
    &Label->Uuid, Label->Name,
    Label->Flags,
    Label->NLabel, Label->Position,
    Label->SetCookie
    ));
  DEBUG ((DEBUG_INFO,
    "  LbaSize: %016x\n"
    "  Dpa/RawSize: %lx/%lx\n"
    "  Slot: %d\n"
    "  Alignment: %x\n",
    Label->LbaSize,
    Label->Dpa, Label->RawSize,
    Label->Slot,
    Label->Alignment
    ));
  DEBUG ((DEBUG_INFO,
    "  TypeGuid: %g\n"
    "  AddressAbstractionGuid: %g\n"
    "  Checksum: %016lx\n",
    &Label->TypeGuid,
    &Label->AddressAbstractionGuid,
    Label->Checksum
    ));
}

/**
  Dump a namespace.

  @param Namespace  The namespace to be dumped.
**/
VOID
DumpNamespace (
  IN NVDIMM_NAMESPACE  *Namespace
  )
{
  UINTN             Index;
  DEBUG ((DEBUG_INFO,
    "Namespace[%g] %a (%a):\n",
    &Namespace->Uuid,
    (Namespace->Type == NamespaceTypePmem) ? "PMEM" : "BLK",
    Namespace->ReadOnly ? "ro" : "rw"
    ));
  DEBUG ((DEBUG_INFO,
    "  Name: %a\n"
    "  BlockSize/TotalSize/RawSize: %x/%lx/%lx\n",
    Namespace->Name,
    Namespace->LbaSize, Namespace->TotalSize, Namespace->RawSize
    ));
  DEBUG ((DEBUG_INFO,
    "  AddressAbstractionGuid: %g\n"
    "  LabelCount/LabelCapacity: %d/%d\n"
    "  SetCookie: %016lx\n",
    &Namespace->AddressAbstractionGuid,
    Namespace->LabelCount, Namespace->LabelCapacity,
    Namespace->SetCookie
    ));

  for (Index = 0; Index < Namespace->LabelCount; Index++) {
    ASSERT (Namespace->Labels[Index].Nvdimm != NULL);
    DEBUG ((DEBUG_INFO, "  [%d/%d]Nvdimm[%08x]:\n",
      Index, Namespace->LabelCount,
      *(UINT32 *)&Namespace->Labels[Index].Nvdimm->DeviceHandle));
    DumpLabel (Namespace->Labels[Index].Label);
  }
}

NVDIMM_REGION *
LocateRegion (
  UINT64          Dpa,
  NVDIMM_REGION   *Regions,
  UINTN           RegionCount
)
{
  UINTN           RegionIndex;
  for (RegionIndex = 0; RegionIndex < RegionCount; RegionIndex++) {
    if ((Dpa >= Regions->Map->MemoryDevicePhysicalAddressRegionBase) &&
      (Dpa < Regions->Map->MemoryDevicePhysicalAddressRegionBase + Regions->Map->MemoryDeviceRegionSize)) {
      break;
    }
    Regions++;
  }
  if (RegionIndex == RegionCount) {
    return NULL;
  } else {
    return Regions;
  }
}

/**
  Enumerate all NVDIMM labels to create(assemble) the namespaces and populate the BlockIo for each namespace.

  @retval EFI_SUCCESS All NVDIMM labels are parsed successfully.
**/
EFI_STATUS
ParseNvdimmLabels (
  VOID
  )
{
  EFI_STATUS                       Status;
  RETURN_STATUS                    RStatus;
  UINTN                            Index;
  NVDIMM                           *Nvdimm;
  LIST_ENTRY                       *Link;
  NVDIMM_NAMESPACE                 *Namespace;
  NVDIMM_LABEL                     *Label;
  NVDIMM_REGION                    *Region;
  UINT64                           SetCookie;
  EFI_NVDIMM_LABEL_SET_COOKIE_INFO *CookieInfo;

  //
  // Enumerate all NVDIMMs to create(assemble) the namespaces.
  //
  for (Link = GetFirstNode (&mPmem.Nvdimms)
    ; !IsNull (&mPmem.Nvdimms, Link)
    ; Link = GetNextNode (&mPmem.Nvdimms, Link)
    ) {
    Nvdimm = NVDIMM_FROM_LINK (Link);

    for (Index = 0; Index < Nvdimm->LabelIndexBlock->NSlot; Index++) {

      //
      // Skip free slot.
      //
      if (IsBitSet (Index, Nvdimm->LabelIndexBlock->Free)) {
        continue;
      }
      DEBUG ((DEBUG_INFO, "Nvdimm[%08x] Label[%d]:\n", *(UINT32 *)&Nvdimm->DeviceHandle, Index));
      DumpLabel (&Nvdimm->Labels[Index]);

      //
      // Skip the invalid label.
      //
      if (!IsLabelValid (&Nvdimm->Labels[Index], (UINT32)Index)) {
        DEBUG ((
          DEBUG_ERROR, "ERROR: Nvdimm[%08x] Label[%d] is invalid! Ignore it!\n",
          *(UINT32 *)&Nvdimm->DeviceHandle, Index
          ));
        continue;
      }

      //
      // Skip the BLK label.
      //
      if (GetNamespaceType (Nvdimm->Labels[Index].Flags) == NamespaceTypeBlock) {
        DEBUG ((
          DEBUG_WARN, "WARN: Nvdimm[%08x] Label[%d] is BLK label, skipped!\n",
          *(UINT32 *)&Nvdimm->DeviceHandle, Index
          ));
        continue;
      }
      //
      // Find the pre-created namespace, or create one.
      //
      Namespace = LocateNamespace (&Nvdimm->Labels[Index], TRUE);
      if ((Namespace == NULL) || (Namespace->Handle != NULL)) {
        //
        // Skip when namespace creation fails, or is already populated.
        //
        continue;
      }
      DumpNamespace (Namespace);

      //
      // Every label should have consistent data.
      //
      if ((Namespace->ReadOnly != IsNamespaceReadOnly (Nvdimm->Labels[Index].Flags)) ||
        (CompareMem (Namespace->Name, Nvdimm->Labels[Index].Name, sizeof (Namespace->Name)) != 0) ||
        (Namespace->LbaSize != Nvdimm->Labels[Index].LbaSize) ||
        (Namespace->SetCookie != Nvdimm->Labels[Index].SetCookie) ||
        !CompareGuid (&Namespace->AddressAbstractionGuid, &Nvdimm->Labels[Index].AddressAbstractionGuid)
        ) {
        DEBUG ((DEBUG_ERROR, "Nvdimm[%08x] Label[%d] is not consistent to Namespace[%g]! Ignore it!\n",
          *(UINT32 *)&Nvdimm->DeviceHandle, Index, &Namespace->Uuid
          ));
        continue;
      }

      Region = LocateRegion (Nvdimm->Labels[Index].Dpa, Nvdimm->PmRegion, Nvdimm->PmRegionCount);
      if (Region == NULL) {
        DEBUG ((DEBUG_ERROR, "ERROR: Nvdimm[%08x] Label[%d] isn't within any region!\n", *(UINT32 *)&Nvdimm->DeviceHandle, Index));
        continue;
      }

      ASSERT (Namespace->Type == NamespaceTypePmem);
      if (Namespace->LabelCapacity != Nvdimm->Labels[Index].NLabel) {
        DEBUG ((DEBUG_ERROR, "Nvdimm[%08x] Label[%d] is not consistent to Namespace[%g]! Ignore it!\n",
          *(UINT32 *)&Nvdimm->DeviceHandle, Index, &Namespace->Uuid
          ));
        continue;
      }

      if (Nvdimm->Labels[Index].Position == 0) {
        if (Region->Map->RegionOffset != 0) {
          DEBUG ((DEBUG_ERROR,
            "Nvdimm[%0x8] Map Region Offset[%d] MUST == 0 AS the first NVDIMM! Ignore this label!\n",
            *(UINT32 *)&Nvdimm->DeviceHandle, Region->Map->RegionOffset
            ));
          continue;
        }

        //
        // Calculate the SPA base for the PM namespace.
        //
        RStatus = DeviceRegionOffsetToSpa (
          Nvdimm->Labels[Index].Dpa - Region->Map->MemoryDevicePhysicalAddressRegionBase,
          Region->Spa,
          Region->Map,
          Region->Interleave,
          &Namespace->PmSpaBase
        );
        if (RETURN_ERROR (RStatus)) {
          DEBUG ((DEBUG_ERROR, "Failed to calculate PmSpaBase for PMEM namespace! Ignore this label!\n"));
          continue;
        }
      }

      //
      // Handle duplicated labels in the same position.
      //
      Label = &Namespace->Labels[Nvdimm->Labels[Index].Position];
      if (Label->Label != NULL) {
        DEBUG ((DEBUG_INFO, "Duplicate label detected!!! Flags (Existing/New) = %x/%x\n",
          Label->Label->Flags, Nvdimm->Labels[Index].Flags));
        if ((Nvdimm->Labels[Index].Flags & EFI_NVDIMM_LABEL_FLAGS_UPDATING) == (Label->Label->Flags & EFI_NVDIMM_LABEL_FLAGS_UPDATING)) {
          //
          // Duplicate labels both with UPDATING set: Reject the entire namespace.
          //
          DEBUG ((
            DEBUG_INFO, "Nvdimm[%0x8] Label[%d] is duplicated. Remove namespace[%g:%a]!\n",
            *(UINT32 *)&Nvdimm->DeviceHandle, Index, &Namespace->Uuid, Namespace->Name
            ));
          RemoveEntryList (&Namespace->Link);
          FreeNamespace (Namespace);
          continue;
        }
        //
        // If UPDATING bit differs, use the one with UPDATING cleared.
        //
        if ((Nvdimm->Labels[Index].Flags & EFI_NVDIMM_LABEL_FLAGS_UPDATING) != 0) {
          DEBUG ((
            DEBUG_INFO, "Nvdimm[%0x8] Label[%d] is in updating state! Ignore it!\n",
            *(UINT32 *)&Nvdimm->DeviceHandle, Index
            ));
          continue;
        }
      }

      //
      // Shared code path for PMEM and BLOCK namespaces.
      // Record the label and NVDIMM where the label resides.
      //
      Label->Nvdimm = Nvdimm;
      Label->Region = Region;
      Label->Label = &Nvdimm->Labels[Index];
      Namespace->LabelCount++;
      Namespace->RawSize += Nvdimm->Labels[Index].RawSize;
    }
  }

  DEBUG ((DEBUG_INFO, "Validate namespaces and publish BlockIo for each of them.\n"));

  //
  // Enumerate all namespaces, to post validate then publish BlockIo for each of them.
  //
  for (Link = GetFirstNode (&mPmem.Namespaces)
    ; !IsNull (&mPmem.Namespaces, Link)
    ; ) {

    Namespace = NVDIMM_NAMESPACE_FROM_LINK (Link);
    //
    // Skip the populated namespace.
    //
    if (Namespace->Handle != NULL) {
      Link = GetNextNode (&mPmem.Namespaces, Link);
      continue;
    }
    DumpNamespace (Namespace);

    ASSERT (Namespace->Type == NamespaceTypePmem);
    if (Namespace->LabelCount != Namespace->LabelCapacity) {
      DEBUG ((
        DEBUG_ERROR, "Namespace[%g:%a] is incompleted. Remove it!\n",
        &Namespace->Uuid, Namespace->Name
        ));
      Link = RemoveEntryList (&Namespace->Link);
      FreeNamespace (Namespace);
      continue;
    }
    CookieInfo = AllocateZeroPool (sizeof (EFI_NVDIMM_LABEL_SET_COOKIE_MAP) * Namespace->LabelCount);
    if (CookieInfo == NULL) {
      DEBUG ((
        DEBUG_ERROR, "Failed to allocate buffer for CookieInfo! Remove the namespace[%g:%a]!\n",
        &Namespace->Uuid, Namespace->Name
        ));
      Link = RemoveEntryList (&Namespace->Link);
      FreeNamespace (Namespace);
      continue;
    }

    //
    // Check whether PMEM namespace is completed.
    //
    for (Index = 0; Index < Namespace->LabelCount; Index++) {
      Label = &Namespace->Labels[Index];
      ASSERT (Label->Nvdimm != NULL);
      ASSERT (Label->Region != NULL);
      if (Label->Region->Map->InterleaveWays != Namespace->LabelCount) {
        DEBUG ((DEBUG_INFO, "Namespace[%g:%a] InterleaveWays [%d] MUST == NLabel[%d]! Remove it!\n",
          &Namespace->Uuid, Namespace->Name,
          Label->Region->Map->InterleaveWays, Namespace->LabelCount
          ));
        break;
      }

      CookieInfo->Mapping[Index].RegionOffset = Label->Region->Map->RegionOffset;
      CookieInfo->Mapping[Index].SerialNumber = Label->Region->Control->SerialNumber;
      CookieInfo->Mapping[Index].VendorId = Label->Region->Control->VendorID;
      if ((Label->Region->Control->ValidFields & BIT0) == 0) {
        //
        // Ignore Manufacturing Location/Date fields when BIT0 is not set.
        //
        CookieInfo->Mapping[Index].ManufacturingDate = 0;
        CookieInfo->Mapping[Index].ManufacturingLocation = 0;
      } else {
        CookieInfo->Mapping[Index].ManufacturingDate = Label->Region->Control->ManufacturingDate;
        CookieInfo->Mapping[Index].ManufacturingLocation = Label->Region->Control->ManufacturingLocation;
      }
    }

    if (Index != Namespace->LabelCount) {
      Link = RemoveEntryList (&Namespace->Link);
      FreeNamespace (Namespace);
      FreePool (CookieInfo);
      continue;
    }

    SetCookie = CalculateFletcher64 (
      (UINT32 *)CookieInfo,
      sizeof (EFI_NVDIMM_LABEL_SET_COOKIE_MAP) * Namespace->LabelCount / sizeof (UINT32)
    );
    FreePool (CookieInfo);
#ifdef NT32
    Namespace->SetCookie = SetCookie;
#endif
    if (Namespace->SetCookie != SetCookie) {
      DEBUG ((DEBUG_ERROR, "Namespace[%g:%a] invalid SetCookie, Expected [%016x] != Actual[%016x]! Remove it!\n",
        &Namespace->Uuid, Namespace->Name,
        SetCookie, Namespace->SetCookie
        ));
      Link = RemoveEntryList (&Namespace->Link);
      FreeNamespace (Namespace);
      continue;
    }

    Namespace->TotalSize = Namespace->RawSize;
    if (CompareGuid (&Namespace->AddressAbstractionGuid, &gEfiBttAbstractionGuid)) {
      Status = BttLoad (
        &Namespace->BttHandle,
        &Namespace->Uuid, &Namespace->TotalSize, &Namespace->LbaSize,
        (BTT_RAW_ACCESS)NvdimmBlockIoReadWriteRawBytes, Namespace
      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to load BTT - %r!\n", Status));
#ifdef AUTO_CREATE_BTT
        Namespace->LbaSize = 512;
        Status = BttInitialize (
          &Namespace->BttHandle,
          &Namespace->Uuid, 256, Namespace->LbaSize, &Namespace->TotalSize,
          (BTT_RAW_ACCESS)NvdimmBlockIoReadWriteRawBytes, Namespace
        );
        DEBUG ((DEBUG_ERROR, "Initialize BTT - %r.\n", Status));
#endif
      }
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to load/initialize BTT - %r! Remove this namespace!\n", Status));
        Link = RemoveEntryList (&Namespace->Link);
        FreeNamespace (Namespace);
        continue;
      }
    }

    //
    // Initialize the ComponentName data
    //
    Status = InitializeComponentName (Namespace);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to initialize ComponentName(2) - %r! Remove this namespace!\n", Status));
      Link = RemoveEntryList (&Namespace->Link);
      FreeNamespace (Namespace);
      continue;
    }

    //
    // Construct the namespace device path
    // For PMEM, the device path is like: <ADR><ADR>...<ADR><NAMESPACE><END>
    // For NVDIMM_BLK_REGION, the device path is like: <ADR><NAMESPACE><END>
    // The device path is constructed in such a way so that next time when the device path is connected,
    // only the necessary NVDIMM label storage is accessed.
    //
    for (Index = 0; Index < Namespace->LabelCount; Index++) {
      CopyMem (
        &((ACPI_ADR_DEVICE_PATH *)Namespace->DevicePath)[Index],
        DevicePathFromHandle (Namespace->Labels[Index].Nvdimm->Handle),
        sizeof (ACPI_ADR_DEVICE_PATH)
      );

      if (Namespace->Type == NamespaceTypeBlock) {
        break;
      }
    }
    CopyGuid (&mNamespaceNodeTemplate.Uuid, &Namespace->Uuid);
    CopyMem (
      &((ACPI_ADR_DEVICE_PATH *)Namespace->DevicePath)[Index],
      &mNamespaceNodeTemplate,
      sizeof (NVDIMM_NAMESPACE_DEVICE_PATH)
    );
    SetDevicePathEndNode (
      (NVDIMM_NAMESPACE_DEVICE_PATH *)(&((ACPI_ADR_DEVICE_PATH *)Namespace->DevicePath)[Index]) + 1
    );
    //
    // Construct the BlockIo.
    //
    InitializeBlockIo (Namespace);


    Status = gBS->InstallMultipleProtocolInterfaces (
      &Namespace->Handle,
      &gEfiBlockIoProtocolGuid,    &Namespace->BlockIo,
      &gEfiDevicePathProtocolGuid, Namespace->DevicePath,
      NULL
    );
    ASSERT_EFI_ERROR (Status);

    OpenNvdimmLabelsByChild (Namespace);
    Link = GetNextNode (&mPmem.Namespaces, Link);
  }

  return EFI_SUCCESS;
}