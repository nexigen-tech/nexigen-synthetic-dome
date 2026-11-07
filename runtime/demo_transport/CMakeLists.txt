# C++ shim for Rust FFI — wraps IceoryxTransport in extern "C" functions.
# Compiled as a STATIC library linked into Rust executables via build.rs.
# Does NOT need -fPIC because it's linked into an executable, not a shared lib.

add_library(xdemo_shim STATIC xdemo_shim.cpp)
target_include_directories(xdemo_shim PRIVATE
    "${CMAKE_SOURCE_DIR}/runtime/transport/include"
    "${CMAKE_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_SOURCE_DIR}/sdk/rust/nxd-abi"
    "${NXD_GENERATED_CPP_DIR}")
target_link_libraries(xdemo_shim PUBLIC nxd_transport)
target_compile_options(xdemo_shim PRIVATE -Wall -Wextra -Werror)
target_compile_features(xdemo_shim PUBLIC cxx_std_23)