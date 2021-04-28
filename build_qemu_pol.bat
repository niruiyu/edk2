call build -p UefiPayloadPkg\UefiPayloadPkg.dsc -a IA32 -a X64 -D UNIVERSAL_PAYLOAD=TRUE -t VS2019 -y upllog.txt
if not %ERRORLEVEL% == 0 exit /b 1

call py -3 UefiPayloadPkg\tools\pack_payload.py -i Build\UefiPayloadPkgX64\DEBUG_VS2019\FV\UEFIPAYLOAD.fd -a 4096 -ai -o UPL.bin
if not %ERRORLEVEL% == 0 exit /b 1

call build -p OvmfPkg\OvmfPkgPol.dsc -a IA32 -a X64 -D DEBUG_ON_SERIAL_PORT -t VS2019 -y ovmflog.txt


@REM llvm-objcopy -I elf32-i386 -O elf32-i386 --add-section .upld.uefi_fv=Build\UefiPayloadPkgX64\DEBUG_VS2019\FV\DXEFV.Fv Build\UefiPayloadPkgX64\DEBUG_CLANGELFPDB\IA32\UniversalPayloadEntry.efi
@REM llvm-objcopy -I elf32-i386 -O elf32-i386 --set-section-alignment .upld.uefi_fv=4096 Build\UefiPayloadPkgX64\DEBUG_CLANGELFPDB\IA32\UniversalPayloadEntry.efi