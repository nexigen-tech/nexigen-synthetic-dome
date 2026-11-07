/* Minimal standalone C wrapper around iceoryx2 C FFI.
 *
 * Does NOT depend on nxd_transport. Uses only the iceoryx2 C FFI library.
 * Compiled as a shared library with -fPIC for loading from Rust via dlopen.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* iceoryx2 C FFI header — found at build/rust/.../iceoryx2-ffi-c-cbindgen/include/ */
#include "iox2/iceoryx2.h"

/* ── Opaque handles ──────────────────────────────────────────────────────── */
typedef struct { iox2_node_h inner; }        node_t;
typedef struct { iox2_publisher_h inner; }   pub_t;
typedef struct { iox2_subscriber_h inner; }  sub_t;

/* ── Error codes ─────────────────────────────────────────────────────────── */
#define OK   0
#define ERR  (-1)

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static int build_pubsub(iox2_node_h node, const char* name,
                         iox2_service_builder_pub_sub_h* out) {
    iox2_service_name_ptr_t n;
    iox2_cast_service_name_ptr(name, strlen(name), &n);

    iox2_service_builder_h svc = NULL;
    iox2_service_builder_create(node, &n, &svc);
    if (!svc) return ERR;

    iox2_service_builder_pub_sub_h ps = NULL;
    iox2_service_builder_pub_sub(svc, &ps);
    if (!ps) { iox2_service_builder_drop(svc); return ERR; }

    iox2_service_builder_open_or_create(ps, &ps);
    if (!ps) return ERR;

    *out = ps;
    return OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Exported API                                                              */
/* ═══════════════════════════════════════════════════════════════════════════ */

int xdemo_node_create(node_t* out) {
    iox2_node_h n = NULL;
    int rc = iox2_node_builder_create("xdemo", &n);
    if (rc != 0 || n == NULL) return ERR;
    out->inner = n;
    return OK;
}

void xdemo_node_drop(node_t* n) {
    if (n->inner) { iox2_node_drop(n->inner); n->inner = NULL; }
}

/* ── Publisher ────────────────────────────────────────────────────────────── */

int xdemo_pub_create(node_t node, const char* name, pub_t* out) {
    iox2_service_builder_pub_sub_h ps = NULL;
    if (build_pubsub(node.inner, name, &ps) != OK) return ERR;

    iox2_publisher_h p = NULL;
    iox2_publisher_builder_create(ps, &p);
    if (!p) return ERR;
    iox2_publisher_builder_max_borrow_count(p, 1, &p);
    iox2_publisher_builder_max_loan_count(p, 1, &p);
    iox2_publisher_builder_create_publisher(p, &p);
    if (!p) return ERR;
    out->inner = p;
    return OK;
}

int xdemo_pub_loan_send(pub_t pub, const void* data, uintptr_t sz) {
    iox2_sample_mut_t s = NULL;
    iox2_publisher_loan_slice_uninit(pub.inner, 1, &s);
    if (!s) return ERR;

    uint8_t* pay = NULL; uintptr_t plen = 0;
    iox2_sample_mut_payload(s, (void**)&pay, &plen);
    if (pay && data && sz <= plen) memcpy(pay, data, sz);

    /* Set user-header with frame_no etc */
    iox2_sample_mut_send(s);
    return OK;
}

void xdemo_pub_drop(pub_t* p) {
    if (p->inner) { iox2_publisher_drop(p->inner); p->inner = NULL; }
}

/* ── Subscriber ───────────────────────────────────────────────────────────── */

int xdemo_sub_create(node_t node, const char* name, sub_t* out) {
    iox2_service_builder_pub_sub_h ps = NULL;
    if (build_pubsub(node.inner, name, &ps) != OK) return ERR;

    iox2_subscriber_h s = NULL;
    iox2_subscriber_builder_create(ps, &s);
    if (!s) return ERR;
    iox2_subscriber_builder_create_subscriber(s, &s);
    if (!s) return ERR;
    out->inner = s;
    return OK;
}

int xdemo_sub_take(sub_t sub, void* buf, uintptr_t* sz) {
    iox2_sample_t s = NULL;
    iox2_subscriber_receive(sub.inner, &s);
    if (!s) return ERR; /* no data */

    const uint8_t* pay = NULL; uintptr_t plen = 0;
    iox2_sample_payload(s, (const void**)&pay, &plen);
    if (pay && buf && *sz >= plen) { memcpy(buf, pay, plen); *sz = plen; }
    iox2_sample_drop(s);
    return OK;
}

void xdemo_sub_drop(sub_t* s) {
    if (s->inner) { iox2_subscriber_drop(s->inner); s->inner = NULL; }
}