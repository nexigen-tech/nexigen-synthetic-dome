cmake_minimum_required(VERSION 3.25)
project(nexigen-synthetic-dome VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build-local path to the sibling nexigen-cpp repository.
set(NEXIGEN_CPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../nexigen-ecosystem/nexigen-cpp"
    CACHE PATH "Path to the local nexigen-cpp repository")

# Wire the local nexigen package as if it were part of the build tree.
set(NEXIGEN_BUILD_TESTS OFF)
set(NEXIGEN_BUILD_EXAMPLES OFF)
set(NEXIGEN_BUILD_TOOLS OFF)
set(NEXIGEN_BUILD_FUZZERS OFF)
set(NEXIGEN_ENABLE_COVERAGE OFF)
set(NEXIGEN_ENABLE_ASAN OFF)
set(NEXIGEN_ENABLE_TSAN OFF)
set(NEXIGEN_NATIVE_ARCH OFF)

add_subdirectory("${NEXIGEN_CPP_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/nexigen-cpp")

# -----------------------------------------------------------------------------
# Contract schema compiler (nxdc) integration
# -----------------------------------------------------------------------------
# Generated C++ headers live under the build tree so they are never committed.
set(NXD_GENERATED_CPP_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/cpp/include")
set(NXD_GENERATED_CONTRACTS_DIR "${NXD_GENERATED_CPP_DIR}/nxd/contracts")
file(MAKE_DIRECTORY "${NXD_GENERATED_CONTRACTS_DIR}")

set(NXD_CONTRACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/contracts")
set(NXD_CONTRACT_SOURCES
    "${NXD_CONTRACTS_DIR}/SampleHeader.yaml"
    "${NXD_CONTRACTS_DIR}/RawCanFrame.yaml"
    "${NXD_CONTRACTS_DIR}/NestedRecord.yaml"
    "${NXD_CONTRACTS_DIR}/RawCanBatch.yaml"
    "${NXD_CONTRACTS_DIR}/SignalValue.yaml"
    "${NXD_CONTRACTS_DIR}/CanHeartbeat.yaml"
    "${NXD_CONTRACTS_DIR}/CanAggregateErrors.yaml"
    "${NXD_CONTRACTS_DIR}/VideoFrame.yaml"
    "${NXD_CONTRACTS_DIR}/VideoHeartbeat.yaml"
    "${NXD_CONTRACTS_DIR}/VideoAggregateErrors.yaml")

# Concrete contracts that produce exactly one header.
set(NXD_SINGLE_HEADERS)
foreach(yaml IN ITEMS SampleHeader RawCanFrame NestedRecord CanHeartbeat CanAggregateErrors VideoFrame VideoHeartbeat VideoAggregateErrors)
    set(source "${NXD_CONTRACTS_DIR}/${yaml}.yaml")
    set(header "${NXD_GENERATED_CONTRACTS_DIR}/${yaml}.hpp")
    list(APPEND NXD_SINGLE_HEADERS "${header}")
    add_custom_command(
        OUTPUT "${header}"
        COMMAND cargo run -q -p nxdc -- emit-cpp "${source}" "${header}"
        DEPENDS "${source}"
        COMMENT "Generating C++ header for ${yaml}"
        VERBATIM)
endforeach()

# Parametric contracts produce one header per concrete instantiation.
set(NXD_BATCH_HEADERS
    "${NXD_GENERATED_CONTRACTS_DIR}/RawCanBatchClassical.hpp"
    "${NXD_GENERATED_CONTRACTS_DIR}/RawCanBatchFd.hpp"
    "${NXD_GENERATED_CONTRACTS_DIR}/RawCanBatchXl.hpp")
add_custom_command(
    OUTPUT ${NXD_BATCH_HEADERS}
    COMMAND cargo run -q -p nxdc -- emit-cpp "${NXD_CONTRACTS_DIR}/RawCanBatch.yaml" "${NXD_GENERATED_CONTRACTS_DIR}"
    DEPENDS "${NXD_CONTRACTS_DIR}/RawCanBatch.yaml"
    COMMENT "Generating C++ headers for RawCanBatch concrete instances"
    VERBATIM)

set(NXD_SIGNALVALUE_HEADERS
    "${NXD_GENERATED_CONTRACTS_DIR}/SignalValueF32.hpp"
    "${NXD_GENERATED_CONTRACTS_DIR}/SignalValueF64.hpp"
    "${NXD_GENERATED_CONTRACTS_DIR}/SignalValueI32.hpp"
    "${NXD_GENERATED_CONTRACTS_DIR}/SignalValueI64.hpp"
    "${NXD_GENERATED_CONTRACTS_DIR}/SignalValueU32.hpp"
    "${NXD_GENERATED_CONTRACTS_DIR}/SignalValueU64.hpp")
add_custom_command(
    OUTPUT ${NXD_SIGNALVALUE_HEADERS}
    COMMAND cargo run -q -p nxdc -- emit-cpp "${NXD_CONTRACTS_DIR}/SignalValue.yaml" "${NXD_GENERATED_CONTRACTS_DIR}"
    DEPENDS "${NXD_CONTRACTS_DIR}/SignalValue.yaml"
    COMMENT "Generating C++ headers for SignalValue concrete instances"
    VERBATIM)

add_custom_target(nxd_generated_headers
    DEPENDS ${NXD_SINGLE_HEADERS} ${NXD_BATCH_HEADERS} ${NXD_SIGNALVALUE_HEADERS})

# -----------------------------------------------------------------------------
# Static library that validates every generated contract header.
# -----------------------------------------------------------------------------
# Each generated header is compiled in its own translation unit.  This lets the
# same helper type names (e.g. Direction, Kind) appear in several headers
# without colliding, and exercises every `static_assert` in the generated code.
set(NXD_VALIDATION_SOURCES)
foreach(header IN LISTS NXD_SINGLE_HEADERS NXD_BATCH_HEADERS NXD_SIGNALVALUE_HEADERS)
    get_filename_component(name "${header}" NAME_WE)
    set(src "${CMAKE_CURRENT_BINARY_DIR}/generated/cpp/validate_${name}.cpp")
    file(GENERATE OUTPUT "${src}" CONTENT "#include <cstddef>\n#include <nxd/contracts/${name}.hpp>\nnamespace nxd::contracts {\n    // Force the aggregate to be instantiated so the header's compile-time\n    // assertions are evaluated.\n    volatile std::size_t g_${name}_size = sizeof(${name});\n}\n")
    list(APPEND NXD_VALIDATION_SOURCES "${src}")
endforeach()

add_library(nxd_contracts STATIC ${NXD_VALIDATION_SOURCES})

add_dependencies(nxd_contracts nxd_generated_headers)

target_link_libraries(nxd_contracts PUBLIC nexigen::core)

target_include_directories(nxd_contracts PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
    $<BUILD_INTERFACE:${NXD_GENERATED_CPP_DIR}>
    $<INSTALL_INTERFACE:include>)

# Enforce the project-wide ABI constraints explicitly; the nexigen package also
# propagates them via its options target, but repeating them here documents the
# contract that generated headers must satisfy.
target_compile_options(nxd_contracts PUBLIC
    -fno-exceptions
    -fno-rtti)

# -----------------------------------------------------------------------------
# Example ABI-1 plugins
# -----------------------------------------------------------------------------
add_library(nxd_example_counter_cpp SHARED examples/components/cpp/counter/counter.cpp)
add_dependencies(nxd_example_counter_cpp nxd_abi_header)
target_include_directories(nxd_example_counter_cpp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_example_counter_cpp PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
set(NXD_ABI_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi/nxd_abi.h")
set(NXD_ABI_HPP "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include/nxd/abi.hpp")

add_custom_target(nxd_abi_header
    COMMAND cbindgen --crate nxd-abi --output "${NXD_ABI_HEADER}" --config "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi/cbindgen.toml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Regenerating ABI-1 C header with cbindgen"
    VERBATIM)

add_executable(nxd_abi_layout_test tests/abi_layout.cpp)
add_dependencies(nxd_abi_layout_test nxd_abi_header)
target_include_directories(nxd_abi_layout_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_abi_layout_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_abi_layout COMMAND nxd_abi_layout_test)

add_executable(nxd_abi_plugin_equivalence_test tests/abi_plugin_equivalence.cpp)
add_dependencies(nxd_abi_plugin_equivalence_test nxd_abi_header nxd_example_counter_cpp)
target_include_directories(nxd_abi_plugin_equivalence_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_link_libraries(nxd_abi_plugin_equivalence_test PRIVATE ${CMAKE_DL_LIBS})
target_compile_options(nxd_abi_plugin_equivalence_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

# Build Rust plugins with --release regardless of CMake build type.  This keeps
# the shared libraries free of the unwind runtime that Cargo's test profile
# would otherwise inject into the debug artifacts.
set(NXD_CARGO_PROFILE "--release")
set(NXD_RUST_COUNTER_SO "${CMAKE_CURRENT_SOURCE_DIR}/target/release/libnxd_example_counter_rust.so")
set(NXD_RUST_ALLOC_COUNTER_SO "${CMAKE_CURRENT_SOURCE_DIR}/target/release/libnxd_test_alloc_counter_rust.so")
set(NXD_RUST_PANIC_SO "${CMAKE_CURRENT_SOURCE_DIR}/target/release/libnxd_test_panic_rust.so")

add_custom_command(
    OUTPUT "${NXD_RUST_COUNTER_SO}"
    COMMAND cargo build -p nxd-example-counter-rust ${NXD_CARGO_PROFILE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building Rust counter example plugin"
    VERBATIM)

add_custom_target(nxd_example_counter_rust DEPENDS "${NXD_RUST_COUNTER_SO}")

add_test(NAME nxd_abi_plugin_equivalence
    COMMAND nxd_abi_plugin_equivalence_test
            "$<TARGET_FILE:nxd_example_counter_cpp>"
            "${NXD_RUST_COUNTER_SO}")
set_tests_properties(nxd_abi_plugin_equivalence PROPERTIES
    DEPENDS "nxd_example_counter_cpp;nxd_example_counter_rust")

# -----------------------------------------------------------------------------
# nxd-host runtime
# -----------------------------------------------------------------------------
add_library(nxd_host STATIC
    host/src/descriptor.cpp
    host/src/plugin.cpp
    host/src/component.cpp
    host/src/headless_driver.cpp
    host/src/harness.cpp
    host/src/thread_roles.cpp)
add_dependencies(nxd_host nxd_abi_header)
target_include_directories(nxd_host PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_link_libraries(nxd_host PUBLIC
    nexigen::app
    nexigen::core
    ${CMAKE_DL_LIBS})
target_compile_options(nxd_host PUBLIC
    -fno-exceptions
    -fno-rtti)

# Generated component descriptors consumed by the host.
set(NXD_COMPONENT_DESCRIPTORS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/descriptors")
file(MAKE_DIRECTORY "${NXD_COMPONENT_DESCRIPTORS_DIR}")

foreach(component IN ITEMS empty deterministic_counter)
    set(yaml "${CMAKE_CURRENT_SOURCE_DIR}/components/${component}.yaml")
    set(json "${NXD_COMPONENT_DESCRIPTORS_DIR}/${component}.descriptor.json")
    set(hash "${NXD_COMPONENT_DESCRIPTORS_DIR}/${component}.descriptor.hash")
    add_custom_command(
        OUTPUT "${json}" "${hash}"
        COMMAND cargo run -q -p nxdc -- emit-component-descriptor "${yaml}" "${json}"
        DEPENDS "${yaml}"
        COMMENT "Generating descriptor and hash for ${component}"
        VERBATIM)
    list(APPEND NXD_DESCRIPTOR_FILES "${json}" "${hash}")
endforeach()

add_custom_target(nxd_component_descriptors DEPENDS ${NXD_DESCRIPTOR_FILES})
add_dependencies(nxd_host nxd_component_descriptors)

# -----------------------------------------------------------------------------
# Empty example plugin
# -----------------------------------------------------------------------------
add_library(nxd_example_empty_cpp SHARED examples/components/cpp/empty/empty.cpp)
add_dependencies(nxd_example_empty_cpp nxd_abi_header)
target_include_directories(nxd_example_empty_cpp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_example_empty_cpp PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

# -----------------------------------------------------------------------------
# C++ test plugins
# -----------------------------------------------------------------------------
add_library(nxd_test_lifecycle_cpp SHARED tests/components/cpp/lifecycle/lifecycle.cpp)
add_dependencies(nxd_test_lifecycle_cpp nxd_abi_header)
target_include_directories(nxd_test_lifecycle_cpp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_lifecycle_cpp PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_library(nxd_test_allocating_cpp SHARED tests/components/cpp/allocating/allocating.cpp)
add_dependencies(nxd_test_allocating_cpp nxd_abi_header)
target_include_directories(nxd_test_allocating_cpp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_allocating_cpp PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

# -----------------------------------------------------------------------------
# Intentionally-invalid plugin variants for load validation tests
# -----------------------------------------------------------------------------
add_library(nxd_test_plugin_bad_abi_version SHARED tests/plugins/bad_plugin.cpp)
target_compile_definitions(nxd_test_plugin_bad_abi_version PRIVATE BAD_ABI_VERSION)
add_dependencies(nxd_test_plugin_bad_abi_version nxd_abi_header)
target_include_directories(nxd_test_plugin_bad_abi_version PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_plugin_bad_abi_version PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_library(nxd_test_plugin_bad_table_size SHARED tests/plugins/bad_plugin.cpp)
target_compile_definitions(nxd_test_plugin_bad_table_size PRIVATE BAD_TABLE_SIZE)
add_dependencies(nxd_test_plugin_bad_table_size nxd_abi_header)
target_include_directories(nxd_test_plugin_bad_table_size PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_plugin_bad_table_size PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_library(nxd_test_plugin_bad_fingerprint SHARED tests/plugins/bad_plugin.cpp)
target_compile_definitions(nxd_test_plugin_bad_fingerprint PRIVATE BAD_FINGERPRINT)
add_dependencies(nxd_test_plugin_bad_fingerprint nxd_abi_header)
target_include_directories(nxd_test_plugin_bad_fingerprint PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_plugin_bad_fingerprint PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_library(nxd_test_plugin_bad_descriptor_hash SHARED tests/plugins/bad_plugin.cpp)
target_compile_definitions(nxd_test_plugin_bad_descriptor_hash PRIVATE BAD_DESCRIPTOR_HASH)
add_dependencies(nxd_test_plugin_bad_descriptor_hash nxd_abi_header)
target_include_directories(nxd_test_plugin_bad_descriptor_hash PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_plugin_bad_descriptor_hash PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_library(nxd_test_plugin_bad_missing_fn SHARED tests/plugins/bad_plugin.cpp)
target_compile_definitions(nxd_test_plugin_bad_missing_fn PRIVATE BAD_MISSING_FN)
add_dependencies(nxd_test_plugin_bad_missing_fn nxd_abi_header)
target_include_directories(nxd_test_plugin_bad_missing_fn PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_plugin_bad_missing_fn PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_library(nxd_test_plugin_bad_extension SHARED tests/plugins/bad_plugin.cpp)
target_compile_definitions(nxd_test_plugin_bad_extension PRIVATE BAD_EXTENSION)
add_dependencies(nxd_test_plugin_bad_extension nxd_abi_header)
target_include_directories(nxd_test_plugin_bad_extension PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_test_plugin_bad_extension PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

# -----------------------------------------------------------------------------
# Rust test plugins
# -----------------------------------------------------------------------------
add_custom_command(
    OUTPUT "${NXD_RUST_ALLOC_COUNTER_SO}"
    COMMAND cargo build -p nxd-test-alloc-counter-rust ${NXD_CARGO_PROFILE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building Rust allocation-counter test plugin"
    VERBATIM)

add_custom_target(nxd_test_alloc_counter_rust DEPENDS "${NXD_RUST_ALLOC_COUNTER_SO}")

add_custom_command(
    OUTPUT "${NXD_RUST_PANIC_SO}"
    COMMAND cargo build -p nxd-test-panic-rust ${NXD_CARGO_PROFILE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building Rust panic test plugin"
    VERBATIM)

add_custom_target(nxd_test_panic_rust DEPENDS "${NXD_RUST_PANIC_SO}")

# -----------------------------------------------------------------------------
# Host-side correctness tests
# -----------------------------------------------------------------------------
add_executable(nxd_host_infrastructure_test tests/host_infrastructure_test.cpp)
target_link_libraries(nxd_host_infrastructure_test PRIVATE nxd_host)
target_include_directories(nxd_host_infrastructure_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_infrastructure_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_host_infrastructure COMMAND nxd_host_infrastructure_test)

add_executable(nxd_host_plugin_load_test tests/host_plugin_load_test.cpp)
target_link_libraries(nxd_host_plugin_load_test PRIVATE nxd_host)
target_include_directories(nxd_host_plugin_load_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_plugin_load_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_host_plugin_load
    COMMAND nxd_host_plugin_load_test
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/deterministic_counter.descriptor.json"
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/deterministic_counter.descriptor.hash"
            "$<TARGET_FILE:nxd_example_counter_cpp>"
            "$<TARGET_FILE:nxd_test_plugin_bad_abi_version>"
            "$<TARGET_FILE:nxd_test_plugin_bad_table_size>"
            "$<TARGET_FILE:nxd_test_plugin_bad_fingerprint>"
            "$<TARGET_FILE:nxd_test_plugin_bad_descriptor_hash>"
            "$<TARGET_FILE:nxd_test_plugin_bad_missing_fn>"
            "$<TARGET_FILE:nxd_test_plugin_bad_extension>")
set_tests_properties(nxd_host_plugin_load PROPERTIES
    DEPENDS "nxd_example_counter_cpp;nxd_test_plugin_bad_abi_version;nxd_test_plugin_bad_table_size;nxd_test_plugin_bad_fingerprint;nxd_test_plugin_bad_descriptor_hash;nxd_test_plugin_bad_missing_fn;nxd_test_plugin_bad_extension")

add_executable(nxd_host_component_lifecycle_test tests/host_component_lifecycle_test.cpp)
target_link_libraries(nxd_host_component_lifecycle_test PRIVATE nxd_host)
target_include_directories(nxd_host_component_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_component_lifecycle_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_host_component_lifecycle
    COMMAND nxd_host_component_lifecycle_test
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/deterministic_counter.descriptor.json"
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/deterministic_counter.descriptor.hash"
            "$<TARGET_FILE:nxd_test_lifecycle_cpp>")
set_tests_properties(nxd_host_component_lifecycle PROPERTIES DEPENDS nxd_test_lifecycle_cpp)

add_executable(nxd_host_thread_roles_test tests/host_thread_roles_test.cpp)
target_link_libraries(nxd_host_thread_roles_test PRIVATE nxd_host)
target_include_directories(nxd_host_thread_roles_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_thread_roles_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_host_thread_roles
    COMMAND nxd_host_thread_roles_test
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/deterministic_counter.descriptor.json")
set_tests_properties(nxd_host_thread_roles PROPERTIES DEPENDS nxd_component_descriptors)

add_executable(nxd_host_harness_test tests/host_harness_test.cpp)
target_link_libraries(nxd_host_harness_test PRIVATE nxd_host)
target_include_directories(nxd_host_harness_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_harness_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_host_harness
    COMMAND nxd_host_harness_test
            "$<TARGET_FILE:nxd_example_empty_cpp>"
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/empty.descriptor.json"
            "$<TARGET_FILE:nxd_example_counter_cpp>"
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/deterministic_counter.descriptor.json"
            "$<TARGET_FILE:nxd_test_allocating_cpp>")
set_tests_properties(nxd_host_harness PROPERTIES
    DEPENDS "nxd_example_empty_cpp;nxd_example_counter_cpp;nxd_test_allocating_cpp;nxd_component_descriptors")

# Child-process runner used by panic/failure-injection tests
add_executable(nxd_host_child_runner tests/host_child_runner.cpp)
target_link_libraries(nxd_host_child_runner PRIVATE nxd_host)
target_include_directories(nxd_host_child_runner PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_child_runner PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_executable(nxd_host_panic_failure_test tests/host_panic_failure_test.cpp)
target_link_libraries(nxd_host_panic_failure_test PRIVATE nxd_host)
target_include_directories(nxd_host_panic_failure_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_panic_failure_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_dependencies(nxd_host_panic_failure_test nxd_test_panic_rust nxd_test_alloc_counter_rust)
add_test(NAME nxd_host_panic_failure
    COMMAND nxd_host_panic_failure_test
            "$<TARGET_FILE:nxd_host_child_runner>"
            "${NXD_RUST_PANIC_SO}"
            "$<TARGET_FILE:nxd_test_allocating_cpp>"
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/deterministic_counter.descriptor.json")
set_tests_properties(nxd_host_panic_failure PROPERTIES
    DEPENDS "nxd_host_child_runner;nxd_test_panic_rust;nxd_test_allocating_cpp;nxd_component_descriptors")

# -----------------------------------------------------------------------------
# Empty-component ABI overhead benchmark
# -----------------------------------------------------------------------------
add_executable(nxd_host_empty_benchmark tests/host_empty_benchmark.cpp)
target_link_libraries(nxd_host_empty_benchmark PRIVATE nxd_host)
target_include_directories(nxd_host_empty_benchmark PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/rust/nxd-abi")
target_compile_options(nxd_host_empty_benchmark PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(nxd_host_empty_benchmark PRIVATE BENCHMARK_CONFIG="Release")
else()
    target_compile_definitions(nxd_host_empty_benchmark PRIVATE BENCHMARK_CONFIG="Debug")
endif()
add_test(NAME nxd_host_empty_benchmark
    COMMAND nxd_host_empty_benchmark
            "$<TARGET_FILE:nxd_example_empty_cpp>"
            "${NXD_COMPONENT_DESCRIPTORS_DIR}/empty.descriptor.json"
            100000)
set_tests_properties(nxd_host_empty_benchmark PROPERTIES
    DEPENDS "nxd_example_empty_cpp;nxd_component_descriptors")

