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

NVDIMM_NAMESPACE_FULL_DEVICE_PATH  mNamespaceDevicePathTemplate = {
  {
    {
      MESSAGING_DEVICE_PATH,
      MSG_NVDIMM_NAMESPACE_DP,
      {sizeof (NVDIMM_NAMESPACE_DEVICE_PATH), 0}
    }
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {sizeof (EFI_DEVICE_PATH_PROTOCOL), 0}
  }
};

BOOLEAN
IsBitSet (
  UINTN   Index,
  UINT8   *Bytes
)
{
  return (BOOLEAN)((Bytes[Index / 8] & (1 << (Index % 8))) != 0);
}

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

  return ValidateFletcher64 ((UINT32 *)LabelIndexBlock, LabelIndexBlock->MySize, &LabelIndexBlock->Checksum);
}

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

  return ValidateFletcher64 ((UINT32 *)Label, sizeof (*Label), &Label->Checksum);
}

// TODO: sanity check
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
    DEBUG ((DEBUG_WARN, "Label Index #0 is not valid!"));
  }

  if (LabelIndexBlock[1] == NULL) {
    Nvdimm->LabelIndexBlock = LabelIndexBlock[0];
    DEBUG ((DEBUG_WARN, " Label Index #1 is not valid!"));

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
      DEBUG ((DEBUG_WARN, "Label Index x-reference check fails!"));
      FreePool (Nvdimm->LabelStorageData);
      return EFI_INVALID_PARAMETER;
    }

    DEBUG ((DEBUG_INFO, "Label Index sequence number = #0(%x) / #1(%x).",
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
  Nvdimm->Labels = (EFI_NVDIMM_LABEL *)&Nvdimm->LabelStorageData[Nvdimm->LabelIndexBlock->LabelOff];
  return EFI_SUCCESS;
}

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

  Namespace = NULL;
  if (Create) {
    Namespace = AllocateZeroPool (sizeof (*Namespace));
    if (Namespace != NULL) {
      Namespace->Signature = NVDIMM_NAMESPACE_SIGNATURE;
      Namespace->Type      = GetNamespaceType (Label->Flags);
      Namespace->ReadOnly  = IsNamespaceReadOnly (Label->Flags);
      Namespace->BlockSize = (Namespace->Type == NamespaceTypeBlock) ? (UINT32)Label->LbaSize : 512;
      Namespace->SetCookie = Label->SetCookie;
      CopyGuid (&Namespace->Uuid, &Label->Uuid);
      CopyMem  (Namespace->Name, Label->Name, sizeof (Namespace->Name));
      CopyGuid (&Namespace->AddressAbstractionGuid, &Label->AddressAbstractionGuid);
      if (Namespace->Type == NamespaceTypeBlock) {
        //
        // Number of labels is unknown until assembling is completed for Local (Block) namespaces.
        //
        Namespace->LabelCapacity = BLOCK_NAMESPACE_INIT_LABEL_COUNT;
      } else {
        //
        // Number of labels equals to NLabel for PMEM namespaces.
        //
        Namespace->LabelCapacity = Label->NLabel;
      }
      Namespace->Labels = AllocateZeroPool (Namespace->LabelCapacity * sizeof (NVDIMM_LABEL));
      if (Namespace->Labels == NULL) {
        FreePool (Namespace);
        Namespace = NULL;
      } else {
        InsertTailList (&mPmem.Namespaces, &Namespace->Link);
      }
    }
  }
  return Namespace;
}

VOID
FreeNamespace (
  NVDIMM_NAMESPACE                 *Namespace
)
{
  ASSERT (Namespace != NULL);
  if (Namespace->Labels != NULL) {
    FreePool (Namespace->Labels);
  }
  BttRelease (Namespace->BttHandle);
  FreePool (Namespace);
}

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

    //
    // Read out all label information from the NVDIMM
    //
    Nvdimm->Signature = NVDIMM_SIGNATURE;
    CopyMem (&Nvdimm->DeviceHandle, DeviceHandle, sizeof (EFI_ACPI_6_0_NFIT_DEVICE_HANDLE));
  }
  return Nvdimm;
}

VOID
FreeNvdimm (
  NVDIMM        *Nvdimm
)
{
  ASSERT (Nvdimm != NULL);
  if (Nvdimm->LabelStorageData != NULL) {
    FreePool (Nvdimm->LabelStorageData);
  }

  if (Nvdimm->Blk.DataWindowAperture != NULL) {
    FreePool (Nvdimm->Blk.DataWindowAperture);
  }
  FreePool (Nvdimm);
}

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

EFI_STATUS
LoadAllNvdimmLabels (
  EFI_HANDLE                  *Handles,
  UINTN                       HandleNum
)
{
  UINTN                       Index;
  EFI_STATUS                  Status;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  NVDIMM                      *Nvdimm;
  ACPI_ADR_DEVICE_PATH        *AcpiAdr;


  for (Index = 0; Index < HandleNum; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    if (EFI_ERROR (Status)) {
      continue;
    }

    AcpiAdr = NULL;
    while (!IsDevicePathEnd (DevicePath)) {
      if ((DevicePathType (DevicePath) == ACPI_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == ACPI_ADR_DP) &&
        (DevicePathNodeLength (DevicePath) == sizeof (ACPI_ADR_DEVICE_PATH))
        ) {
        AcpiAdr = (ACPI_ADR_DEVICE_PATH *)DevicePath;
        break;
      }
    }

    if (AcpiAdr == NULL) {
      //
      // Cannot find the ACPI_ADR device path node.
      //
      continue;
    }

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

VOID
DumpLabel (
  EFI_NVDIMM_LABEL       *Label
)
{
  DEBUG ((DEBUG_INFO,
    "  Uuid/Name: %g/%a\n"
    "  Flags: %04x\n"
    "  NLabel/Position: %d/%d\n"
    "  SetCookie: %016x\n"
    "  LbaSize: %016x\n"
    "  Dpa/RawSize: %016x/%016x\n"
    "  Slot: %d\n"
    "  Alignment: %02x\n"
    "  TypeGuid: %g\n"
    "  AddressAbstractionGuid: %g\n"
    "  Checksum: %016x\n",
    &Label->Uuid, Label->Name,
    Label->Flags,
    Label->NLabel, Label->Position,
    Label->SetCookie,
    Label->LbaSize,
    Label->Dpa, Label->RawSize,
    Label->Slot,
    Label->Alignment,
    &Label->TypeGuid,
    &Label->AddressAbstractionGuid,
    Label->Checksum
    ));
}

VOID
DumpNamespace (
  NVDIMM_NAMESPACE  *Namespace
)
{
  UINTN             Index;
  DEBUG ((DEBUG_INFO,
    "Namespace[%g]:\n"
    "  Type: %d\n"
    "  ReadOnly: %d\n"
    "  Name: %a\n"
    "  AddressAbstractionGuid: %g\n"
    "  LabelCount/LabelCapacity: %d/%d\n"
    "  SetCookie: %016x"
    "  BlockSize/TotalSize: %016x/%016x",
    &Namespace->Uuid,
    Namespace->Type,
    Namespace->ReadOnly,
    Namespace->Name,
    &Namespace->AddressAbstractionGuid,
    Namespace->LabelCount, Namespace->LabelCapacity,
    Namespace->SetCookie,
    Namespace->BlockSize, Namespace->TotalSize
    ));

  for (Index = 0; Index < Namespace->LabelCount; Index++) {
    ASSERT (Namespace->Labels[Index].Nvdimm != NULL);
    DEBUG ((DEBUG_INFO, "  [%d/%d]Nvdimm[%08x]:\n",
      Index, Namespace->LabelCount,
      *(UINT32 *)&Namespace->Labels[Index].Nvdimm->DeviceHandle));
    DumpLabel (Namespace->Labels[Index].Label);
  }
}

EFI_STATUS
ParseNvdimmLabels (
  EFI_HANDLE                  *Handles,
  UINTN                       HandleNum
)
{
  EFI_STATUS                       Status;
  RETURN_STATUS                    RStatus;
  UINTN                            Index;
  NVDIMM                           *Nvdimm;
  LIST_ENTRY                       *Link;
  NVDIMM_NAMESPACE                 *Namespace;
  NVDIMM_LABEL                     *Label;
  UINT64                           SetCookie;
  EFI_NVDIMM_LABEL_SET_COOKIE_MAP  CookieMap;
  EFI_NVDIMM_LABEL_SET_COOKIE_INFO *CookieInfo;

  if (!mPmem.Initialized) {
    //
    // Parse ACPI NFIT table and create all NVDIMM instances referenced in ACPI NFIT table.
    // It may create more than HandleNum NVDIMM instances.
    //
    Status = ParseNfit ();
    if (EFI_ERROR (Status)) {
      return Status;
    }
    mPmem.Initialized = TRUE;
  }

  //
  // Load the NVDIMM Labels
  //
  Status = LoadAllNvdimmLabels (Handles, HandleNum);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Enumerate all NVDIMMs to assemble/create the namespaces.
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
      if (!IsLabelValid (&Nvdimm->Labels[Index], (UINT32)Index)) {
        DEBUG ((DEBUG_ERROR, "Nvdimm[%08x] Label[%d] is invalid! Ignore it!\n", *(UINT32 *)&Nvdimm->DeviceHandle, Index));
        continue;
      }

      //
      // Find the pre-created namespace, or create one.
      //
      Namespace = LocateNamespace (&Nvdimm->Labels[Index], TRUE);
      if (Namespace == NULL) {
        continue;
      }
      DumpNamespace (Namespace);

      //
      // Every label should have consistent data.
      //
      if ((Namespace->ReadOnly != IsNamespaceReadOnly (Nvdimm->Labels[Index].Flags)) ||
        (CompareMem (Namespace->Name, Nvdimm->Labels[Index].Name, sizeof (Namespace->Name)) != 0) ||
        (Namespace->BlockSize != Nvdimm->Labels[Index].LbaSize) ||
        (Namespace->SetCookie != Nvdimm->Labels[Index].SetCookie) ||
        !CompareGuid (&Namespace->AddressAbstractionGuid, &Nvdimm->Labels[Index].AddressAbstractionGuid)
        ) {
        DEBUG ((DEBUG_ERROR, "Nvdimm[%08x] Label[%d] is not consistent to Namespace[%g]! Ignore it!\n",
          *(UINT32 *)&Nvdimm->DeviceHandle, Index, &Namespace->Uuid
          ));
        continue;
      }

      if (Namespace->Type == NamespaceTypePmem) {
        if (Namespace->LabelCapacity != Nvdimm->Labels[Index].NLabel) {
          DEBUG ((DEBUG_ERROR, "Nvdimm[%08x] Label[%d] is not consistent to Namespace[%g]! Ignore it!\n",
            *(UINT32 *)&Nvdimm->DeviceHandle, Index, &Namespace->Uuid
            ));
          continue;
        }

        if (Nvdimm->Labels[Index].Dpa < Nvdimm->PmMap->MemoryDevicePhysicalAddressRegionBase) {
          DEBUG ((DEBUG_ERROR,
            "Nvdimm[%0x8] Label[%d] DPA[%d] MUST >= MAP Region Base[%d]! Ignore it!\n",
            *(UINT32 *)&Nvdimm->DeviceHandle, Index,
            Nvdimm->Labels[Index].Dpa, Nvdimm->PmMap->MemoryDevicePhysicalAddressRegionBase
            ));
          continue;
        }

        if (Index == 0) {
          if (Nvdimm->PmMap->RegionOffset != 0) {
            DEBUG ((DEBUG_ERROR,
              "Nvdimm[%0x8] Map Region Offset[%d] MUST == 0 AS the first NVDIMM! Ignore this label!\n",
              *(UINT32 *)&Nvdimm->DeviceHandle, Nvdimm->PmMap->RegionOffset
              ));
            continue;
          }

          //
          // Calculate the SPA base for the PM namespace.
          //
          RStatus = DeviceRegionOffsetToSpa (
            Nvdimm->Labels[0].Dpa - Nvdimm->PmMap->MemoryDevicePhysicalAddressRegionBase,
            Nvdimm->PmSpa,
            Nvdimm->PmMap,
            Nvdimm->PmInterleave,
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
        if (Label->Nvdimm != NULL) {
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
      } else {
        //
        // Collect all Block(Local) labels
        //
        if (Namespace->LabelCount == Namespace->LabelCapacity) {
          Label = ReallocatePool (
            Namespace->LabelCapacity * sizeof (NVDIMM_LABEL),
            (Namespace->LabelCapacity + BLOCK_NAMESPACE_INIT_LABEL_COUNT) * sizeof (NVDIMM_LABEL),
            Namespace->Labels
          );
          if (Label == NULL) {
            DEBUG ((
              DEBUG_ERROR, "Failed to enlarge label space[count = %d]. Remove namespace[%g:%a]!\n",
              Namespace->LabelCapacity + BLOCK_NAMESPACE_INIT_LABEL_COUNT,
              &Namespace->Uuid, Namespace->Name
              ));
            RemoveEntryList (&Namespace->Link);
            FreeNamespace (Namespace);
            continue;
          }
          Namespace->Labels         = Label;
          Namespace->LabelCapacity += BLOCK_NAMESPACE_INIT_LABEL_COUNT;
        }
        Label = &Namespace->Labels[Namespace->LabelCount];
      }

      //
      // Shared code path for PMEM and BLOCK namespaces.
      // Record the label and NVDIMM where the label resides.
      //
      Label->Nvdimm = Nvdimm;
      Label->Label  = &Nvdimm->Labels[Index];
      Namespace->LabelCount++;
      Namespace->TotalSize += Nvdimm->Labels[Index].RawSize;
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
    DumpNamespace (Namespace);

    if (Namespace->Type == NamespaceTypePmem) {
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
        if (Label->Nvdimm->PmMap->InterleaveWays != Namespace->LabelCount) {
          DEBUG ((DEBUG_INFO, "Namespace[%g:%a] InterleaveWays [%d] MUST == NLabel[%d]! Remove it!\n",
            &Namespace->Uuid, Namespace->Name,
            Label->Nvdimm->PmMap->InterleaveWays, Namespace->LabelCount
            ));
          break;
        }

        CookieInfo->Mapping[Index].RegionOffset          = Label->Nvdimm->PmMap->RegionOffset;
        CookieInfo->Mapping[Index].SerialNumber          = Label->Nvdimm->PmControl->SerialNumber;
        CookieInfo->Mapping[Index].VendorId              = Label->Nvdimm->PmControl->VendorID;
        CookieInfo->Mapping[Index].ManufacturingDate     = Label->Nvdimm->PmControl->ManufacturingDate;
        CookieInfo->Mapping[Index].ManufacturingLocation = Label->Nvdimm->PmControl->ManufacturingLocation;
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
    } else {
      //
      // Block namespaces:
      // Sort the labels by Dpa, then check the NLabel/Position value in each label.
      //
      PerformQuickSort (Namespace->Labels, Namespace->LabelCount, sizeof (NVDIMM_LABEL), CompareLabelDpa);
      for (Index = 0; Index < Namespace->LabelCount; Index++) {
        Label = &Namespace->Labels[Index];

        if (Index == 0) {
          if ((Label->Label->NLabel != Namespace->LabelCount) ||
            (Label->Label->Position != 0)) {
            DEBUG ((DEBUG_INFO, "Namespace[%g:%a] is incomplete or contains invalid first label! Remove it!\n",
              &Namespace->Uuid, Namespace->Name
              ));
            break;
          }
        } else {
          if ((Label->Label->NLabel != 0xFF) ||
            (Label->Label->Position != 0xFF)) {
            DEBUG ((DEBUG_INFO, "Namespace[%g:%a] contains invalid non-first label! Remove it!\n",
              &Namespace->Uuid, Namespace->Name,
              Label->Label->NLabel, Label->Label->Position
              ));
            break;
          }
          if (Label->Nvdimm != Namespace->Labels[0].Nvdimm) {
            DEBUG ((DEBUG_INFO, "Namespace[%g:%a] contains non-local labels! Remove it!\n",
              &Namespace->Uuid, Namespace->Name
              ));
            break;
          }
        }
      }
      if (Index != Namespace->LabelCount) {
        Link = RemoveEntryList (&Namespace->Link);
        FreeNamespace (Namespace);
        continue;
      }
      Label = &Namespace->Labels[0];
      CookieMap.RegionOffset          = Label->Nvdimm->Blk.ControlMap->RegionOffset;
      CookieMap.SerialNumber          = Label->Nvdimm->Blk.Control->SerialNumber;
      CookieMap.VendorId              = Label->Nvdimm->Blk.Control->VendorID;
      CookieMap.ManufacturingDate     = Label->Nvdimm->Blk.Control->ManufacturingDate;
      CookieMap.ManufacturingLocation = Label->Nvdimm->Blk.Control->ManufacturingLocation;
      SetCookie = CalculateFletcher64 ((UINT32 *)&CookieMap, sizeof (CookieMap) / sizeof (UINT32));
    }

    if (Namespace->SetCookie != SetCookie) {
      DEBUG ((DEBUG_ERROR, "Namespace[%g:%a] invalid SetCookie, Expected [%016x] != Actual[%016x]! Remove it!\n",
        &Namespace->Uuid, Namespace->Name,
        SetCookie, Namespace->SetCookie
        ));
      Link = RemoveEntryList (&Namespace->Link);
      FreeNamespace (Namespace);
      continue;
    }

    if (CompareGuid (&Namespace->AddressAbstractionGuid, &gEfiBttAbstractionGuid)) {
      Status = BttLoad (
        &Namespace->BttHandle,
        &Namespace->Uuid, &Namespace->TotalSize, &Namespace->BlockSize,
        NvdimmBlockIoReadWriteBytes, Namespace
      );
      if (EFI_ERROR (Status)) {
#ifdef AUTO_CREATE_BTT
        DEBUG ((DEBUG_WARN, "Failed to load BTT! Initialize BTT!\n"));
        Status = BttInitialize (
          &Namespace->BttHandle,
          &Namespace->Uuid, 256, 512, &Namespace->TotalSize, &Namespace->BlockSize,
          NvdimmBlockIoReadWriteBytes, Namespace
        );
#else
        DEBUG ((DEBUG_ERROR, "Failed to load BTT! Remove this namespace!\n"));
        Link = RemoveEntryList (&Namespace->Link);
        FreeNamespace (Namespace);
        continue;
#endif
      }
    }

    InitializeBlockIo (Namespace);
    ASSERT (sizeof (Namespace->DevicePath) == sizeof (mNamespaceDevicePathTemplate));
    CopyMem (&Namespace->DevicePath, &mNamespaceDevicePathTemplate, sizeof (mNamespaceDevicePathTemplate));
    CopyGuid (&Namespace->DevicePath.NvdimmNamespace.Uuid, &Namespace->Uuid);

    Status = gBS->InstallMultipleProtocolInterfaces (
      &Namespace->Handle,
      &gEfiBlockIoProtocolGuid, &Namespace->BlockIo,
      &gEfiDevicePathProtocolGuid, &Namespace->DevicePath,
      NULL
    );
    ASSERT_EFI_ERROR (Status);

    OpenNvdimmLabelsByChild (Namespace);
    Link = GetNextNode (&mPmem.Namespaces, Link);
  }
}