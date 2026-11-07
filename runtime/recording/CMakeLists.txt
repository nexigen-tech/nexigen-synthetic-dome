# Recording, replay, and validation runtime services.
add_library(nxd_recording STATIC
    src/mcap.cpp
    src/recorder.cpp
    src/replay.cpp)

add_dependencies(nxd_recording nxd_generated_headers)

target_include_directories(nxd_recording PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../blackboard/include"
    "${NXD_GENERATED_CPP_DIR}")

target_link_libraries(nxd_recording PUBLIC
    nxd_transport
    nxd_blackboard)

target_compile_features(nxd_recording PUBLIC cxx_std_23)
target_compile_options(nxd_recording PUBLIC
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_executable(nxd_recording_mcap_test tests/mcap_test.cpp)
target_link_libraries(nxd_recording_mcap_test PRIVATE nxd_recording)
target_include_directories(nxd_recording_mcap_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${NXD_GENERATED_CPP_DIR}")
target_compile_options(nxd_recording_mcap_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_recording_mcap COMMAND nxd_recording_mcap_test)

add_executable(nxd_recording_mcap_reader_helpers_test tests/mcap_reader_helpers_test.cpp)
target_link_libraries(nxd_recording_mcap_reader_helpers_test PRIVATE nxd_recording)
target_include_directories(nxd_recording_mcap_reader_helpers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${NXD_GENERATED_CPP_DIR}")
target_compile_options(nxd_recording_mcap_reader_helpers_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_recording_mcap_reader_helpers COMMAND nxd_recording_mcap_reader_helpers_test)

add_executable(nxd_recording_mcap_writer_helpers_test tests/mcap_writer_helpers_test.cpp)
target_link_libraries(nxd_recording_mcap_writer_helpers_test PRIVATE nxd_recording)
target_include_directories(nxd_recording_mcap_writer_helpers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${NXD_GENERATED_CPP_DIR}")
target_compile_options(nxd_recording_mcap_writer_helpers_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_recording_mcap_writer_helpers COMMAND nxd_recording_mcap_writer_helpers_test)

add_executable(nxd_recording_recorder_replay_test tests/recorder_replay_test.cpp)
add_dependencies(nxd_recording_recorder_replay_test nxd_generated_headers)
target_link_libraries(nxd_recording_recorder_replay_test PRIVATE
    nxd_recording
    nxd_blackboard)
target_include_directories(nxd_recording_recorder_replay_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../blackboard/include"
    "${NXD_GENERATED_CPP_DIR}")
target_compile_options(nxd_recording_recorder_replay_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_recording_recorder_replay COMMAND nxd_recording_recorder_replay_test)

