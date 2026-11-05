# Composer runtime library.
#
# Declarative composition layer: validates system manifests and emits Dome
# Plans consumed by `nexigen-run`.

add_library(nxd_composer STATIC
    src/plan.cpp
    src/validation.cpp
    src/manifest.cpp
    src/descriptor_validation.cpp
    src/plan_emission.cpp)

target_include_directories(nxd_composer PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")

target_link_libraries(nxd_composer PUBLIC
    nxd_host
    nexigen::core)

target_compile_options(nxd_composer PUBLIC
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

set_target_properties(nxd_composer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Dome Plan serialization test.
# -----------------------------------------------------------------------------
add_executable(nxd_composer_plan_test tests/plan_test.cpp)

target_link_libraries(nxd_composer_plan_test PRIVATE nxd_composer)

target_include_directories(nxd_composer_plan_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_options(nxd_composer_plan_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_composer_plan COMMAND nxd_composer_plan_test)

add_executable(nxd_composer_validation_test tests/validation_test.cpp)

target_link_libraries(nxd_composer_validation_test PRIVATE nxd_composer)

target_include_directories(nxd_composer_validation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_options(nxd_composer_validation_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_composer_validation COMMAND nxd_composer_validation_test)

add_executable(nxd_composer_source_name_test tests/source_name_validation_test.cpp)

target_link_libraries(nxd_composer_source_name_test PRIVATE nxd_composer)

target_include_directories(nxd_composer_source_name_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_options(nxd_composer_source_name_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_composer_source_name
    COMMAND nxd_composer_source_name_test
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/virtual_can_can_a.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/virtual_can_isolation.json")

add_executable(nxd_composer_manifest_test tests/manifest_test.cpp)

target_link_libraries(nxd_composer_manifest_test PRIVATE nxd_composer)

target_include_directories(nxd_composer_manifest_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_options(nxd_composer_manifest_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_composer_manifest COMMAND nxd_composer_manifest_test)

add_executable(nxd_composer_plan_emission_test tests/plan_emission_test.cpp)

target_link_libraries(nxd_composer_plan_emission_test PRIVATE nxd_composer)

target_include_directories(nxd_composer_plan_emission_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/host/include")

target_compile_options(nxd_composer_plan_emission_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_composer_plan_emission COMMAND nxd_composer_plan_emission_test)

# -----------------------------------------------------------------------------
# Descriptor validation test.
# -----------------------------------------------------------------------------
add_executable(nxd_composer_descriptor_validation_test tests/descriptor_validation_test.cpp)

target_link_libraries(nxd_composer_descriptor_validation_test PRIVATE nxd_composer)

target_include_directories(nxd_composer_descriptor_validation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/host/include"
    "${CMAKE_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_SOURCE_DIR}/sdk/rust/nxd-abi")

target_compile_options(nxd_composer_descriptor_validation_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_test(NAME nxd_composer_descriptor_validation COMMAND nxd_composer_descriptor_validation_test)
