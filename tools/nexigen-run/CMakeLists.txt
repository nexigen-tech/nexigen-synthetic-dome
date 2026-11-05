# NXD walking-skeleton runner.
#
# Loads a Composer-generated Dome Plan and drives one deterministic frame
# through Chronos while managing the declared child processes.

add_executable(nexigen-run
    src/main.cpp)

add_executable(nxd-ws01-echo
    tests/fixtures/echo_component.cpp)

target_link_libraries(nexigen-run PRIVATE
    nxd_composer
    nxd_chronos)

target_include_directories(nexigen-run PRIVATE
    "${CMAKE_SOURCE_DIR}/runtime/composer/include"
    "${CMAKE_SOURCE_DIR}/runtime/chronos/include")

target_compile_options(nexigen-run PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

set_target_properties(nexigen-run PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

set_target_properties(nxd-ws01-echo PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Walking-skeleton integration test
# -----------------------------------------------------------------------------
# Copy the committed nxd-ws01 plan and its fixture executable into a single
# directory so the relative executable path in the plan resolves correctly.
set(NXD_WS01_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
file(MAKE_DIRECTORY "${NXD_WS01_FIXTURE_DIR}")

add_custom_command(TARGET nxd-ws01-echo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:nxd-ws01-echo>" "${NXD_WS01_FIXTURE_DIR}/nxd-ws01-echo"
    COMMENT "Copying nxd-ws01-echo fixture"
    VERBATIM)

add_custom_command(TARGET nexigen-run POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/nxd-ws01.json" "${NXD_WS01_FIXTURE_DIR}/nxd-ws01.json"
    COMMENT "Copying committed nxd-ws01 plan"
    VERBATIM)

add_test(NAME nxd_nexigen_run_walking_skeleton
    COMMAND nexigen-run "${NXD_WS01_FIXTURE_DIR}/nxd-ws01.json"
    WORKING_DIRECTORY "${NXD_WS01_FIXTURE_DIR}")
