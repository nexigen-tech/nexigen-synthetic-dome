/* NXD plugin ABI-1 C++ wrapper.
 *
 * This header includes the cbindgen-generated C ABI declaration and adds
 * C++-specific conveniences.  It is valid under C++23 with -fno-exceptions
 * and -fno-rtti.
 */

#ifndef NXD_ABI_HPP
#define NXD_ABI_HPP

#include <cstddef>

extern "C" {
#include "nxd_abi.h"
}

namespace nxd {

inline constexpr std::size_t abi_table_size_v = sizeof(NxdAbiTable);

} // namespace nxd

#endif // NXD_ABI_HPP
