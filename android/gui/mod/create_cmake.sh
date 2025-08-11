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

add_library($m SHARED \${MOD_DIR}/main.c)
# target_link_libraries($m z)
EOF
done


# main.cc
