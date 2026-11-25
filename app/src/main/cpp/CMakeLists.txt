cmake_minimum_required(VERSION 3.22.1)
project(mgpu LANGUAGES CXX)

add_library(mgpu SHARED native-lib.cpp)

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

if(NOT vulkan-lib)
    message(FATAL_ERROR "Vulkan loader library was not found in the Android NDK sysroot")
endif()

target_link_libraries(mgpu PRIVATE ${vulkan-lib} ${log-lib} ${android-lib})
target_compile_features(mgpu PRIVATE cxx_std_17)
target_compile_options(mgpu PRIVATE -Wall -Wextra -Werror=return-type)
