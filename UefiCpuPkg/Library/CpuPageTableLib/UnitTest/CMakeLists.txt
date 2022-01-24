# CMakeList.txt : CMake project for PageTableLibTest, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("PageTableLibTest")

add_compile_definitions (UNIT_TEST)
include_directories ("../../../../MdePkg/Include" "../../../../MdePkg/Include/X64" "../../../../UefiCpuPkg/Include")
# Add source to this project's executable.
add_executable (PageTableLibTest "PageTableLibTest.c" "../CpuPageTableMap.c" "../CpuPageTable.h" "../CpuPageTableParse.c")

# TODO: Add tests and install targets if needed.
