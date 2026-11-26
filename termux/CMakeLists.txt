cmake_minimum_required(VERSION 3.22.1)
project(mgpu_termux_cli LANGUAGES CXX)

include(CTest)

add_library(mgpu_cli_args STATIC src/cli_args.cpp)
target_include_directories(mgpu_cli_args PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mgpu_cli_args PUBLIC cxx_std_17)
target_compile_options(mgpu_cli_args PRIVATE -Wall -Wextra -Werror)

if(BUILD_TESTING)
    add_executable(mgpu_cli_args_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cli_args_test.cpp")
    target_link_libraries(mgpu_cli_args_test PRIVATE mgpu_cli_args)
    target_compile_features(mgpu_cli_args_test PRIVATE cxx_std_17)
    add_test(NAME mgpu_cli_args_test COMMAND mgpu_cli_args_test)
endif()
