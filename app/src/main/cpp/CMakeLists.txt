cmake_minimum_required(VERSION 3.22.1)
project(mgpu LANGUAGES CXX)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vector_spv.h")
    message(FATAL_ERROR "vector_spv.h ausente. Execute tools/embed_spirv.py antes do Gradle.")
endif()

set(FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../ffmpeg-android" CACHE PATH "FFmpeg Android output")
set(FFMPEG_ABI_DIR "${FFMPEG_ROOT}/${ANDROID_ABI}")
set(SHADERC_ABI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../shaderc-android/${ANDROID_ABI}")

if(NOT EXISTS "${FFMPEG_ABI_DIR}/include/libavutil/avutil.h")
    message(FATAL_ERROR "Headers FFmpeg ausentes para ${ANDROID_ABI}: ${FFMPEG_ABI_DIR}/include")
endif()
if(NOT EXISTS "${SHADERC_ABI_DIR}/include/shaderc/shaderc.h")
    message(FATAL_ERROR "Headers Shaderc ausentes para ${ANDROID_ABI}: ${SHADERC_ABI_DIR}/include")
endif()
if(NOT EXISTS "${SHADERC_ABI_DIR}/lib/libshaderc.a")
    message(FATAL_ERROR "Biblioteca Shaderc ausente para ${ANDROID_ABI}: ${SHADERC_ABI_DIR}/lib/libshaderc.a")
endif()

set(FFMPEG_LIBRARIES avfilter avformat avcodec swscale swresample avutil)
foreach(component IN LISTS FFMPEG_LIBRARIES)
    if(NOT EXISTS "${FFMPEG_ABI_DIR}/lib/lib${component}.a")
        message(FATAL_ERROR "Biblioteca FFmpeg ausente: ${FFMPEG_ABI_DIR}/lib/lib${component}.a")
    endif()
    add_library(ffmpeg_${component} STATIC IMPORTED GLOBAL)
    set_target_properties(ffmpeg_${component} PROPERTIES
        IMPORTED_LOCATION "${FFMPEG_ABI_DIR}/lib/lib${component}.a"
    )
endforeach()

add_library(ffmpeg_shaderc STATIC IMPORTED GLOBAL)
set_target_properties(ffmpeg_shaderc PROPERTIES
    IMPORTED_LOCATION "${SHADERC_ABI_DIR}/lib/libshaderc.a"
)

add_library(mgpu SHARED
    native-lib.cpp
    gpu_core.cpp
    ffmpeg_bridge.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

if(NOT vulkan-lib)
    message(FATAL_ERROR "Vulkan loader library was not found in the Android NDK sysroot")
endif()

target_include_directories(mgpu PRIVATE
    "${FFMPEG_ABI_DIR}/include"
    "${SHADERC_ABI_DIR}/include"
)

target_link_libraries(mgpu PRIVATE
    ffmpeg_avfilter
    ffmpeg_avformat
    ffmpeg_avcodec
    ffmpeg_swscale
    ffmpeg_swresample
    ffmpeg_avutil
    ffmpeg_shaderc
    ${vulkan-lib}
    ${log-lib}
    ${android-lib}
)

# FFmpeg and static C++ dependencies may require libc++ atomic symbols on
# some ARM API levels. NDK 22+ commonly provides them through libc++ itself,
# so resolve libatomic only when it is actually present.
find_library(ATOMIC_LIBRARY atomic)
if(ATOMIC_LIBRARY)
    target_link_libraries(mgpu PRIVATE ${ATOMIC_LIBRARY})
endif()

target_compile_features(mgpu PRIVATE cxx_std_17)
target_compile_options(mgpu PRIVATE -Wall -Wextra -Werror=return-type)
