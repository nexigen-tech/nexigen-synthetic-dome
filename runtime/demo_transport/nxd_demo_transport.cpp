// C++ wrapper around iceoryx2 transport for Rust FFI.
// Compiled as a shared library; Rust calls extern "C" functions.

#include <array>
#include <cstdint>
#include <cstring>
#include <new>

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/runtime/transport/iceoryx_transport.hpp"

namespace tx = nxd::runtime::transport;

// ── Opaque handles (C-compatible) ──────────────────────────────────────────

extern "C" {

struct NxdDemoNode { void* _r; };
struct NxdDemoPub  { void* _r; };
struct NxdDemoSub  { void* _r; };

static const std::array<std::uint8_t, 32> kLayoutHash{};

// ── Transport lifecycle ─────────────────────────────────────────────────────

int nxd_demo_transport_init(NxdDemoNode* out) {
    auto* t = new (std::nothrow) tx::IceoryxTransport();
    if (!t) return -1;
    out->_r = t;
    return 0;
}

void nxd_demo_transport_fini(NxdDemoNode* node) {
    delete static_cast<tx::IceoryxTransport*>(node->_r);
    node->_r = nullptr;
}

// ── Publisher ───────────────────────────────────────────────────────────────

int nxd_demo_publisher_create(NxdDemoNode node, const char* name,
                               NxdDemoPub* out) {
    auto* t = static_cast<tx::IceoryxTransport*>(node._r);
    auto* pub = t->create_publisher(name, sizeof(nxd::contracts::RawCanBatchClassical), 1, kLayoutHash);
    if (!pub) return -1;
    out->_r = pub;
    return 0;
}

int nxd_demo_publisher_loan_send(NxdDemoPub pub, const void* data,
                                  uintptr_t element_size) {
    auto* p = static_cast<tx::Publisher*>(pub._r);
    auto loan = p->loan();
    if (!loan.valid()) return -1;
    if (loan.data && data && loan.element_size >= element_size)
        std::memcpy(loan.data, data, element_size);
    tx::SampleMetadata meta{};
    p->publish(meta);
    return 0;
}

void nxd_demo_publisher_drop(NxdDemoPub*) {}
// Publisher deleted by transport destructor

// ── Subscriber ──────────────────────────────────────────────────────────────

int nxd_demo_subscriber_create(NxdDemoNode node, const char* name,
                                NxdDemoSub* out) {
    auto* t = static_cast<tx::IceoryxTransport*>(node._r);
    auto* sub = t->create_subscriber(name, sizeof(nxd::contracts::RawCanBatchClassical), 1, kLayoutHash);
    if (!sub) return -1;
    out->_r = sub;
    return 0;
}

int nxd_demo_subscriber_take(NxdDemoSub sub, void* buf, uintptr_t* esize) {
    auto* s = static_cast<tx::Subscriber*>(sub._r);
    auto view = s->take();
    if (!view.valid()) return -1;
    if (view.data && buf && *esize >= view.element_size * view.count) {
        std::memcpy(buf, view.data, view.element_size * view.count);
        *esize = view.element_size * view.count;
    }
    return 0;
}

void nxd_demo_subscriber_drop(NxdDemoSub*) {}
// Subscriber deleted by transport destructor

} // extern "C"