#ifndef XDEMO_SHIM_H
#define XDEMO_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xd_handle { void* p; };

/* Transport lifecycle */
int  xd_init(struct xd_handle* h);
void xd_fini(struct xd_handle* h);

/* Subscriber */
int  xd_sub(struct xd_handle h, const char* name, struct xd_handle* out);
int  xd_take(struct xd_handle s, void* buf, uintptr_t* esize);
void xd_sub_drop(struct xd_handle* s);

/* Publisher */
int  xd_pub(struct xd_handle h, const char* name, struct xd_handle* out);
int  xd_send(struct xd_handle p, const void* data, uintptr_t sz);
void xd_pub_drop(struct xd_handle* p);

#ifdef __cplusplus
}
#endif

#endif /* XDEMO_SHIM_H */