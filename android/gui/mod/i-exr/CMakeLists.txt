cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(vkdt)
set(NAME i-exr)

set(MOD i-exr)
set(SRC_DIR ../../../../src)
set(MOD_DIR ${SRC_DIR}/pipe/modules/${MOD})
set(ASS_DIR ../../assets)
include_directories(${SRC_DIR})
include_directories(${MOD_DIR})

add_library(i-exr SHARED
    ${MOD_DIR}/main.cc)
target_link_libraries(i-exr z)

# nothing fucking works. maybe grab from like 
#  ./gui/build/intermediates/stripped_native_libs/release/stripReleaseDebugSymbols/out/lib/arm64-v8a/libi-exr.so
# and copy post build in gradle.
# install(TARGETS i-exr LIBRARY DESTINATION ../../assets/${MOD})
set_target_properties(i-exr PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${ASS_DIR}/${MOD})
