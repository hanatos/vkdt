#!/bin/bash
# re-create sub directories and their CMakeLists.txt files
# according to the modules in the real source tree.

>../mods.cmake
# main.c
cd ../../../src/pipe/modules
mods=$(find . -name main.c | cut -f2 -d/ | uniq)
cd -
for m in $mods
do
  mkdir -p $m
  echo "add_subdirectory(mod/$m)" >> ../mods.cmake
  cat << EOF > $m/CMakeLists.txt
set(NAME $m)
set(MOD $m)
set(SRC_DIR ../../../../src)
set(MOD_DIR \${SRC_DIR}/pipe/modules/\${MOD})
set(ASS_DIR ../../assets)
include_directories(\${SRC_DIR})
include_directories(\${MOD_DIR})

set(CMAKE_C_FLAGS "\${CMAKE_C_FLAGS} -DVKDT_DSO_BUILD -D__USE_BSD=1")
add_library($m SHARED \${MOD_DIR}/main.c)
target_link_libraries($m android)
EOF
done

# dependencies and special stuff:
cat << EOF >> i-jpg/CMakeLists.txt
include_directories(\${CMAKE_BINARY_DIR}/libjpeg-turbo/src)
include_directories(\${CMAKE_BINARY_DIR}/libjpeg-turbo-build)
add_dependencies(i-jpg jpeg-turbo)
target_link_libraries(i-jpg jpeg)
EOF
cat << EOF >> o-jpg/CMakeLists.txt
include_directories(\${CMAKE_BINARY_DIR}/libjpeg-turbo/src)
include_directories(\${CMAKE_BINARY_DIR}/libjpeg-turbo-build)
add_dependencies(o-jpg jpeg-turbo)
target_link_libraries(o-jpg jpeg)
EOF
cat << EOF >> i-bc1/CMakeLists.txt
target_link_libraries(i-bc1 z)
EOF
cat << EOF >> o-bc1/CMakeLists.txt
target_link_libraries(o-bc1 z)
EOF
cat << EOF >> i-raw/CMakeLists.txt
add_library(rawloader STATIC IMPORTED)
# for whatever reason the cwd is fucked up later so we expand it during parse time:
file(REAL_PATH \${MOD_DIR} MOD_ABS BASE_DIRECTORY "\${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(rawloader PROPERTIES IMPORTED_LOCATION \${MOD_ABS}/rawloader-c/target/aarch64-linux-android/release/librawloader.a)
target_link_libraries(i-raw rawloader)
EOF


# main.cc
# TODO the c++ stuff: i-exr o-exr i-mcraw

# now create list of modules
cd ../../../src/pipe/modules
mods=$(find . -name readme.md | cut -f2 -d/| sort|uniq | grep -v quake | grep -v readme.md)
cat << EOF > modlist.h
#pragma once
static const char *const dt_mod[] = {
EOF
for m in $mods
do
  echo \"$m\", >> modlist.h
done
echo "};" >> modlist.h
cd -
