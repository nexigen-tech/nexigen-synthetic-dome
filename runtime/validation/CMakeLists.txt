# Validation runtime service.
add_library(nxd_validation STATIC
    src/validation.cpp)

add_dependencies(nxd_validation nxd_generated_headers)

target_include_directories(nxd_validation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${NXD_GENERATED_CPP_DIR}")

target_link_libraries(nxd_validation PUBLIC
    nxd_transport)

target_compile_features(nxd_validation PUBLIC cxx_std_23)
target_compile_options(nxd_validation PUBLIC
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_executable(nxd_validation_service_test tests/validation_service_test.cpp)
add_dependencies(nxd_validation_service_test nxd_generated_headers)
target_link_libraries(nxd_validation_service_test PRIVATE
    nxd_validation
    nxd_blackboard)
target_include_directories(nxd_validation_service_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../blackboard/include"
    "${NXD_GENERATED_CPP_DIR}")
target_compile_options(nxd_validation_service_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_validation_service COMMAND nxd_validation_service_test)

add_executable(nxd_validation_record_replay_validate_test tests/record_replay_validate_test.cpp)
add_dependencies(nxd_validation_record_replay_validate_test nxd_generated_headers)
target_link_libraries(nxd_validation_record_replay_validate_test PRIVATE
    nxd_validation
    nxd_recording
    nxd_transport
    nxd_blackboard)
target_include_directories(nxd_validation_record_replay_validate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../recording/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../blackboard/include"
    "${NXD_GENERATED_CPP_DIR}")
target_compile_options(nxd_validation_record_replay_validate_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_validation_record_replay_validate COMMAND nxd_validation_record_replay_validate_test)
