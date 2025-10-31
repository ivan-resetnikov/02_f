@echo off

set clang_bin=clang

:: Library paths
set vendor_src_path=C:\dev\c++\.cmake_installs\src\vendor
set vendor_inc_path=C:\dev\c++\.cmake_installs\include
set vendor_lib_path=C:\dev\c++\.cmake_installs\lib

:: Source files
set src_path=./src

set src_files=
set src_files=%src_files% %src_path%/pack.c

:: Include
set include_flags=
set include_flags=%include_flags% -I%vendor_inc_path%

:: Linking
set link_flags=
set link_flags=%link_flags% -L%vendor_lib_path%
set link_flags=%link_flags% -lSDL3

:: Compile
set compile_command=%clang_bin% %include_flags% %src_files% %link_flags% -o ./bin/pack.exe

echo %clang_bin%:
echo %compile_command%

@REM mkdir -p "./bin"
%compile_command%
