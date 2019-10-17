/** @file
  This file declares EFI PCI Express Platform Protocol that provide the interface between
  the PCI bus driver/PCI Host Bridge Resource Allocation driver and a platform-specific
  driver to describe the unique features of a platform.
  This protocol is optional.

Copyright (c) 2019 - 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent


**/

#ifndef _PCI_EXPRESS_PLATFORM_H_
#define _PCI_EXPRESS_PLATFORM_H_

///
/// This file must be included because the EFI_PCI_EXPRESS_PLATFORM_PROTOCOL uses
/// EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE.
///
#include <Protocol/PciHostBridgeResourceAllocation.h>

///
/// Global ID for the  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL.
///
#define  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL_GUID \
  { \
    0x787b0367, 0xa945, 0x4d60, {0x8d, 0x34, 0xb9, 0xd1, 0x88, 0xd2, 0xd0, 0xb6} \
  }


///
/// Forward declaration for EFI_PCI_EXPRESS_PLATFORM_PROTOCOL.
///
typedef struct _EFI_PCI_EXPRESS_PLATFORM_PROTOCOL  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL;

///
/// Related Definitions for EFI_PCI_EXPRESS_DEVICE_POLICY
///

///
/// EFI encoding used in notification to the platform, for any of the PCI Express
/// capabilities feature state, to indicate that it is not a supported feature,
/// or its present state is unknown
///
#define EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO            0xFF
#define EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE  0xFE

///
/// Following are the data types for EFI_PCI_EXPRESS_DEVICE_POLICY
/// each for the PCI standard feature and its corresponding bitmask
/// representing the valid combinations of PCI attributes
///

///
/// PCI Express device policy for PCIe device Maximum Payload Size.
///[DONE]
/// Refer to PCI Express Base Specification 4 (chapter 7.5.3.4) on what
/// value can be used.
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO, then the Maximum
/// Payload Size (Device Control Register BIT[5:7]) is set to the bigger value
/// between this device's capability and settings of other devices in the same
/// heirarchy.
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high
/// 5 bits are not 0, then this feature is skipped for the device but the
/// device capability is still be considerred when calculating the aligned
/// Maximum Payload Size.
/// Otherwise the policy value is written to Device Control Register BIT[5:7].
///

///
/// PCI Express device policy for PCIe Maximum Read Request Size.
///[DONE]
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO, then Device Control
/// Register BIT[12:14] is set to the same as Device Control Register BIT[5:7].
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high
/// 5 bits are not 0, then the feature is skipped.
/// Otherwise the policy value is written to Device Control Register BIT[12:14].
///

/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
/// EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high 7 bits are not 0,
/// the feature is skipped.
/// Otherwise the policy value is written to Device Control Register BIT[4].
///
/// PCI Express device policy for PCIe Extended Tag.
/// Refer to PCI Express Base Specification 4 (chapter 7.5.3.4), on how to
/// translate the below EFI encodings as per the PCI hardware terminology.
/// If this data member value is 0 then there is no platform policy to
/// override, this feature would be enabled as per its PCI specification based
/// on the device capabilities.
///
#define EFI_PCI_EXPRESS_EXTENDED_TAG_AUTO   0x00  // No request for override
#define EFI_PCI_EXPRESS_EXTENDED_TAG_5BIT   0x01  // set to default 5-bit
#define EFI_PCI_EXPRESS_EXTENDED_TAG_8BIT   0x02  // set to 8-bit if applicable
#define EFI_PCI_EXPRESS_EXTENDED_TAG_10BIT  0x03  // set to 10-bit if applicable

///
/// PCI Express device policy for PCIe Link's Active State Power Management.
/// Refer to PCI Express Base Specification 4 (chapter 7.5.3.7), on how to
/// translate the below EFI encodings as per the PCI hardware terminology.
/// If this data member value is 0 then there is no platform policy to
/// override, this feature would be enabled as per its PCI specification based
/// on the device capabilities.
///
#define EFI_PCI_EXPRESS_ASPM_AUTO           0x00  // No request for override
#define EFI_PCI_EXPRESS_ASPM_DISABLE        0x01  // set to default disable state
#define EFI_PCI_EXPRESS_ASPM_L0s_SUPPORT    0x02  // set to L0s state
#define EFI_PCI_EXPRESS_ASPM_L1_SUPPORT     0x03  // set to L1 state
#define EFI_PCI_EXPRESS_ASPM_L0S_L1_SUPPORT 0x04  // set to L0s and L1 state

///
/// PCI Express device policy for PCIe Device's Relaxed Ordering.
///[DONE]
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
/// EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high 7 bits are not 0,
/// the feature is skipped.
/// Otherwise the policy value is written to Device Control Register BIT[4].
///

///
/// PCI Express device policy for PCIe Device's No-Snoop.
///[DONE]
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
/// EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high 7 bits are not 0,
/// the feature is skipped.
/// Otherwise the policy value is written to Device Control Register BIT[11].
///

///
/// PCI Express device policy for PCIe Link's Clock Configuration.
/// Refer to PCI Express Base Specification 4 (chapter 7.5.3.7), on how to
/// translate the below EFI encodings as per the PCI hardware terminology.
/// If this data member value is 0 then there is no platform policy to
/// override, this feature would be enabled as per its PCI specification based
/// on the device capabilities.
///
#define EFI_PCI_EXPRESS_COMMON_CLOCK_CONFIGURATION_AUTO   0x00   // No request for override
#define EFI_PCI_EXPRESS_COMMON_CLOCK_CONFIGURATION_ASYNC  0x01   // set to default asynchronous clock
#define EFI_PCI_EXPRESS_COMMON_CLOCK_CONFIGURATION_SYNC   0x02   // set to common clock

///
/// PCI Express device policy for PCIe Link's Extended Synch.
/// Refer to PCI Express Base Specification 4 (chapter 7.5.3.7), on how to
/// translate the below EFI encodings as per the PCI hardware terminology.
/// If this data member value is 0 then there is no platform policy to
/// override, this feature would be enabled as per its PCI specification based
/// on the device capabilities.
///
#define EFI_PCI_EXPRESS_EXTENDED_SYNC_AUTO    0x00  // No request for override
#define EFI_PCI_EXPRESS_EXTENDED_SYNC_DISABLE 0x01  // set to default disable state
#define EFI_PCI_EXPRESS_EXTENDED_SYNC_ENABLE  0x02  // set to enable state

///
/// PCI Express device policy for PCIe Device's AtomicOp.
///[DONE]
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
/// EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high 6 bits are not 0,
/// then the feature is skipped for the certain device.
/// Otherwise the policy value is written to Device Control 2 Register BIT[6:7]
/// if it's supported by the device per Device Capabilities 2 Register.
///

///
/// PCI Express device policy for PCIe Device's LTR Mechanism.
///[DONE]
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
/// EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high 7 bits are not 0
/// for the bridge, then the feature is only enabled when any devices under
/// the bridge request to enable the feature and skipped otherwise.
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO,
/// EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE or its high 7 bits are not 0
/// for the end device, the feature is skipped.
/// Otherwise the policy value is written to Device Control 2 Register BIT[10] if
/// it's supported by the device per Device Capabilities 2 Register.
///

///
/// PCI Express device policy for PCIe Precision Time Measurement.
/// Refer to PCI Express Base Specification 4 (chapter 7.5.3.16), on how to
/// translate the below EFI encodings as per the PCI hardware terminology.
/// If this data member value is 0 then there is no platform policy to
/// override, this feature would be enabled as per its PCI specification based
/// on the device capabilities.
///
#define EFI_PCI_EXPRESS_PTM_AUTO      0x00  // No request for override
#define EFI_PCI_EXPRESS_PTM_DISABLE   0x01  // set to default disable state
#define EFI_PCI_EXPRESS_PTM_ENABLE    0x02  // set to enable state only
#define EFI_PCI_EXPRESS_PTM_ROOT_SEL  0x02  // set to root select & enable ///TODO

///
/// PCI Express device policy for PCIe Device's Completion Timeout.
///[DONE]
/// If policy value is EFI_PCI_EXPRESS_DEVICE_POLICY_AUTO or
/// EFI_PCI_EXPRESS_DEVICE_POLICY_NOT_APPLICABLE, then the feature is skipped.
/// Otherwise, the policy value is written to Device Control 2 Register BIT[0:4]
/// if it's supported by the device per Device Capabilities 2 Register.
///

///
/// PCI device platform policy for PCIe Link's Clock Power Management.
/// Refer to PCI Express Base Specification 5 (chapter 7.5.3.7), on how to
/// translate the below EFI encodings as per the PCI hardware terminology.
/// If this data member value is 0 then there is no platform policy to
/// override, this feature would be enabled as per its PCI specification based
/// on the device capabilities.
///
#define EFI_PCI_EXPRESS_CLOCK_POWER_MANAGEMENT_AUTO    0x00  // No request for override
#define EFI_PCI_EXPRESS_CLOCK_POWER_MANAGEMENT_DISABLE 0x01  // set to default disable state
#define EFI_PCI_EXPRESS_CLOCK_POWER_MANAGEMENT_ENABLE  0x02  // set to enable state

///
/// PCI device platform policy for PCIe Link's L1 PM Substates.
/// Refer to PCI Express Base Specification 5 (chapter 7.8.3.3), on how to
/// translate the below EFI encodings as per the PCI hardware terminology.
/// If the data member "Override" value is 0 than there is no platform policy to
/// override, other data members must be ignored. If 1 then other data
/// members will be used as per the device capabilities.
/// The platform is expected to return any combination from the four L1 PM
/// Substates.
///

typedef struct {
  //
  // set to indicate the platform request to override the L1 PM Substates using
  // the other data member bit fields; bit clear means no request from platform
  // to override the L1 PM Substates for the device. Ignored when passed as input
  // parameters.
  //
  UINT8 Override:1;
  //
  // set to enable the PCI-PM L1.2 state; bit clear to force default disable state
  //
  UINT8 PciPmL12:1;
  //
  // set to enable the PCI-PM L1.1 state; bit clear to force default disable state
  //
  UINT8 PciPmL11:1;
  //
  // set to enable the ASPM L1.2 state; bit clear to force default disable state
  //
  UINT8 AspmL12:1;
  //
  // set to enable the ASPM L1.1 state; bit clear to force default disable state
  //
  UINT8 AspmL11:1;
  UINT8 Reserved:3;
} EFI_PCI_EXPRESS_L1PM_SUBSTATES;

typedef struct {
  UINT8                          MaxPayloadSize;//DONE
  UINT8                          MaxReadRequestSize;//DONE
  UINT8                          ExtendedTag;
  UINT8                          RelaxedOrdering;//DONE
  UINT8                          NoSnoop;//DONE
  UINT8                          AspmControl;
  UINT8                          CommonClockConfiguration;
  UINT8                          ExtendedSynch;
  UINT8                          AtomicOp;//DONE
  UINT8                          Ltr;//DONE
  UINT8                          Ptm;
  UINT8                          CompletionTimeout;//DONE
  UINT8                          ClockPowerManagement;
  EFI_PCI_EXPRESS_L1PM_SUBSTATES L1PmSubstates;
} EFI_PCI_EXPRESS_DEVICE_POLICY;

///
/// The EFI_PCI_EXPRESS_DEVICE_STATE is an alias of the data type of
/// EFI_PCI_EXPRESS_DEVICE_POLICY, used in notifying the platform about the
/// PCI Express features device configured states, through the NotifyDeviceState
/// interface method.
/// The EFI encoded macros like EFI_PCI_EXPRESS_*_AUTO, with the value 0 will not
/// be used to report the PCI feature definite state; similarly for the data type
/// of DeviceCtl2AtomicOp and L1PMSubstates, its data member "Override" will not
/// be used. For any of the device's PCI features that are not supported, or its
/// state is unknown, it will be returned as EFI_PCI_EXPRESS_NOT_APPLICABLE. ///TODO: How about the two structures?
///
typedef EFI_PCI_EXPRESS_DEVICE_POLICY EFI_PCI_EXPRESS_DEVICE_STATE;

/**
  Interface to retrieve the PCI device-specific platform policies to override
  the PCI Express feature capabilities, of an PCI device.

  The consumer driver(s), like PCI Bus driver and PCI Host Bridge Resource Allocation
  Protocol drivers; can call this member function to retrieve the platform policies
  specific to PCI device, related to its PCI Express capabilities. The producer of
  this protocol is platform whom shall provide the device-specific policies to
  override its PCIe features.

  The GetDevicePolicy() member function is meant to return data about other PCI
  Express features which would be supported by the PCI Bus driver in future;
  like for example the MPS, MRRS, Extended Tag, ASPM, etc. The details about
  this PCI features can be obtained from the PCI Express Base Specification (Rev.4
  or 5). The EFI encodings for these features are defined in the EFI_PCI_EXPRESS
  _PLATFORM_POLICY, see the Related Definition section for this. This member function
  will use the associated EFI handle of the root bridge instance and the PCI address
  of type EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS to determine the physical
  PCI device within the chipset, to return its device-specific platform policies.
  It is caller's responsibility to allocate the buffer and pass its pointer of
  type EFI_PCI_EXPRESS_DEVICE_POLICY to this member function, to get its device
  -specific policy data.
  The caller is required to initialize the input buffer with either of two values:-
  1.  EFI_PCI_EXPRESS_NOT_APPLICABLE: for those PCI Express features which are not
      supported by the calling driver
  2.  EFI_PCI_EXPRESS_*_AUTO: for those PCI Express features which are supported
      by the calling driver
  In order to skip any PCI Express feature for any particular PCI device, this
  interface is expected to return its hardware default state as defined by the
  PCI Express Base Specification.

  @param[in]      This          Pointer to the  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL instance.
  @param[in]      RootBridge    EFI handle of associated root bridge to the PCI device
  @param[in]      PciAddress    The address of the PCI device on the PCI bus.
                                This address can be passed to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
                                member functions to access the PCI configuration space of the
                                device. See UEFI 2.1 Specification for the definition
                                of EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS.
  @param[in]      Size          The size, in bytes, of the input buffer, in next parameter.
  @param[in,out]  PciExPolicy   The pointer to platform policy with respect to other
                                PCI features like, the MPS, MRRS, etc. Type
                                EFI_PCI_EXPRESS_DEVICE_POLICY is defined above.


  @retval EFI_SUCCESS            The function completed successfully, may returns
                                 platform policy data for the given PCI component
  @retval EFI_UNSUPPORTED        PCI component belongs to PCI topology but not
                                 part of chipset to provide the platform policy
  @retval EFI_INVALID_PARAMETER  If any of the input parameters are passed with
                                 invalid data

 **/
typedef
EFI_STATUS
(EFIAPI * EFI_PCI_EXPRESS_GET_DEVICE_POLICY) (
  IN CONST  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL             *This,
  IN        EFI_HANDLE                                    RootBridge,
  IN        EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS   PciAddress,
  IN        UINTN                                         Size,
  IN OUT    EFI_PCI_EXPRESS_DEVICE_POLICY                 *PciePolicy
);

/**
  Notifies the platform about the PCI device current configuration state w.r.t.
  its PCIe capabilites.

  The PCI Bus driver or PCI Host Bridge Resource Allocation Protocol drivers
  can call this member function to notify the present PCIe configuration state
  of the PCI device, to the platform. This is expected to be invoked after the
  completion of the PCI enumeration phase.

  The NotifyDeviceState() member function is meant to provide configured
  data, to the platform about the PCIe features which would be supported by the
  PCI Bus driver in future; like for example the MPS, MRRS, Extended Tag, ASPM,
  etc. The details about this PCI features can be obtained from the PCI Express
  Base Specification.

  The EFI encodings and data types used to report out the present configuration
  state, are same as those that were used by the platform to return the device-specific
  platform policies, in the EFI_PCI_EXPRESS_DEVICE_POLICY (see the "Related
  Definition" section for this). The difference is that it should return the actual
  state in the EFI_PCI_EXPRESS_DEVICE_CONFIGURATION; without any macros corresponding
  to EFI_PCI_EXPRESS_*_AUTO, and for the data types of DeviceCtl2AtomicOp and
  L1PMSubstates, its corresponding data member "Override" bit field value shall be
  ignored, will not be applicable. Note that, if the notifying driver does not
  support any PCIe feature than it shall return its corresponding value as
  EFI_PCI_EXPRESS_NOT_APPLICABLE.

  This member function will use the associated EFI handle of the PCI IO Protocol
  to address the physical PCI device within the chipset. It is caller's
  responsibility to allocate the buffer and pass its pointer to this member
  function.

  @param[in]  This                      Pointer to the  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL instance.
  @param[in]  PciDevice                 The associated PCI IO Protocol handle of the PCI
                                        device. Type EFI_HANDLE is defined in
                                        InstallProtocolInterface() in the UEFI 2.1
                                        Specification
  @param[in] Size                       The size, in bytes, of the input buffer in next parameter.
  @param[in] PciExDeviceConfiguration   The pointer to device configuration state with respect
                                        to other PCI features like, the MPS, MRRS, etc. Type
                                        EFI_PCI_EXPRESS_DEVICE_CONFIGURATION is an alias to
                                        EFI_PCI_EXPRESS_DEVICE_POLICY, as defined above.


  @retval EFI_SUCCESS             This function completed successfully, the platform
                                  was able to identify the PCI device successfully
  @retval EFI_INVALID_PARAMETER   Platform was not able to identify the PCI device;
                                  or its PCI Express device configuration state
                                  provided has invalid data.

 **/
typedef
EFI_STATUS
(EFIAPI * EFI_PCI_EXPRESS_NOTIFY_DEVICE_STATE) (
  IN CONST  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL     *This,
  IN        EFI_HANDLE                            PciDevice,
  IN        UINTN                                 Size,
  IN        EFI_PCI_EXPRESS_DEVICE_STATE          *PcieState
  );

///
/// The EFI_PCI_EXPRESS_PLATFORM_POLICY is use to exchange feature policy, at a
/// system level. Each boolean member represents each PCI Express feature defined
/// in the PCI Express Base Specification. The value TRUE on exchange indicates
/// support for this feature configuration, and FALSE indicates not supported,
/// or no configuration required on return data. The order of members represent
/// PCI Express features that are fixed for this protocol definition, and should
/// be aligned with the definition of the EFI_PCI_EXPRESS_DEVICE_POLICY.
///
typedef struct {
  //
  // For PCI Express feature - Maximum Payload Size
  //
  BOOLEAN  MaxPayloadSize;
  //
  // For PCI Express feature - Maximum Read Request Size
  //
  BOOLEAN  MaxReadRequestSize;
  //
  // For PCI Express feature - Extended Tag
  //
  BOOLEAN  ExtendedTag;
  //
  // For PCI Express feature - Relaxed Ordering
  //
  BOOLEAN  RelaxedOrdering;
  //
  // For PCI Express feature - No-Snoop
  //
  BOOLEAN  NoSnoop;
  //
  // For PCI Express feature - ASPM state
  //
  BOOLEAN  Aspm;
  //
  // For PCI Express feature - Common Clock Configuration
  //
  BOOLEAN  CommonClockConfiguration;
  //
  // For PCI Express feature - Extended Sync
  //
  BOOLEAN  ExtendedSynch;
  //
  // For PCI Express feature - Atomic Op
  //
  BOOLEAN  AtomicOp;
  //
  // For PCI Express feature - LTR
  //
  BOOLEAN  Ltr;
  //
  // For PCI Express feature - PTM
  //
  BOOLEAN  Ptm;
  //
  // For PCI Express feature - Completion Timeout
  //
  BOOLEAN  CompletionTimeout;
  //
  // For PCI Express feature - Clock Power Management
  //
  BOOLEAN  ClockPowerManagement;
  //
  // For PCI Express feature - L1 PM Substates
  //
  BOOLEAN  L1PmSubstates;
} EFI_PCI_EXPRESS_PLATFORM_POLICY;

/**
  Informs the platform about its PCI Express features support capability and it
  accepts request from platform to initialize only those features.

  The caller shall first invoke this interface to inform platform about the list
  of PCI Express feature supported, in the buffer of type EFI_PCI_EXPRESS_PLATFORM_POLICY.
  On return, it expects the list of supported PCI Express features that are required
  to be configured; in the same form with the value of TRUE. The caller shall
  ignore the feature request if its value is TRUE if that feature was initialized
  as FALSE when it was passed as the input parameter.

  The caller shall treat this list of PCI Express features as the global platform
  requirement; and corresponding to that feature list it expects device-specific
  policy through the interface GetDevicePolicy() to configure individual PCI devices.
  For example, say that the caller indicates first 8 PCI Express features in the
  list of EFI_PCI_EXPRESS_PLATFORM_POLICY, and platform wants only 5 PCI Express
  features from that list to be configured by the caller. Thus, based on feature
  list recorded here, the device-specific policy returned in the GetDevicePolicy()
  for every PCI device in the system; and the caller shall configure only 5 PCI
  Express features accordingly.

  The protocol producing driver shall use the size input parameter to determine
  the length of the buffer of type EFI_PCI_EXPRESS_PLATFORM_POLICY, and to also
  determine version of the caller. If the size of the input buffer is more than
  what its supporting version (indicated through the MajorVersion and MinorVersion
  data members of the protocol) than it shall return EFI_INVALID_PARAMETER.

  The error code in returned status essential means that the caller cannot initialize
  any PCI Express features. Thus, this interface would be primary interface for the
  caller to initialize the PCI Express features for the platform, apart from
  obtaining the handle of this protocol.

  @param[in]      This             Pointer to the  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL
                                   instance.
  @param[in]      Size             type UINTN to indicate the size of the input
                                   buffer
  @param[in,out]  PlatformPolicy   Pointer to the EFI_PCI_EXPRESS_PLATFORM_POLICY.
                                   On input, the caller indicates the list of supported
                                   PCI Express features. On output, the platform
                                   indicates the required features out of the list
                                   that needs to be initialized by the caller.

  @retval EFI_SUCCESS              The function completed successfully, returns
                                   platform policy for the PCI Express features
  @retval EFI_INVALID_PARAMETER    If any of the input parameters are passed with
                                   invalid data; like the pointer to the protocol
                                   is not valid, the size of the input buffer is
                                   greater than the version supported, the pointer
                                   to buffer is NULL.

 **/
typedef
EFI_STATUS
(EFIAPI * EFI_PCI_EXPRESS_GET_POLICY) (
  IN CONST  EFI_PCI_EXPRESS_PLATFORM_PROTOCOL     *This,
  IN        UINTN                                 Size,
  IN OUT    EFI_PCI_EXPRESS_PLATFORM_POLICY       *PlatformPolicy
);

#define EFI_PCI_EXPRESS_PLATFORM_PROTOCOL_REVISION 0x00010001

///
/// This protocol provides the interface between the PCI bus driver/PCI Host
/// Bridge Resource Allocation driver and a platform-specific driver to describe
/// the unique PCI Express features of a platform.
///
struct _EFI_PCI_EXPRESS_PLATFORM_PROTOCOL {
  ///
  /// The revision of this PCIe Platform Protocol
  ///
  UINT32                                 Revision;
  ///
  /// Retrieves the PCIe device-specific platform policy regarding enumeration.
  ///
  EFI_PCI_EXPRESS_GET_DEVICE_POLICY      GetDevicePolicy;
  ///
  /// Informs the platform about the PCIe capabilities programmed, based on the
  /// present state of the PCI device
  ///
  EFI_PCI_EXPRESS_NOTIFY_DEVICE_STATE    NotifyDeviceState;
  ///
  /// Retrieve the platform policy to select the PCI Express features
  ///
  EFI_PCI_EXPRESS_GET_POLICY             GetPolicy;

};

extern EFI_GUID   gEfiPciExpressPlatformProtocolGuid;

#endif
