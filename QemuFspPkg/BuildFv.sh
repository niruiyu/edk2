#!/bin/bash

# FSP build script
#
# Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.
#
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution. The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php.
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

FSP_PKG_NAME="QemuFspPkg"
FSP_BASENAME="QEMUFSP"
TOOL_CHAIN_TAG="GCC"
OUTPUT_DIR="Build/${FSP_PKG_NAME}/"
SYMBOL_PREFIX="_"

function usage {
  echo "Usage: $0 [/h | /? | /r | /d | /clean]"
  exit 1
}
if [[ "$1" == "/h" || "$1" == "/?" ]]; then
  usage
  exit 1
fi

if [[ -z "$WORKSPACE" ]]; then
  cd $(dirname "$0")/..
  pwd
  . edksetup.sh --reconfig
  export NASM_PREFIX="/usr/bin/"
fi

MISC_FLAGS=""
FSP_BD_COMMON="-p ${FSP_PKG_NAME}/${FSP_PKG_NAME}.dsc ${MISC_FLAGS} -a X64 -n 4 -t ${TOOL_CHAIN_TAG} -Y PCD -Y LIBRARY"

case "$1" in
  "/debug")
    echo WORKSPACE=$WORKSPACE
    echo EDK_TOOLS_PATH=$EDK_TOOLS_PATH
    echo FSP_PKG_NAME=$FSP_PKG_NAME
    echo $FSP_BASENAME
    echo $TOOL_CHAIN_TAG
    echo $OUTPUT_DIR
    exit 0
    ;;
  "/clean")
    echo "Removing Build and Conf directories ..."
    rm -rf Build Conf *.log
    unset WORKSPACE
    unset EDK_TOOLS_PATH
    exit 0
    ;;
  "/r")
    BD_TARGET="RELEASE"
    BD_MACRO="${MISC_FLAGS}"
    BD_ARGS="${FSP_BD_COMMON} -b RELEASE ${BD_MACRO} -y ${OUTPUT_DIR}/ReportRelease.log"
    FSP_BUILD_TYPE="0x0001"
    FSP_RELEASE_TYPE="0x0002"
    ;;
  "/d" | "")
    BD_TARGET="DEBUG"
    BD_MACRO="${MISC_FLAGS}"
    BD_ARGS="${FSP_BD_COMMON} -b DEBUG ${BD_MACRO} -y ${OUTPUT_DIR}/ReportDebug.log"
    FSP_BUILD_TYPE="0x0000"
    FSP_RELEASE_TYPE="0x0000"
    ;;
  *)
    echo "ERROR: "$1" is not a valid parameter."
    usage
    ;;
esac

# Build process
function prebuild {
  echo "Start of PreBuild ..."
  TOOL_MACRO="${BD_MACRO} -DFSP_VER=${FSP_VER}"
  FSP_T_UPD_GUID="34686CA3-34F9-4901-B82A-BA630F0714C6"
  FSP_M_UPD_GUID="39A250DB-E465-4DD1-A2AC-E2BD3C0E2385"
  FSP_S_UPD_GUID="CAE3605B-5B34-4C85-B3D7-27D54273C40F"

  python3 IntelFsp2Pkg/Tools/GenCfgOpt.py UPDTXT \
    ${FSP_PKG_NAME}/${FSP_PKG_NAME}.dsc \
    Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV \
    ${TOOL_MACRO}
  if [[ $? -eq 256 ]]; then
    echo "DSC is not changed, no need to recreate MAP and BIN file"
  else
    for GUID in ${FSP_T_UPD_GUID} ${FSP_M_UPD_GUID} ${FSP_S_UPD_GUID}; do
      rm -f Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${GUID}.bin \
            Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${GUID}.map
      BPDG Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${GUID}.txt \
          -o Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${GUID}.bin \
          -m Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${GUID}.map
      if [[ $? -eq 1 ]]; then
        echo "PreBuild failed!"
        exit 1
      fi
    done
  fi


  python3 IntelFsp2Pkg/Tools/GenCfgOpt.py HEADER \
    ${FSP_PKG_NAME}/${FSP_PKG_NAME}.dsc \
    Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV \
    ${FSP_PKG_NAME}/Include/BootLoaderPlatformData.h \
    ${TOOL_MACRO}
  if [[ $? -eq 256 ]]; then
    echo "No need to recreate header file"
  else
    python3 IntelFsp2Pkg/Tools/GenCfgOpt.py GENBSF \
      ${FSP_PKG_NAME}/${FSP_PKG_NAME}.dsc \
      Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV \
      Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${FSP_BASENAME}.bsf \
      ${TOOL_MACRO}
    if [[ $? -eq 1 ]]; then
      echo "PreBuild failed!"
      exit 1
    fi
    if [[ $? -eq 256 ]]; then
      echo "No need to recreate bsf file"
    else
      echo "BSF file created successfully"
    fi

    
  fi

  echo "End of PreBuild ..."
}

function build_fsp {
  echo "PREBUILD"
  prebuild
  echo "BUILD"
  build -m ${FSP_PKG_NAME}/FspHeader/FspHeader.inf ${BD_ARGS} -DCFG_PREBUILD
  if [[ $? -ne 0 ]]; then
    exit 1
  fi
  build ${BD_ARGS}
  if [[ $? -ne 0 ]]; then
    exit 1
  fi
  echo "POSTBUILD"
  postbuild
}

function postbuild {
  echo "Start of PostBuild ..."

  echo "Patching FSP-M ..."
  python3 IntelFsp2Pkg/Tools/PatchFv.py \
    Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV \
    FSP-M:${FSP_BASENAME} \
    "0x0000,            _BASE_FSP-M_,                                                                          @Temporary Base" \
    "<[0x0000]>+0x00AC, [<[0x0000]>+0x0020],                                                                   @FSP-M Size" \
    "<[0x0000]>+0x00B0, [0x0000],                                                                              @FSP-M Base" \
    "<[0x0000]>+0x00B4, ([<[0x0000]>+0x00B4] & 0xFFFFFFFF) | 0x0001,                                           @FSP-M Image Attribute" \
    "<[0x0000]>+0x00B6, ([<[0x0000]>+0x00B6] & 0xFFFF0FFC) | 0x2000 | ${FSP_BUILD_TYPE} | ${FSP_RELEASE_TYPE}, @FSP-M Component Attribute" \
    "<[0x0000]>+0x00B8, D5B86AEA-6AF7-40D4-8014-982301BC3D89:0x1C - <[0x0000]>,                                @FSP-M CFG Offset" \
    "<[0x0000]>+0x00BC, [D5B86AEA-6AF7-40D4-8014-982301BC3D89:0x14] & 0xFFFFFF - 0x001C,                       @FSP-M CFG Size" \
    "<[0x0000]>+0x00D0, Fsp24SecCoreM:${SYMBOL_PREFIX}FspMemoryInitApi - [0x0000],                             @MemoryInitApi API" \
    "<[0x0000]>+0x00D4, Fsp24SecCoreM:${SYMBOL_PREFIX}TempRamExitApi - [0x0000],                               @TempRamExit API" \
    "Fsp24SecCoreM:${SYMBOL_PREFIX}FspPeiCoreEntryOff, PeiCore:${SYMBOL_PREFIX}_ModuleEntryPoint - [0x0000],   @PeiCore Entry" \
    "0x0000,            0x00000000,                                                                            @Restore the value" \
    "Fsp24SecCoreM:${SYMBOL_PREFIX}FspInfoHeaderRelativeOff, Fsp24SecCoreM:${SYMBOL_PREFIX}AsmGetFspInfoHeader - {912740BE-2284-4734-B971-84B027353F0C:0x1C}, @FSP-M Header Offset"

  if [[ $? -ne 0 ]]; then
    echo "PostBuild failed!"
    exit 1
  fi

  echo "Patching FSP-S ..."
  python3 IntelFsp2Pkg/Tools/PatchFv.py \
    Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV \
    FSP-S:${FSP_BASENAME} \
    "0x0000,            _BASE_FSP-S_,                                                                          @Temporary Base" \
    "<[0x0000]>+0x00AC, [<[0x0000]>+0x0020],                                                                   @FSP-S Size" \
    "<[0x0000]>+0x00B0, [0x0000],                                                                              @FSP-S Base" \
    "<[0x0000]>+0x00B4, ([<[0x0000]>+0x00B4] & 0xFFFFFFFF) | 0x0001,                                           @FSP-S Image Attribute" \
    "<[0x0000]>+0x00B6, ([<[0x0000]>+0x00B6] & 0xFFFF0FFC) | 0x3000 | ${FSP_BUILD_TYPE} | ${FSP_RELEASE_TYPE}, @FSP-S Component Attribute" \
    "<[0x0000]>+0x00B8, E3CD9B18-998C-4F76-B65E-98B154E5446F:0x1C - <[0x0000]>,                                @FSP-S CFG Offset" \
    "<[0x0000]>+0x00BC, [E3CD9B18-998C-4F76-B65E-98B154E5446F:0x14] & 0xFFFFFF - 0x001C,                       @FSP-S CFG Size" \
    "<[0x0000]>+0x00D8, Fsp24SecCoreS:${SYMBOL_PREFIX}FspSiliconInitApi - [0x0000],                            @SiliconInit API" \
    "<[0x0000]>+0x00CC, Fsp24SecCoreS:${SYMBOL_PREFIX}NotifyPhaseApi - [0x0000],                               @NotifyPhase API" \
    "Fsp24SecCoreS:${SYMBOL_PREFIX}FspPeiCoreEntryOff, PeiCore:${SYMBOL_PREFIX}_ModuleEntryPoint - [0x0000],   @PeiCore Entry" \
    "0x0000,            0x00000000,                                                                            @Restore the value" \
    "Fsp24SecCoreS:${SYMBOL_PREFIX}FspInfoHeaderRelativeOff, Fsp24SecCoreS:${SYMBOL_PREFIX}AsmGetFspInfoHeader - {912740BE-2284-4734-B971-84B027353F0C:0x1C}, @FSP-S Header Offset"

  if [[ $? -ne 0 ]]; then
    echo "PostBuild failed!"
    exit 1
  fi

  cp Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${FSP_BASENAME}.fd ${OUTPUT_DIR}
  cp Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/${FSP_BASENAME}.bsf ${OUTPUT_DIR}
  cp Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/FspUpd.h ${OUTPUT_DIR}
  cp Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/FsptUpd.h ${OUTPUT_DIR}
  cp Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/FspmUpd.h ${OUTPUT_DIR}
  cp Build/${FSP_PKG_NAME}/${BD_TARGET}_${TOOL_CHAIN_TAG}/FV/FspsUpd.h ${OUTPUT_DIR}

  python3 IntelFsp2Pkg/Tools/SplitFspBin.py \
    rebase -f ${OUTPUT_DIR}/${FSP_BASENAME}.fd \
    -c s m -b 0xFFD80000 0xFFDC4000 \
    -o ${OUTPUT_DIR} -n QEMU_FSP_REBASE.fd

  python3 IntelFsp2Pkg/Tools/SplitFspBin.py \
    split -f ${OUTPUT_DIR}/QEMU_FSP_REBASE.fd \
    -o ${OUTPUT_DIR}

  echo "Patch is DONE"

  echo "End of PostBuild ..."
}

function build_qsp {
  echo "Start of Build QSP ..."
  cd ../edk2-platforms/Platform/Intel
  python build_bios.py -p BoardX58Ich10X64
}

pushd .
build_fsp
build_qsp
popd