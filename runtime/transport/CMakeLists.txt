add_library(nxd_transport STATIC
    src/headless_transport.cpp
    src/iceoryx_transport.cpp
)

add_dependencies(nxd_transport nxd_generated_headers)

target_include_directories(nxd_transport PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${NXD_GENERATED_CPP_DIR}"
)

target_link_libraries(nxd_transport PUBLIC iceoryx2-c::static-lib)

target_compile_features(nxd_transport PUBLIC cxx_std_23)
target_compile_options(nxd_transport PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

add_executable(nxd_transport_headless_test tests/headless_transport_test.cpp)
target_link_libraries(nxd_transport_headless_test PRIVATE nxd_transport)
target_compile_options(nxd_transport_headless_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_transport_headless COMMAND nxd_transport_headless_test)

add_executable(nxd_transport_iceoryx_smoke_test tests/iceoryx_transport_smoke_test.cpp)
target_link_libraries(nxd_transport_iceoryx_smoke_test PRIVATE nxd_transport)
target_compile_options(nxd_transport_iceoryx_smoke_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_transport_iceoryx_smoke COMMAND nxd_transport_iceoryx_smoke_test)

add_executable(nxd_transport_iceoryx_fork_test tests/iceoryx_transport_fork_test.cpp)
target_link_libraries(nxd_transport_iceoryx_fork_test PRIVATE nxd_transport)
target_compile_options(nxd_transport_iceoryx_fork_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_transport_iceoryx_fork COMMAND nxd_transport_iceoryx_fork_test)
