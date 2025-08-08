cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(vkdt)
set(NAME vkdt)

find_package(Vulkan REQUIRED)

set(SRC_DIR ../../src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -DVK_USE_PLATFORM_ANDROID_KHR") # -DVK_NO_PROTOTYPES")

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

file(GLOB V_GLOB LIST_DIRECTORIES true "mod/*")
foreach(item ${V_GLOB})
	if(IS_DIRECTORY ${item})
		add_subdirectory(${item})
	endif()
endforeach()
