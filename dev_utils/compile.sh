#!/bin/bash

clang_bin="clang"
pack_bin="./bin/pack"

# Library paths
vendor_src_path="/home/ivan/dev/c++/.cmake_installs/src/vendor"
vendor_inc_path="/home/ivan/dev/c++/.cmake_installs/include"
vendor_lib_path="/home/ivan/dev/c++/.cmake_installs/lib"

# Source files
src_path="./src"

src_files=""
src_files="${src_files} ${src_path}/main.c"
src_files="${src_files} ${vendor_src_path}/gl.c"

# Include
include_flags=""
include_flags="${include_flags} -I${vendor_inc_path}"

# Linking
link_flags=""
link_flags="${link_flags} ${vendor_lib_path}/libSDL3.a"
link_flags="${link_flags} -lm"

# Compile
mkdir -p "./bin"

compile_command="${clang_bin} ${include_flags} ${src_files} ${link_flags} -o ./bin/main"

echo "${clang_bin}:"
echo "${compile_command}"
${compile_command}

# Pack assets
exclusion_flags=""
exclusion_flags="${exclusion_flags} -x:*.blend"
exclusion_flags="${exclusion_flags} -x:*.blend1"

pack_command="${pack_bin} -i:./assets -o:./bin/assets.bin ${exclusion_flags}"

echo "${pack_bin}:"
echo "${pack_command}"

${pack_command}
