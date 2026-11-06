# NXD walking-skeleton runner.
#
# Loads a Composer-generated Dome Plan and drives one deterministic frame
# through Chronos while managing the declared child processes.

add_executable(nexigen-run
    src/main.cpp)

add_executable(nxd-ws01-echo
    tests/fixtures/echo_component.cpp)

add_dependencies(nexigen-run nxd_generated_headers nxd-ws01-replay-fixture-gen)

target_link_libraries(nexigen-run PRIVATE
    nxd_composer
    nxd_chronos
    nxd_recording
    nxd_validation)

target_include_directories(nexigen-run PRIVATE
    "${CMAKE_SOURCE_DIR}/runtime/composer/include"
    "${CMAKE_SOURCE_DIR}/runtime/chronos/include"
    "${CMAKE_SOURCE_DIR}/runtime/recording/include"
    "${CMAKE_SOURCE_DIR}/runtime/validation/include"
    "${CMAKE_SOURCE_DIR}/runtime/transport/include"
    "${NXD_GENERATED_CPP_DIR}")

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
# Fixture generator: produces a tiny MCAP file used by the services test.
# -----------------------------------------------------------------------------
add_executable(nxd-ws01-replay-fixture-gen
    tests/fixtures/generate_replay_fixture.cpp)

target_link_libraries(nxd-ws01-replay-fixture-gen PRIVATE nxd_recording)

target_include_directories(nxd-ws01-replay-fixture-gen PRIVATE
    "${CMAKE_SOURCE_DIR}/runtime/recording/include"
    "${CMAKE_SOURCE_DIR}/runtime/transport/include"
    "${NXD_GENERATED_CPP_DIR}")

target_compile_options(nxd-ws01-replay-fixture-gen PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

set_target_properties(nxd-ws01-replay-fixture-gen PROPERTIES
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
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/nxd-ws01-services.json" "${NXD_WS01_FIXTURE_DIR}/nxd-ws01-services.json"
    COMMENT "Copying committed nxd-ws01 plans"
    VERBATIM)

add_custom_command(TARGET nxd-ws01-replay-fixture-gen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NXD_WS01_FIXTURE_DIR}"
    COMMAND "$<TARGET_FILE:nxd-ws01-replay-fixture-gen>" "${NXD_WS01_FIXTURE_DIR}/replay.mcap"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/nxd-ws01-services.json" "${NXD_WS01_FIXTURE_DIR}/nxd-ws01-services.json"
    COMMENT "Generating replay fixture and copying services plan"
    VERBATIM)

add_test(NAME nxd_nexigen_run_walking_skeleton
    COMMAND nexigen-run "${NXD_WS01_FIXTURE_DIR}/nxd-ws01.json"
    WORKING_DIRECTORY "${NXD_WS01_FIXTURE_DIR}")

add_test(NAME nxd_nexigen_run_services
    COMMAND nexigen-run "${NXD_WS01_FIXTURE_DIR}/nxd-ws01-services.json"
    WORKING_DIRECTORY "${NXD_WS01_FIXTURE_DIR}")
set_tests_properties(nxd_nexigen_run_services PROPERTIES
    DEPENDS "nxd-ws01-replay-fixture-gen")
