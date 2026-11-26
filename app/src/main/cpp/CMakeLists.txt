cmake_minimum_required(VERSION 3.22.1)
project(mgpu LANGUAGES CXX)

if(NOT EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/vector_spv.h")
    message(FATAL_ERROR "vector_spv.h ausente. Execute tools/embed_spirv.py antes do Gradle.")
endif()

add_library(mgpu SHARED native-lib.cpp)
find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

if(NOT vulkan-lib)
    message(FATAL_ERROR "Vulkan loader library was not found in the Android NDK sysroot")
endif()

target_link_libraries(mgpu PRIVATE \${vulkan-lib} \${log-lib} \${android-lib})
target_compile_features(mgpu PRIVATE cxx_std_17)
target_compile_options(mgpu PRIVATE -Wall -Wextra -Werror=return-type)
