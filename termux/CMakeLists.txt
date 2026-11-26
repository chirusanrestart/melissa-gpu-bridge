cmake_minimum_required(VERSION 3.22.1)
project(mgpu_termux_cli LANGUAGES CXX)

include(CTest)

add_library(mgpu_cli_args STATIC src/cli_args.cpp)
target_include_directories(mgpu_cli_args PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mgpu_cli_args PUBLIC cxx_std_17)
target_compile_options(mgpu_cli_args PRIVATE -Wall -Wextra -Werror)

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(mgpu_cli_args_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cli_args_test.cpp")
    target_link_libraries(mgpu_cli_args_test PRIVATE mgpu_cli_args)
    target_compile_features(mgpu_cli_args_test PRIVATE cxx_std_17)
    add_test(NAME mgpu_cli_args_test COMMAND mgpu_cli_args_test)

    add_executable(mgpu_cli_smoke
        src/main.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/gpu_core_stub.cpp"
    )
    target_include_directories(mgpu_cli_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp"
    )
    target_link_libraries(mgpu_cli_smoke PRIVATE mgpu_cli_args)
    target_compile_features(mgpu_cli_smoke PRIVATE cxx_std_17)
    target_compile_options(mgpu_cli_smoke PRIVATE -Wall -Wextra -Werror)

    add_test(NAME mgpu_help COMMAND mgpu_cli_smoke help)
    set_tests_properties(mgpu_help PROPERTIES PASS_REGULAR_EXPRESSION "mgpu info")

    add_test(NAME mgpu_info_json COMMAND mgpu_cli_smoke info --json)
    set_tests_properties(mgpu_info_json PROPERTIES PASS_REGULAR_EXPRESSION "\"ok\":true")

    add_test(NAME mgpu_test_json COMMAND mgpu_cli_smoke test 1048576 --json)
    set_tests_properties(mgpu_test_json PROPERTIES PASS_REGULAR_EXPRESSION "\"element_count\":1048576")

    add_test(NAME mgpu_bench COMMAND mgpu_cli_smoke bench)
    set_tests_properties(mgpu_bench PROPERTIES
        PASS_REGULAR_EXPRESSION "1048576 elementos"
    )

    add_test(NAME mgpu_invalid_count COMMAND mgpu_cli_smoke test 0)
    set_tests_properties(mgpu_invalid_count PROPERTIES WILL_FAIL TRUE)
    add_test(NAME mgpu_json_validity COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cli_json_test.py" $<TARGET_FILE:mgpu_cli_smoke>)
endif()

if(ANDROID)
    set(MGPU_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp")
    if(NOT EXISTS "${MGPU_CORE_DIR}/vector_spv.h")
        message(FATAL_ERROR "vector_spv.h ausente. Compile o shader SPIR-V antes do CMake.")
    endif()

    find_library(VULKAN_LIBRARY vulkan)
    if(NOT VULKAN_LIBRARY)
        message(FATAL_ERROR "Android NDK Vulkan loader import library was not found.")
    endif()

    add_executable(mgpu
        src/main.cpp
        "${MGPU_CORE_DIR}/gpu_core.cpp"
    )
    target_include_directories(mgpu PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${MGPU_CORE_DIR}"
    )
    target_link_libraries(mgpu PRIVATE mgpu_cli_args "${VULKAN_LIBRARY}")
    target_compile_features(mgpu PRIVATE cxx_std_17)
    target_compile_options(mgpu PRIVATE -Wall -Wextra -Werror=return-type)
endif()
