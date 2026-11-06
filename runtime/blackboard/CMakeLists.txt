# Blackboard runtime library.
#
# Slow shared-state service with single-writer entries, revision counters,
# apply_frame_no, and BlackboardUpdate events.  The C API is exposed so that
# both C++ and Rust components can share the same in-process Blackboard
# instance.

add_library(nxd_blackboard SHARED
    src/blackboard.cpp)

target_include_directories(nxd_blackboard PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")

target_compile_features(nxd_blackboard PUBLIC cxx_std_23)

target_compile_options(nxd_blackboard PUBLIC
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

# TSan cannot reason about std::atomic_thread_fence; suppress the resulting
# warning on the seqlock implementation until it is rewritten with explicit
# atomic load/store ordering.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_FLAGS MATCHES "-fsanitize=thread")
    target_compile_options(nxd_blackboard PRIVATE -Wno-tsan)
endif()

set_target_properties(nxd_blackboard PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Layout verification and functional test for the headless backend.
# -----------------------------------------------------------------------------
add_executable(nxd_blackboard_layout_test tests/blackboard_layout_test.cpp)
add_dependencies(nxd_blackboard_layout_test nxd_generated_headers)

target_link_libraries(nxd_blackboard_layout_test PRIVATE nxd_blackboard)

target_include_directories(nxd_blackboard_layout_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${NXD_GENERATED_CPP_DIR}")

target_compile_options(nxd_blackboard_layout_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_blackboard_layout COMMAND nxd_blackboard_layout_test)
