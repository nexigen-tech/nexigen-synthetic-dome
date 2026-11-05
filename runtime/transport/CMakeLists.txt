add_library(nxd_transport STATIC
    src/headless_transport.cpp
    src/iceoryx_transport.cpp
)

target_include_directories(nxd_transport PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

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

add_executable(nxd_transport_iceoryx_stub_test tests/iceoryx_transport_stub_test.cpp)
target_link_libraries(nxd_transport_iceoryx_stub_test PRIVATE nxd_transport)
target_compile_options(nxd_transport_iceoryx_stub_test PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)
add_test(NAME nxd_transport_iceoryx_stub COMMAND nxd_transport_iceoryx_stub_test)
