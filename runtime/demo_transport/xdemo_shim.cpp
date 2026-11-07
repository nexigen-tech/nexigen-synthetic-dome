// C++ shim for Rust FFI — wraps IceoryxTransport in extern "C" functions.
#include <array>
#include <cstdint>
#include <cstring>
#include <new>

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/runtime/transport/iceoryx_transport.hpp"

namespace tx = nxd::runtime::transport;
using nxd::contracts::RawCanBatchClassical;

static const std::array<std::uint8_t, 32> kHash{};

extern "C" {

struct xd_handle { void* p; };

int xd_init(xd_handle* h) {
    auto* t = new (std::nothrow) tx::IceoryxTransport();
    if (!t) return -1;
    h->p = t; return 0;
}
void xd_fini(xd_handle* h) {
    delete static_cast<tx::IceoryxTransport*>(h->p);
    h->p = 0;
}

int xd_pub(xd_handle h, const char* name, xd_handle* p) {
    auto* t = static_cast<tx::IceoryxTransport*>(h.p);
    auto* pub = t->create_publisher(name, sizeof(RawCanBatchClassical), 1, kHash);
    if (!pub) return -1;
    p->p = pub; return 0;
}

int xd_send(xd_handle p, const void* data, uintptr_t sz) {
    auto* pub = static_cast<tx::Publisher*>(p.p);
    auto loan = pub->loan();
    if (!loan.valid()) return -1;
    if (loan.data && data && loan.element_size >= sz)
        std::memcpy(loan.data, data, sz);
    tx::SampleMetadata m{};
    pub->publish(m);
    return 0;
}
void xd_pub_drop(xd_handle* p) { p->p = 0; }

int xd_sub(xd_handle h, const char* name, xd_handle* s) {
    auto* t = static_cast<tx::IceoryxTransport*>(h.p);
    auto* sub = t->create_subscriber(name, sizeof(RawCanBatchClassical), 1, kHash);
    if (!sub) return -1;
    s->p = sub; return 0;
}

int xd_take(xd_handle s, void* buf, uintptr_t* esize) {
    auto* sub = static_cast<tx::Subscriber*>(s.p);
    auto v = sub->take();
    if (!v.valid()) return -1;
    if (v.data && buf && *esize >= v.element_size * v.count) {
        std::memcpy(buf, v.data, v.element_size * v.count);
        *esize = v.element_size * v.count;
    }
    return 0;
}
void xd_sub_drop(xd_handle* s) { s->p = 0; }

} // extern "C"