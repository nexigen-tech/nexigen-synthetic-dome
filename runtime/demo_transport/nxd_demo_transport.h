#ifndef NXD_DEMO_TRANSPORT_H
#define NXD_DEMO_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handles */
typedef struct { void* inner; } NxdDemoNode;
typedef struct { void* inner; } NxdDemoPublisher;
typedef struct { void* inner; } NxdDemoSubscriber;

/* Error codes */
#define NXD_DEMO_OK       0
#define NXD_DEMO_ERR_NODE  (-1)
#define NXD_DEMO_ERR_SVC   (-2)
#define NXD_DEMO_ERR_PUB   (-3)
#define NXD_DEMO_ERR_SUB   (-4)
#define NXD_DEMO_ERR_LOAN  (-5)
#define NXD_DEMO_ERR_TAKE  (-6)

/* Node lifecycle */
int  nxd_demo_node_create(NxdDemoNode* out);
void nxd_demo_node_drop(NxdDemoNode* node);

/* Publisher */
int  nxd_demo_publisher_create(NxdDemoNode node, const char* service_name,
                                NxdDemoPublisher* out);
int  nxd_demo_publish(NxdDemoPublisher pub, const void* data,
                       uintptr_t element_size);
void nxd_demo_publisher_drop(NxdDemoPublisher* pub);

/* Subscriber */
int  nxd_demo_subscriber_create(NxdDemoNode node, const char* service_name,
                                 NxdDemoSubscriber* out);
int  nxd_demo_subscriber_take(NxdDemoSubscriber sub, void* buffer,
                               uintptr_t* element_size);
void nxd_demo_subscriber_drop(NxdDemoSubscriber* sub);

#ifdef __cplusplus
}
#endif

#endif /* NXD_DEMO_TRANSPORT_H */