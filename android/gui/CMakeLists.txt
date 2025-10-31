cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(vkdt)
set(NAME vkdt)

find_package(Vulkan REQUIRED)

set(SRC_DIR ../../src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -D__USE_BSD=1 -DVK_USE_PLATFORM_ANDROID_KHR") # -DVK_NO_PROTOTYPES")

file(GLOB CORE_SRC "${SRC_DIR}/core/*.c")
file(GLOB DB_SRC "${SRC_DIR}/db/*.c")
file(GLOB GUI_SRC "${SRC_DIR}/gui/*.c")
file(GLOB PIPE_SRC "${SRC_DIR}/pipe/*.c")
file(GLOB QVK_SRC "${SRC_DIR}/qvk/*.c")
file(GLOB SND_SRC "${SRC_DIR}/snd/none.c")
set(VKDT_SRC ${GUI_SRC} ${CORE_SRC} ${DB_SRC} ${PIPE_SRC} ${QVK_SRC} ${SND_SRC}) # src/main/vulkan_wrapper/vulkan_wrapper.c)
# include_directories(src/main/vulkan_wrapper)

add_library(native-lib SHARED ${VKDT_SRC})
add_library(native-app-glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
# or?
# set(CMAKE_C_VISIBILITY_PRESET hidden)
# set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic -fvisibility=hidden")

include_directories(${Vulkan_INCLUDE_DIR})
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/core)
include_directories(${SRC_DIR}/db)
include_directories(${SRC_DIR}/gui)
include_directories(${SRC_DIR}/pipe)
include_directories(${SRC_DIR}/qvk)
include_directories(${SRC_DIR}/snd)
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(
    native-lib
    native-app-glue
    android
    log
    ${Vulkan_LIBRARIES}
)

# TODO get these handed in from gradle?
set(ANDROID_VERSION 31)
set(NDK_PATH /opt/android-sdk/ndk/28.2.13676358/)
include(ExternalProject)
# use frozen hash for current main so we don't need
# internet access all the time
ExternalProject_Add(jpeg-turbo
  GIT_REPOSITORY    https://github.com/libjpeg-turbo/libjpeg-turbo.git
  GIT_TAG           f158143ec0caceb7fd3c073205ded90f4e2b98eb
  SOURCE_DIR        "${CMAKE_BINARY_DIR}/libjpeg-turbo"
  BINARY_DIR        "${CMAKE_BINARY_DIR}/libjpeg-turbo-build"
  CMAKE_ARGS        -DANDROID_ABI=arm64-v8a -DANDROID_ARM_MODE=arm -DANDROID_PLATFORM=android-${ANDROID_VERSION} -DANDROID_TOOLCHAIN=clang -DCMAKE_ASM_FLAGS="--target=aarch64-linux-android${ANDROID_VERSION}" -DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake
  INSTALL_DIR       "${CMAKE_BINARY_DIR}"
  INSTALL_COMMAND   /bin/true
)
link_directories("${CMAKE_BINARY_DIR}/libjpeg-turbo-build")

# because everybody loves bloatware we have to list all subdirectories
# one by one by hand. !@#$)*&
include(mods.cmake)
