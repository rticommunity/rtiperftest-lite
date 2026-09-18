/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Middleware implementation for RTI Connext DDS Micro 4.x.
 *
 * Implements the abstract middleware interface defined in
 * middleware/middleware_interface.h, mapping it to Micro's C API.
 *
 * Discovery plugin: DPSE is the default and takes precedence over RTI_DPDE.
 * With RTI_DPSE, every remote participant and endpoint is asserted explicitly
 * for a fixed, two-process publisher/subscriber topology.
 *
 * UDPv4 is always available. Serialized SHMEM and Zero Copy are optional
 * capabilities selected at runtime when compiled into the image.
 */
#include "../core/perftest_lite_config.h"
#include "middleware_interface.h"
#include "../os/os_interface.h"

#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
#include "memory_checkpoint.h"
#endif

#include "rti_me_c.h"
#include "osapi/osapi_heap.h"
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
#include "rti_me_psl/osapi/osapi_heap_test.h"
#endif
#include "wh_sm/wh_sm_history.h"
#include "rh_sm/rh_sm_history.h"
#if !defined(RTI_DPSE) && !defined(RTI_DPDE)
#define RTI_DPSE
#endif

#if defined(RTI_DPSE)
#include "disc_dpse/disc_dpse_dpsediscovery.h"
#else
#include "disc_dpde/disc_dpde_discovery_plugin.h"
#endif
#include "netio/netio_udp.h"
#ifdef PERFTEST_LITE_HAS_MICRO_SHMEM
#include "netio_shmem/netio_shmem.h"
#endif
#ifdef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
#include "netio_zcopy/netio_zcopy.h"
#include "netio_zcopy/netio_zcopy_default_notif_mech.h"
#endif

#include "perftest.h"
#include "perftestPlugin.h"
#include "perftestSupport.h"

#include <stdio.h>
#include <string.h>

#define PLITE_LOG(...) PERFTEST_LITE_PRINT(__VA_ARGS__)

typedef struct {
    PerftestLiteMiddleware                base;  /* MUST be first */
    const PerftestLiteMiddlewareConfig   *cfg;
    DDS_DomainParticipantFactory         *factory;
    DDS_DomainParticipant                *participant;
    DDS_Publisher                        *publisher;
    DDS_Subscriber                       *subscriber;
    DDS_Topic                            *ping_topic;
    DDS_Topic                            *pong_topic;
    int                                   matched_pubs;
    int                                   matched_subs;
    int                                   is_zerocopy;
    int                                   is_shmem;
} MicroMW;

typedef struct {
    PerftestLiteWriter base;
    DDS_DataWriter    *dw;
    PerftestTypeDataWriter *tdw;
    const PerftestLiteTypeInterface *type_iface;
} MicroWriter;

typedef struct {
    PerftestLiteReader base;
    DDS_DataReader    *dr;
    PerftestTypeDataReader *tdr;
    const PerftestLiteTypeInterface *type_iface;
    PerftestLiteListener listener;
    MicroMW *owner;
} MicroReader;

/* -------------------------------------------------------------------- */
/*  Listener trampolines                                                */
/* -------------------------------------------------------------------- */

/* Reserved for middlewares that report writer-side matches separately
 * (Pro, etc.). Currently unused with Micro. */
__attribute__((unused))
static void on_pub_matched(void *l, DDS_DataWriter *w,
                           const struct DDS_PublicationMatchedStatus *s)
{
    (void)w;
    MicroReader *self = (MicroReader *)l; /* shared by writer/reader */
    if (!self || !self->listener.on_publication_matched) return;
    self->listener.on_publication_matched(self->listener.user_data,
                                          s->current_count_change);
}

static void on_sub_matched(void *l, DDS_DataReader *r,
                           const struct DDS_SubscriptionMatchedStatus *s)
{
    (void)r;
    MicroReader *self = (MicroReader *)l;
    if (!self || !self->listener.on_subscription_matched) return;
    self->listener.on_subscription_matched(self->listener.user_data,
                                           s->current_count_change);
}

/* The reader-side `take` is invoked by the engine from on_data_available,
 * and pushes each sample into a user-supplied callback. The middleware
 * owns the loaned storage. */
typedef struct {
    void (*cb)(void *user, PerftestSample *sample);
    void *user;
    int   count;
    const PerftestLiteTypeInterface *type_iface;
} TakeCtx;

/* PerftestSample wraps a PerftestType*; we cast through. */
static int micro_reader_take(PerftestLiteReader *base,
                             void (*cb)(void *user, PerftestSample *sample),
                             void *user)
{
    MicroReader *r = (MicroReader *)base;
    struct DDS_SampleInfoSeq info_seq = DDS_SEQUENCE_INITIALIZER;
    struct PerftestTypeSeq    sample_seq = DDS_SEQUENCE_INITIALIZER;
    DDS_ReturnCode_t rc;
    int taken = 0;
    int i;

    rc = PerftestTypeDataReader_take(r->tdr, &sample_seq, &info_seq,
            DDS_LENGTH_UNLIMITED, DDS_ANY_SAMPLE_STATE,
            DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
    if (rc != DDS_RETCODE_OK) {
        return 0;
    }

    for (i = 0; i < PerftestTypeSeq_get_length(&sample_seq); ++i) {
        struct DDS_SampleInfo *info = DDS_SampleInfoSeq_get_reference(&info_seq, i);
        if (!info->valid_data) continue;
        PerftestType *raw = PerftestTypeSeq_get_reference(&sample_seq, i);
        cb(user, (PerftestSample *)raw);
        taken++;
    }

    PerftestTypeDataReader_return_loan(r->tdr, &sample_seq, &info_seq);
    PerftestTypeSeq_finalize(&sample_seq);
    DDS_SampleInfoSeq_finalize(&info_seq);
    return taken;
}

static void on_data_available_cb(void *l, DDS_DataReader *dr)
{
    (void)dr;
    MicroReader *r = (MicroReader *)l;
    if (!r->listener.on_data_available) return;
    r->listener.on_data_available(r->listener.user_data, &r->base);
}

/* -------------------------------------------------------------------- */
/*  Writer impl                                                         */
/* -------------------------------------------------------------------- */

static int micro_writer_write(PerftestLiteWriter *base, PerftestSample *sample)
{
    MicroWriter *w = (MicroWriter *)base;
    PerftestType *raw = (PerftestType *)w->type_iface->raw(sample);
    DDS_ReturnCode_t rc =
        PerftestTypeDataWriter_write(w->tdw, raw, &DDS_HANDLE_NIL);
    return (rc == DDS_RETCODE_OK) ? 0 : -1;
}

#ifdef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
static int micro_writer_write_zcopy(PerftestLiteWriter *base,
                                    PerftestSample *sample)
{
    MicroWriter *w = (MicroWriter *)base;
    PerftestType *loaned = NULL;
    DDS_ReturnCode_t rc;

    rc = DDS_DataWriter_get_loan(w->dw, (void **)&loaned);
    if (rc == DDS_RETCODE_OUT_OF_RESOURCES) {
        return -1;
    }
    if (rc != DDS_RETCODE_OK) {
        PLITE_LOG("[mw] get_loan failed: %d\n", rc);
        return -1;
    }
    if (w->type_iface->copy_for_write((PerftestSample *)loaned, sample) != 0) {
        PLITE_LOG("[mw] failed to copy loaned sample\n");
        goto discard_loan;
    }

    rc = PerftestTypeDataWriter_write(w->tdw, loaned, &DDS_HANDLE_NIL);
    if (rc != DDS_RETCODE_OK) {
        PLITE_LOG("[mw] zcopy write failed: %d\n", rc);
        goto discard_loan;
    }
    return 0;

discard_loan:
    rc = DDS_DataWriter_discard_loan(w->dw, loaned);
    if (rc != DDS_RETCODE_OK) {
        PLITE_LOG("[mw] discard_loan failed: %d\n", rc);
    }
    return -1;
}
#endif

/* -------------------------------------------------------------------- */
/*  Helpers                                                             */
/* -------------------------------------------------------------------- */

static RTI_BOOL configure_udp(MicroMW *self, RT_Registry_T *registry)
{
    struct UDP_InterfaceFactoryProperty *p = NULL;
    OSAPI_Heap_allocate_struct(&p, struct UDP_InterfaceFactoryProperty);
    if (!p) return RTI_FALSE;
    *p = UDP_INTERFACE_FACTORY_PROPERTY_DEFAULT;

    /* Override only if reasonable: the defaults are tuned by Micro and
     * passing zero or out-of-range values trips the property validator. */
    if (PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE > 0
        && PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE <= 65507) {
        p->max_message_size = PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE;
    }
    if (PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE > 0) {
        p->max_receive_buffer_size = PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE;
    }
    if (PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE > 0) {
        p->max_send_buffer_size = PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE;
    }

    /* Restrict to a single allowed interface (the one the user passed,
     * or loopback if none). */
    DDS_StringSeq_set_maximum(&p->allow_interface, 1);
    DDS_StringSeq_set_length(&p->allow_interface, 1);
    *DDS_StringSeq_get_reference(&p->allow_interface, 0) =
        DDS_String_dup(self->cfg->nic);

    /* On bare-metal targets the UDP PSL cannot enumerate interfaces (it
     * has no notion of getifaddrs and the PSL stub only logs WARNING 249
     * "interface list is empty"). When the OS layer can hand us an IPv4
     * address for the requested NIC, disable auto-config and pre-populate
     * if_table so Micro binds to the right address.
     *
     * UDP_InterfaceTableEntry fields are host byte order while the OS
     * interface returns network byte order. */
    {
        const PerftestLiteOsInterface *os = perftest_lite_os_get();
        uint32_t addr_be = (os && os->nic_address_ipv4)
            ? os->nic_address_ipv4(self->cfg->nic) : 0u;
        uint32_t mask_be = (os && os->nic_netmask_ipv4)
            ? os->nic_netmask_ipv4(self->cfg->nic) : UINT32_MAX;
        if (addr_be != 0u && addr_be != UINT32_MAX
                && mask_be != 0u && mask_be != UINT32_MAX) {
            const uint16_t endian_test = 1u;
            const int little_endian =
                *((const uint8_t *)&endian_test) == 1u;
            uint32_t addr_host = addr_be;
            uint32_t mask_host = mask_be;
            if (little_endian) {
                addr_host =
                    ((addr_be & 0x000000FFu) << 24) |
                    ((addr_be & 0x0000FF00u) <<  8) |
                    ((addr_be & 0x00FF0000u) >>  8) |
                    ((addr_be & 0xFF000000u) >> 24);
                mask_host =
                    ((mask_be & 0x000000FFu) << 24) |
                    ((mask_be & 0x0000FF00u) <<  8) |
                    ((mask_be & 0x00FF0000u) >>  8) |
                    ((mask_be & 0xFF000000u) >> 24);
            }
            p->disable_auto_interface_config = RTI_TRUE;
            if (!UDP_InterfaceTable_add_entry(&p->if_table,
                    addr_host, mask_host, self->cfg->nic,
                    UDP_INTERFACE_INTERFACE_UP_FLAG)) {
                PLITE_LOG("[mw] failed to add udp if_table entry\n");
                return RTI_FALSE;
            }
        }
    }

    if (!RT_Registry_unregister(registry, NETIO_DEFAULT_UDP_NAME, NULL, NULL)) {
        /* Some Micro builds do not auto-register the default UDP plugin.
         * That's fine: we just register ours below. */
    }
    if (!RT_Registry_register(registry, NETIO_DEFAULT_UDP_NAME,
            UDP_InterfaceFactory_get_interface(),
            (struct RT_ComponentFactoryProperty *)p, NULL)) {
        PLITE_LOG("[mw] failed to register udp\n");
        return RTI_FALSE;
    }
    return RTI_TRUE;
}

#ifdef RTI_DPSE
static RTI_BOOL configure_dpse(RT_Registry_T *registry,
                               struct DDS_DomainParticipantQos *qos)
{
    struct DPSE_DiscoveryPluginProperty dpse_prop =
        DPSE_DiscoveryPluginProperty_INITIALIZER;

    if (!RT_Registry_register(registry, "dpse",
            DPSE_DiscoveryFactory_get_interface(), &dpse_prop._parent, NULL)) {
        return RTI_FALSE;
    }
    return RT_ComponentFactoryId_set_name(&qos->discovery.discovery.name,
                                          "dpse");
}

static RTI_INT32 local_writer_object_id(const char *topic_name)
{
    return strcmp(topic_name, PERFTEST_LITE_PING_TOPIC_NAME) == 0
        ? PERFTEST_LITE_PING_DW_KEY : PERFTEST_LITE_PONG_DW_KEY;
}

static RTI_INT32 local_reader_object_id(const char *topic_name)
{
    return strcmp(topic_name, PERFTEST_LITE_PING_TOPIC_NAME) == 0
        ? PERFTEST_LITE_PING_DR_KEY : PERFTEST_LITE_PONG_DR_KEY;
}

static RTI_INT32 remote_writer_object_id(const char *topic_name)
{
    return strcmp(topic_name, PERFTEST_LITE_PING_TOPIC_NAME) == 0
        ? PERFTEST_LITE_PING_DW_KEY : PERFTEST_LITE_PONG_DW_KEY;
}

static RTI_INT32 remote_reader_object_id(const char *topic_name)
{
    return strcmp(topic_name, PERFTEST_LITE_PING_TOPIC_NAME) == 0
        ? PERFTEST_LITE_PING_DR_KEY : PERFTEST_LITE_PONG_DR_KEY;
}

static RTI_BOOL assert_remote_subscription(MicroMW *self,
                                           const char *topic_name,
                                           const struct DDS_DataWriterQos *qos)
{
    struct DDS_SubscriptionBuiltinTopicData remote =
        DDS_SubscriptionBuiltinTopicData_INITIALIZER;

    remote.topic_name = DDS_String_dup(topic_name);
    remote.type_name = DDS_String_dup(self->cfg->type_iface->type_name());
    if (!remote.topic_name || !remote.type_name) return RTI_FALSE;
    remote.reliability.kind = qos->reliability.kind;
    remote.durability.kind = qos->durability.kind;
    remote.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] =
        remote_reader_object_id(topic_name);

    return DPSE_RemoteSubscription_assert(self->participant,
        self->cfg->remote_participant_name, &remote,
        PerftestTypeI_get_key_kind()) == DDS_RETCODE_OK;
}

static RTI_BOOL assert_remote_publication(MicroMW *self,
                                          const char *topic_name,
                                          const struct DDS_DataReaderQos *qos)
{
    struct DDS_PublicationBuiltinTopicData remote =
        DDS_PublicationBuiltinTopicData_INITIALIZER;

    remote.topic_name = DDS_String_dup(topic_name);
    remote.type_name = DDS_String_dup(self->cfg->type_iface->type_name());
    if (!remote.topic_name || !remote.type_name) return RTI_FALSE;
    remote.reliability.kind = qos->reliability.kind;
    remote.durability.kind = qos->durability.kind;
    remote.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] =
        remote_writer_object_id(topic_name);

    return DPSE_RemotePublication_assert(self->participant,
        self->cfg->remote_participant_name, &remote,
        PerftestTypeI_get_key_kind()) == DDS_RETCODE_OK;
}
#else
static RTI_BOOL configure_dpde(RT_Registry_T *registry,
                               struct DDS_DomainParticipantQos *qos)
{
    struct DPDE_DiscoveryPluginProperty dpde_prop =
        DPDE_DiscoveryPluginProperty_INITIALIZER;
    if (!RT_Registry_register(registry, "dpde",
            DPDE_DiscoveryFactory_get_interface(),
            &dpde_prop._parent, NULL)) {
        return RTI_FALSE;
    }
    return RT_ComponentFactoryId_set_name(&qos->discovery.discovery.name,
                                          "dpde");
}
#endif

#ifdef PERFTEST_LITE_HAS_MICRO_SHMEM
static RTI_BOOL configure_shmem(RT_Registry_T *registry)
{
    struct NETIO_SHMEMInterfaceFactoryProperty *property = NULL;

    OSAPI_Heap_allocate_struct(
        &property, struct NETIO_SHMEMInterfaceFactoryProperty);
    if (!property) return RTI_FALSE;
    *property = (struct NETIO_SHMEMInterfaceFactoryProperty)
        NETIO_SHMEMInterfaceFactoryProperty_INITIALIZER;

    if (!RT_Registry_register(registry, NETIO_DEFAULT_SHMEM_NAME,
            NETIO_SHMEMInterfaceFactory_get_interface(),
            (struct RT_ComponentFactoryProperty *)property, NULL)) {
        PLITE_LOG("[mw] failed to register shmem\n");
        OSAPI_Heap_free_struct(property);
        return RTI_FALSE;
    }
    return RTI_TRUE;
}

static RTI_BOOL configure_shmem_qos(
    MicroMW *self,
    struct DDS_DomainParticipantQos *qos)
{
    char **str_ref;

    if (self->cfg->peer && self->cfg->peer[0]) {
        PLITE_LOG("[mw] WARNING: -peer is ignored for SHMEM; using _shmem://\n");
    }
    if (!DDS_StringSeq_set_maximum(&qos->transports.enabled_transports, 1)
        || !DDS_StringSeq_set_length(&qos->transports.enabled_transports, 1)
        || !DDS_StringSeq_set_maximum(&qos->user_traffic.enabled_transports, 1)
        || !DDS_StringSeq_set_length(&qos->user_traffic.enabled_transports, 1)
        || !DDS_StringSeq_set_maximum(&qos->discovery.enabled_transports, 1)
        || !DDS_StringSeq_set_length(&qos->discovery.enabled_transports, 1)
        || !DDS_StringSeq_set_maximum(&qos->discovery.initial_peers, 1)
        || !DDS_StringSeq_set_length(&qos->discovery.initial_peers, 1)) {
        return RTI_FALSE;
    }

    str_ref = DDS_StringSeq_get_reference(&qos->transports.enabled_transports, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup(NETIO_DEFAULT_SHMEM_NAME))) return RTI_FALSE;
    str_ref = DDS_StringSeq_get_reference(&qos->user_traffic.enabled_transports, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup("_shmem://"))) return RTI_FALSE;
    str_ref = DDS_StringSeq_get_reference(&qos->discovery.enabled_transports, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup("_shmem://"))) return RTI_FALSE;
    str_ref = DDS_StringSeq_get_reference(&qos->discovery.initial_peers, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup("_shmem://"))) return RTI_FALSE;
    return RTI_TRUE;
}
#endif

#ifdef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
static RTI_BOOL configure_zerocopy(
    RT_Registry_T *registry,
    const PerftestLiteMiddlewareConfig *cfg)
{
    struct ZCOPY_NotifMechanismProperty *notif_mech;
    struct ZCOPY_NotifInterfaceFactoryProperty *notif_prop;

    if (!NDDS_Transport_ZeroCopy_initialize(registry, NULL, NULL)) {
        PLITE_LOG("[mw] NDDS_Transport_ZeroCopy_initialize failed\n");
        return RTI_FALSE;
    }

    OSAPI_Heap_allocate_struct(
        &notif_mech, struct ZCOPY_NotifMechanismProperty);
    if (!notif_mech) return RTI_FALSE;
    *notif_mech = (struct ZCOPY_NotifMechanismProperty)
        ZCOPY_NotifMechanismProperty_INITIALIZER;
    notif_mech->intf_addr = 0;

    OSAPI_Heap_allocate_struct(
        &notif_prop, struct ZCOPY_NotifInterfaceFactoryProperty);
    if (!notif_prop) {
        OSAPI_Heap_free_struct(notif_mech);
        return RTI_FALSE;
    }
    *notif_prop = (struct ZCOPY_NotifInterfaceFactoryProperty)
        ZCOPY_NotifInterfaceFactoryProperty_INITIALIZER;
    notif_prop->user_property = notif_mech;
    notif_prop->max_samples_per_notif = cfg->latency_test
        ? PERFTEST_LITE_ZCOPY_MAX_SAMPLES_PER_NOTIF_LATENCY
        : PERFTEST_LITE_ZCOPY_MAX_SAMPLES_PER_NOTIF_THROUGHPUT;

    if (!ZCOPY_NotifMechanism_register(registry, NETIO_DEFAULT_NOTIF_NAME,
                                        notif_prop)) {
        PLITE_LOG("[mw] ZCOPY_NotifMechanism_register failed\n");
        OSAPI_Heap_free_struct(notif_prop);
        OSAPI_Heap_free_struct(notif_mech);
        return RTI_FALSE;
    }
    return RTI_TRUE;
}

static RTI_BOOL configure_zerocopy_qos(struct DDS_DomainParticipantQos *qos)
{
    char **str_ref;

    if (!DDS_StringSeq_set_maximum(&qos->transports.enabled_transports, 2)
        || !DDS_StringSeq_set_length(&qos->transports.enabled_transports, 2)
        || !DDS_StringSeq_set_maximum(&qos->user_traffic.enabled_transports, 1)
        || !DDS_StringSeq_set_length(&qos->user_traffic.enabled_transports, 1)
        || !DDS_StringSeq_set_maximum(&qos->discovery.enabled_transports, 1)
        || !DDS_StringSeq_set_length(&qos->discovery.enabled_transports, 1)
        || !DDS_StringSeq_set_maximum(&qos->discovery.initial_peers, 1)
        || !DDS_StringSeq_set_length(&qos->discovery.initial_peers, 1)) {
        return RTI_FALSE;
    }

    str_ref = DDS_StringSeq_get_reference(&qos->transports.enabled_transports, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup(NETIO_DEFAULT_NOTIF_NAME))) return RTI_FALSE;
    str_ref = DDS_StringSeq_get_reference(&qos->transports.enabled_transports, 1);
    if (!str_ref || !(*str_ref = DDS_String_dup(NETIO_DEFAULT_UDP_NAME))) return RTI_FALSE;
    str_ref = DDS_StringSeq_get_reference(&qos->user_traffic.enabled_transports, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup("notif://"))) return RTI_FALSE;
    /* discovery.enabled_transports takes a locator PREFIX, not a full address.
     * Micro matches receive locators (e.g. "_udp://0.0.0.0:7411") against this
     * prefix. Using "_udp://127.0.0.1" would fail because the receive locator
     * starts with "_udp://0.0.0.0", not "_udp://127.0.0.1". */
    str_ref = DDS_StringSeq_get_reference(&qos->discovery.enabled_transports, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup("_udp://"))) return RTI_FALSE;
    str_ref = DDS_StringSeq_get_reference(&qos->discovery.initial_peers, 0);
    if (!str_ref || !(*str_ref = DDS_String_dup("_udp://127.0.0.1"))) return RTI_FALSE;
    qos->discovery.accept_unknown_peers = DDS_BOOLEAN_TRUE;
    return RTI_TRUE;
}
#endif

/* -------------------------------------------------------------------- */
/*  Middleware lifecycle                                                */
/* -------------------------------------------------------------------- */

static int micro_init(PerftestLiteMiddleware *base,
                      const PerftestLiteMiddlewareConfig *cfg)
{
    MicroMW *self = (MicroMW *)base->impl;
    self->cfg = cfg;
    self->is_zerocopy = 0;
    self->is_shmem = 0;

    if (cfg->transport_id && strcmp(cfg->transport_id, "UDPv4") != 0) {
        if (strcmp(cfg->transport_id, "SHMEM") == 0) {
#if defined(PERFTEST_LITE_HAS_MICRO_SHMEM) && !defined(PERFTEST_LITE_TYPE_ZCOPY)
            self->is_shmem = 1;
#elif defined(PERFTEST_LITE_TYPE_ZCOPY)
            PLITE_LOG("[mw] -transport SHMEM requires PERFTEST_LITE_TYPE=sequence\n");
            return -1;
#else
            PLITE_LOG("[mw] -transport SHMEM is not compiled in\n");
            return -1;
#endif
        } else if (strcmp(cfg->transport_id, "ZeroCopy") == 0) {
#if defined(PERFTEST_LITE_HAS_MICRO_ZEROCOPY) && defined(PERFTEST_LITE_TYPE_ZCOPY)
            self->is_zerocopy = 1;
#elif defined(PERFTEST_LITE_HAS_MICRO_ZEROCOPY)
            PLITE_LOG("[mw] -transport ZeroCopy requires PERFTEST_LITE_TYPE=zcopy\n");
            return -1;
#else
            PLITE_LOG("[mw] -transport ZeroCopy is not compiled in\n");
            return -1;
#endif
        } else {
            PLITE_LOG("[mw] transport '%s' not supported\n", cfg->transport_id);
            return -1;
        }
    }

    self->factory = DDS_DomainParticipantFactory_get_instance();
    RT_Registry_T *registry =
        DDS_DomainParticipantFactory_get_registry(self->factory);

    /* Verbose logging while we are setting up; helps diagnose port issues. */
    OSAPI_Log_set_verbosity(OSAPI_LOG_VERBOSITY_WARNING);

    if (!RT_Registry_register(registry, DDSHST_WRITER_DEFAULT_HISTORY_NAME,
            WHSM_HistoryFactory_get_interface(), NULL, NULL)) {
        PLITE_LOG("[mw] failed to register wh\n"); return -1;
    }
    if (!RT_Registry_register(registry, DDSHST_READER_DEFAULT_HISTORY_NAME,
            RHSM_HistoryFactory_get_interface(), NULL, NULL)) {
        PLITE_LOG("[mw] failed to register rh\n"); return -1;
    }

    if (!configure_udp(self, registry)) {
        return -1;
    }

#ifdef PERFTEST_LITE_HAS_MICRO_SHMEM
    if (self->is_shmem && !configure_shmem(registry)) return -1;
#endif

#ifdef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
    if (self->is_zerocopy && !configure_zerocopy(registry, cfg)) return -1;
#endif

    struct DDS_DomainParticipantQos dp_qos =
        DDS_DomainParticipantQos_INITIALIZER;

#ifdef RTI_DPSE
    if (!configure_dpse(registry, &dp_qos)) {
        PLITE_LOG("[mw] failed to configure dpse\n");
#else
    if (!configure_dpde(registry, &dp_qos)) {
        PLITE_LOG("[mw] failed to configure dpde\n");
#endif
        DDS_DomainParticipantQos_finalize(&dp_qos);
        return -1;
    }

#ifdef PERFTEST_LITE_HAS_MICRO_SHMEM
    if (self->is_shmem && !configure_shmem_qos(self, &dp_qos)) {
        PLITE_LOG("[mw] failed to configure SHMEM QoS\n");
        DDS_DomainParticipantQos_finalize(&dp_qos);
        return -1;
    }
#endif
#ifdef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
    if (self->is_zerocopy && !configure_zerocopy_qos(&dp_qos)) {
        PLITE_LOG("[mw] failed to configure Zero Copy QoS\n");
        DDS_DomainParticipantQos_finalize(&dp_qos);
        return -1;
    }
#endif
    if (!self->is_shmem && !self->is_zerocopy) {
        DDS_StringSeq_set_maximum(&dp_qos.discovery.initial_peers, 1);
        DDS_StringSeq_set_length(&dp_qos.discovery.initial_peers, 1);
        *DDS_StringSeq_get_reference(&dp_qos.discovery.initial_peers, 0) =
            DDS_String_dup(cfg->peer);
    }

    dp_qos.resource_limits.max_destination_ports = 8;
    dp_qos.resource_limits.max_receive_ports     = 8;
    dp_qos.resource_limits.local_topic_allocation  = 4;
    dp_qos.resource_limits.local_type_allocation   = 2;
    dp_qos.resource_limits.local_reader_allocation = 4;
    dp_qos.resource_limits.local_writer_allocation = 4;
    dp_qos.resource_limits.remote_participant_allocation = 4;
    dp_qos.resource_limits.remote_reader_allocation      = 8;
    dp_qos.resource_limits.remote_writer_allocation      = 8;

    DDS_EntityNameQosPolicy_set_name(&dp_qos.participant_name,
        cfg->participant_name);
    dp_qos.protocol.participant_id = cfg->participant_id;

    self->participant = DDS_DomainParticipantFactory_create_participant(
        self->factory, cfg->domain_id, &dp_qos, NULL, DDS_STATUS_MASK_NONE);
    DDS_DomainParticipantQos_finalize(&dp_qos);
    if (!self->participant) {
        PLITE_LOG("[mw] failed to create participant\n");
        return -1;
    }

#ifdef RTI_DPSE
    if (!cfg->remote_participant_name
        || DPSE_RemoteParticipant_assert(self->participant,
               cfg->remote_participant_name) != DDS_RETCODE_OK) {
        PLITE_LOG("[mw] failed to assert remote participant\n");
        return -1;
    }
#endif

    if (PerftestTypeTypeSupport_register_type(self->participant,
            cfg->type_iface->type_name()) != DDS_RETCODE_OK) {
        PLITE_LOG("[mw] failed to register type\n");
        return -1;
    }

    self->ping_topic = DDS_DomainParticipant_create_topic(self->participant,
        PERFTEST_LITE_PING_TOPIC_NAME, cfg->type_iface->type_name(),
        &DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    self->pong_topic = DDS_DomainParticipant_create_topic(self->participant,
        PERFTEST_LITE_PONG_TOPIC_NAME, cfg->type_iface->type_name(),
        &DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (!self->ping_topic || !self->pong_topic) {
        PLITE_LOG("[mw] failed to create topics\n");
        return -1;
    }

    self->publisher = DDS_DomainParticipant_create_publisher(self->participant,
        &DDS_PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    self->subscriber = DDS_DomainParticipant_create_subscriber(self->participant,
        &DDS_SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (!self->publisher || !self->subscriber) {
        PLITE_LOG("[mw] failed to create pub/sub\n");
        return -1;
    }

#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    /* This excludes endpoint sample pools. The engine emits a second,
     * endpoint-ready checkpoint after its writer and reader are created. */
    perftest_lite_memory_emit_checkpoint(
        "participant_ready", "middleware", cfg->process_start_heap_bytes);
#endif
    return 0;
}

static DDS_Topic *topic_by_name(MicroMW *self, const char *name)
{
    if (strcmp(name, PERFTEST_LITE_PING_TOPIC_NAME) == 0) return self->ping_topic;
    if (strcmp(name, PERFTEST_LITE_PONG_TOPIC_NAME) == 0) return self->pong_topic;
    return NULL;
}

static PerftestLiteWriter *micro_create_writer(PerftestLiteMiddleware *base,
                                               const char *topic_name,
                                               int reliable)
{
    MicroMW *self = (MicroMW *)base->impl;
    DDS_Topic *t = topic_by_name(self, topic_name);
    int history_depth;
    int max_samples;
    if (!t) return NULL;

    history_depth = self->cfg->latency_test
        ? PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY
        : PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT;
    max_samples = self->cfg->latency_test
        ? PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY
        : PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT;

    struct DDS_DataWriterQos qos = DDS_DataWriterQos_INITIALIZER;
    qos.reliability.kind = reliable
        ? DDS_RELIABLE_RELIABILITY_QOS : DDS_BEST_EFFORT_RELIABILITY_QOS;
    /* Zero Copy v2 supports reliable writers but requires a bounded KEEP_LAST
     * queue. KEEP_ALL is unsupported by netio_zcopy and makes writer creation
     * fail. Regular Micro transports retain the previous KEEP_ALL behavior. */
#ifndef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
    if (reliable) {
        qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
        qos.history.depth = max_samples;
    } else {
#else
    {
#endif
        qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
        qos.history.depth = history_depth;
    }
    qos.resource_limits.max_samples_per_instance = max_samples;
    qos.resource_limits.max_instances            = PERFTEST_LITE_DW_MAX_INSTANCES;
    qos.resource_limits.max_samples              =
        qos.resource_limits.max_samples_per_instance *
        qos.resource_limits.max_instances;
    if (reliable) {
        qos.protocol.rtps_reliable_writer.heartbeat_period.sec =
            PERFTEST_LITE_DW_HEARTBEAT_PERIOD_NS / 1000000000;
        qos.protocol.rtps_reliable_writer.heartbeat_period.nanosec =
            PERFTEST_LITE_DW_HEARTBEAT_PERIOD_NS % 1000000000;
    }

#ifdef RTI_DPSE
    qos.protocol.rtps_object_id = local_writer_object_id(topic_name);
#endif

    DDS_DataWriter *dw = DDS_Publisher_create_datawriter(self->publisher, t,
        &qos, NULL, DDS_STATUS_MASK_NONE);
    if (!dw) {
        DDS_DataWriterQos_finalize(&qos);
        return NULL;
    }

#ifdef RTI_DPSE
    if (!assert_remote_subscription(self, topic_name, &qos)) {
        PLITE_LOG("[mw] failed to assert remote subscription for %s\n",
                  topic_name);
        DDS_DataWriterQos_finalize(&qos);
        return NULL;
    }
#endif
    DDS_DataWriterQos_finalize(&qos);

    MicroWriter *w = NULL;
    OSAPI_Heap_allocate_struct(&w, MicroWriter);
    if (!w) return NULL;
    w->dw = dw;
    w->tdw = PerftestTypeDataWriter_narrow(dw);
    w->type_iface = self->cfg->type_iface;
#ifdef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
    w->base.write = self->is_zerocopy
        ? micro_writer_write_zcopy : micro_writer_write;
#else
    w->base.write = micro_writer_write;
#endif
    w->base.impl  = w;
    return &w->base;
}

static PerftestLiteReader *micro_create_reader(PerftestLiteMiddleware *base,
                                               const char *topic_name,
                                               int reliable,
                                               const PerftestLiteListener *l)
{
    MicroMW *self = (MicroMW *)base->impl;
    DDS_Topic *t = topic_by_name(self, topic_name);
    int history_depth;
    int max_samples;
    if (!t) return NULL;

    history_depth = self->cfg->latency_test
        ? PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY
        : PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT;
    max_samples = self->cfg->latency_test
        ? PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY
        : PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT;

    struct DDS_DataReaderQos qos = DDS_DataReaderQos_INITIALIZER;
    qos.reliability.kind = reliable
        ? DDS_RELIABLE_RELIABILITY_QOS : DDS_BEST_EFFORT_RELIABILITY_QOS;
    qos.history.kind     = DDS_KEEP_LAST_HISTORY_QOS;
    qos.history.depth    = history_depth;
    qos.resource_limits.max_samples_per_instance = max_samples;
    qos.resource_limits.max_instances            = PERFTEST_LITE_DR_MAX_INSTANCES;
    qos.resource_limits.max_samples              =
        qos.resource_limits.max_samples_per_instance *
        qos.resource_limits.max_instances;
    qos.reader_resource_limits.max_remote_writers = PERFTEST_LITE_DR_MAX_REMOTE_WRITERS;
    qos.reader_resource_limits.max_remote_writers_per_instance = PERFTEST_LITE_DR_MAX_REMOTE_WRITERS;

#ifdef RTI_DPSE
    qos.protocol.rtps_object_id = local_reader_object_id(topic_name);
#endif

    MicroReader *r = NULL;
    OSAPI_Heap_allocate_struct(&r, MicroReader);
    if (!r) { DDS_DataReaderQos_finalize(&qos); return NULL; }
    r->listener = *l;
    r->owner = self;
    r->type_iface = self->cfg->type_iface;

    struct DDS_DataReaderListener dr_listener = DDS_DataReaderListener_INITIALIZER;
    dr_listener.on_data_available = on_data_available_cb;
    dr_listener.on_subscription_matched = on_sub_matched;
    dr_listener.as_listener.listener_data = r;

    DDS_DataReader *dr = DDS_Subscriber_create_datareader(self->subscriber,
        DDS_Topic_as_topicdescription(t), &qos, &dr_listener,
        DDS_DATA_AVAILABLE_STATUS | DDS_SUBSCRIPTION_MATCHED_STATUS);
    if (!dr) {
        DDS_DataReaderQos_finalize(&qos);
        OSAPI_Heap_free_struct(r);
        return NULL;
    }

#ifdef RTI_DPSE
    if (!assert_remote_publication(self, topic_name, &qos)) {
        PLITE_LOG("[mw] failed to assert remote publication for %s\n",
                  topic_name);
        DDS_DataReaderQos_finalize(&qos);
        OSAPI_Heap_free_struct(r);
        return NULL;
    }
#endif
    DDS_DataReaderQos_finalize(&qos);
    r->dr  = dr;
    r->tdr = PerftestTypeDataReader_narrow(dr);
    r->base.take = micro_reader_take;
    r->base.impl = r;
    return &r->base;
}

/* For Micro+DPDE, matching is fully discovered. We listen for the matched
 * counts via the listeners so wait_for_match doesn't need to do anything
 * special; the engine drives the wait loop itself using user_data. */
static int micro_wait_for_match(PerftestLiteMiddleware *m, int32_t timeout_ms)
{
    (void)m; (void)timeout_ms;
    return 0;
}

static void micro_shutdown(PerftestLiteMiddleware *base)
{
    MicroMW *self = (MicroMW *)base->impl;
    if (self->participant) {
        DDS_DomainParticipant_delete_contained_entities(self->participant);
        DDS_DomainParticipantFactory_delete_participant(self->factory,
            self->participant);
        self->participant = NULL;
    }
    /* Best-effort: leave the registry alone (subsequent runs reuse it). */
}

/* -------------------------------------------------------------------- */
/*  Singleton                                                           */
/* -------------------------------------------------------------------- */

static MicroMW g_micro_state;
static PerftestLiteMiddleware g_micro_mw = {
    .init           = micro_init,
    .create_writer  = micro_create_writer,
    .create_reader  = micro_create_reader,
    .wait_for_match = micro_wait_for_match,
    .shutdown       = micro_shutdown,
    .impl           = &g_micro_state
};

PerftestLiteMiddleware *perftest_lite_middleware_get(void)
{
    g_micro_mw.impl = &g_micro_state;
    return &g_micro_mw;
}
