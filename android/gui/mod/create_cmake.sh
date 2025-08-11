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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVKDT_DSO_BUILD")
add_library($m SHARED \${MOD_DIR}/main.c)
# target_link_libraries($m z)
EOF
done

# dependencies and special stuff:
cat << EOF >> i-jpg/CMakeLists.txt
add_dependencies(i-jpg jpeg-turbo)
target_link_libraries(i-jpg jpeg)
EOF
cat << EOF >> o-jpg/CMakeLists.txt
add_dependencies(o-jpg jpeg-turbo)
target_link_libraries(o-jpg jpeg)
EOF
cat << EOF >> i-bc1/CMakeLists.txt
target_link_libraries(i-bc1 z)
EOF
cat << EOF >> o-bc1/CMakeLists.txt
target_link_libraries(o-bc1 z)
EOF

# main.cc
