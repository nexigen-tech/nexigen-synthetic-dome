# Chronos runtime library.
#
# Simulation-time distribution service: integer-nanosecond time bases,
# execution-mode policy, and frame-context distribution.  Built with the same
# ABI constraints as the rest of the project: C++23, no exceptions/RTTI, and
# warnings treated as errors.

add_library(nxd_chronos STATIC
    src/chronos.cpp
    src/state_machine.cpp
    src/partition.cpp
    src/let_boundary.cpp
    src/c_api.cpp)

add_library(nxd_chronos_capi SHARED
    src/c_api.cpp
    src/state_machine.cpp
    src/partition.cpp
    src/let_boundary.cpp
    src/chronos.cpp)

target_include_directories(nxd_chronos PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")

target_link_libraries(nxd_chronos PUBLIC
    nexigen::core)

target_compile_options(nxd_chronos PUBLIC
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

set_target_properties(nxd_chronos PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

set_target_properties(nxd_chronos_capi PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_include_directories(nxd_chronos_capi PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")

target_link_libraries(nxd_chronos_capi PUBLIC
    nexigen::core)

target_compile_options(nxd_chronos_capi PUBLIC
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_dependencies(nxd_chronos_capi nxd_runtime_chronos_header)

# -----------------------------------------------------------------------------
# C header generation from the Rust crate layout (cbindgen).
# -----------------------------------------------------------------------------
set(NXD_RUNTIME_CHRONOS_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/rust/nxd_runtime_chronos.h")

add_custom_command(
    OUTPUT "${NXD_RUNTIME_CHRONOS_HEADER}"
    COMMAND cbindgen --crate nxd-runtime-chronos --output "${NXD_RUNTIME_CHRONOS_HEADER}" --config "${CMAKE_CURRENT_SOURCE_DIR}/rust/cbindgen.toml"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Regenerating Chronos runtime C header with cbindgen"
    VERBATIM)

add_custom_target(nxd_runtime_chronos_header
    DEPENDS "${NXD_RUNTIME_CHRONOS_HEADER}")

add_dependencies(nxd_chronos nxd_runtime_chronos_header)

# -----------------------------------------------------------------------------
# Cross-language layout verification test.
# -----------------------------------------------------------------------------
add_executable(nxd_chronos_time_layout_test tests/time_layout_test.cpp)
add_dependencies(nxd_chronos_time_layout_test nxd_runtime_chronos_header)

target_link_libraries(nxd_chronos_time_layout_test PRIVATE nxd_chronos)

target_include_directories(nxd_chronos_time_layout_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/rust")

target_compile_options(nxd_chronos_time_layout_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_chronos_time_layout COMMAND nxd_chronos_time_layout_test)

add_executable(nxd_chronos_state_machine_test tests/state_machine_test.cpp)
add_dependencies(nxd_chronos_state_machine_test nxd_runtime_chronos_header)

target_link_libraries(nxd_chronos_state_machine_test PRIVATE nxd_chronos)

target_include_directories(nxd_chronos_state_machine_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/rust")

target_compile_options(nxd_chronos_state_machine_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_chronos_state_machine COMMAND nxd_chronos_state_machine_test)

add_executable(nxd_chronos_mode_policy_test tests/mode_policy_test.cpp)
add_dependencies(nxd_chronos_mode_policy_test nxd_runtime_chronos_header)

target_link_libraries(nxd_chronos_mode_policy_test PRIVATE nxd_chronos)

target_include_directories(nxd_chronos_mode_policy_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/rust")

target_compile_options(nxd_chronos_mode_policy_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_chronos_mode_policy COMMAND nxd_chronos_mode_policy_test)

add_executable(nxd_chronos_frame_distribution_test tests/frame_distribution_test.cpp)
add_dependencies(nxd_chronos_frame_distribution_test nxd_runtime_chronos_header)

target_link_libraries(nxd_chronos_frame_distribution_test PRIVATE nxd_chronos)

target_include_directories(nxd_chronos_frame_distribution_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/rust")

target_compile_options(nxd_chronos_frame_distribution_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_chronos_frame_distribution COMMAND nxd_chronos_frame_distribution_test)

add_executable(nxd_chronos_c_api_test tests/c_api_test.cpp)
add_dependencies(nxd_chronos_c_api_test nxd_runtime_chronos_header)

target_link_libraries(nxd_chronos_c_api_test PRIVATE nxd_chronos)

target_include_directories(nxd_chronos_c_api_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/rust")

target_compile_options(nxd_chronos_c_api_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_chronos_c_api COMMAND nxd_chronos_c_api_test)

add_executable(nxd_chronos_let_boundary_test tests/let_boundary_test.cpp)
add_dependencies(nxd_chronos_let_boundary_test nxd_runtime_chronos_header)

target_link_libraries(nxd_chronos_let_boundary_test PRIVATE nxd_chronos)

target_include_directories(nxd_chronos_let_boundary_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/rust")

target_compile_options(nxd_chronos_let_boundary_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_chronos_let_boundary COMMAND nxd_chronos_let_boundary_test)
