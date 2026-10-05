#include "module_abi.h"
#include "hid_report_parser.h"

#include <stddef.h>
#include <stdint.h>

#define HIDPAD_VERSION "1.1.51"
#define HIDPAD_EXPORT __attribute__((visibility("default")))
#define HIDPAD_MAX_REPORTS 12
#define HIDPAD_MAX_SCAN_RESULTS 16
#define HIDPAD_EVENT_BUDGET 64
#define HIDPAD_REPORT_CACHE_SIZE 32
#define HIDPAD_REPORT_MAP_MAX_SIZE 512
#define HIDPAD_MAX_VENDOR_SERVICES 12
#define HIDPAD_MAX_VENDOR_CHANNELS 8
#define HIDPAD_KEEPALIVE_MS 15000u
#define HIDPAD_KEEPALIVE_RETRY_MS 3000u
#define HIDPAD_BTP_INIT_GAP_MS 50u
#define HIDPAD_BTP_INIT_WAIT_MS 500u
#define HIDPAD_BTP_START_DELAY_MS 100u
#define HIDPAD_BTP_HEARTBEAT_MS 1000u
#define HIDPAD_FLYDIGI_ACQUIRE_RETRY_MS 500u
#define HIDPAD_NOTIFY_RECONNECT_MS 2500u
/* Temporary device A/B test: leave quiet Q34/Q36 connections intact. */
#define HIDPAD_ENABLE_NOTIFY_RECONNECT 0
#define HIDPAD_RESCAN_MIN_MS 1000u
#define HIDPAD_RESCAN_MAX_MS 8000u
#define HIDPAD_CONN_INTERVAL_MIN 7u
#define HIDPAD_CONN_INTERVAL_MAX 24u
#define HIDPAD_BTP_CONN_INTERVAL_MIN 6u
#define HIDPAD_BTP_CONN_INTERVAL_MAX 8u
#define HIDPAD_CONN_LATENCY 0u
#define HIDPAD_CONN_SUPERVISION_TIMEOUT 500u
#define HIDPAD_WORKER_STACK_BYTES (6u * 1024u)
#define HIDPAD_WORKER_PRIORITY 2u
#define HIDPAD_WORKER_CORE 0

/* Keep only the host procedures used by this module. The public procedure IDs
 * remain the firmware/module boundary; this compact table is module-local. */
typedef struct hidpad_time_api_t {
    uint32_t (*millis)(void);
} hidpad_time_api_t;

typedef struct hidpad_heap_api_t {
    void *(*calloc)(size_t n, size_t size, uint32_t caps);
    void (*free)(void *ptr);
} hidpad_heap_api_t;

typedef struct hidpad_serial_api_t {
    void (*println)(const char *text);
} hidpad_serial_api_t;

typedef struct hidpad_task_api_t {
    int32_t (*create_ex)(const char *name, void (*entry)(void *), void *arg,
                         uint32_t stack_bytes, uint32_t priority, int32_t core,
                         uint32_t heap_caps, void **out_task);
} hidpad_task_api_t;

typedef struct hidpad_sync_api_t {
    int32_t (*create_counting)(uint32_t max_count, uint32_t initial_count,
                               module_sync_handle_t *out_handle);
    int32_t (*create_mutex)(module_sync_handle_t *out_handle);
    int32_t (*take)(module_sync_handle_t handle, uint32_t timeout_ms);
    int32_t (*give)(module_sync_handle_t handle);
    void (*destroy)(module_sync_handle_t handle);
} hidpad_sync_api_t;

typedef struct hidpad_runtime_api_t {
    int32_t (*event_post)(lua_State *L, int32_t lua_ref);
    void (*event_cancel)(lua_State *L, int32_t lua_ref);
} hidpad_runtime_api_t;

typedef struct hidpad_lua_api_t {
    int (*gettop)(lua_State *L);
    int (*isnil)(lua_State *L, int idx);
    int (*istable)(lua_State *L, int idx);
    int (*isnumber)(lua_State *L, int idx);
    int (*isstring)(lua_State *L, int idx);
    int64_t (*tointeger)(lua_State *L, int idx);
    const char *(*tostring)(lua_State *L, int idx);
    void *(*touserdata)(lua_State *L, int idx);
    void (*pushnil)(lua_State *L);
    void (*pushboolean)(lua_State *L, int value);
    void (*pushinteger)(lua_State *L, int64_t value);
    void (*pushstring)(lua_State *L, const char *text);
    void (*pushlightuserdata)(lua_State *L, void *ptr);
    void (*pushcclosure)(lua_State *L, module_lua_cfunction_t fn, int nup);
    void (*pushvalue)(lua_State *L, int idx);
    void (*createtable)(lua_State *L, int narr, int nrec);
    void (*setfield)(lua_State *L, int idx, const char *key);
    int (*registry_ref)(lua_State *L);
    void (*registry_unref)(lua_State *L, int ref);
    int (*upvalue_index)(int n);
} hidpad_lua_api_t;

typedef struct hidpad_ble_api_t {
    int32_t (*open)(uint32_t owner_token, const module_ble_config_t *cfg,
                    module_ble_session_t *out_session);
    int32_t (*close)(module_ble_session_t session);
    int32_t (*gap_scan)(module_ble_session_t session, const module_ble_scan_config_t *cfg);
    int32_t (*gap_scan_stop)(module_ble_session_t session);
    int32_t (*gap_connect)(module_ble_session_t session, uint8_t addr_type,
                           const char *address, uint32_t timeout_ms);
    int32_t (*gap_disconnect)(module_ble_session_t session, uint16_t conn_handle);
    int32_t (*gap_pair)(module_ble_session_t session, uint16_t conn_handle, int32_t async_pair);
    int32_t (*gattc_discover_services)(module_ble_session_t session, uint16_t conn_handle,
                                       const char *uuid_or_null);
    int32_t (*gattc_discover_characteristics)(module_ble_session_t session, uint16_t conn_handle,
                                              uint16_t start_handle, uint16_t end_handle,
                                              const char *uuid_or_null);
    int32_t (*gattc_discover_descriptors)(module_ble_session_t session, uint16_t conn_handle,
                                          uint16_t start_handle, uint16_t end_handle);
    int32_t (*gattc_read)(module_ble_session_t session, uint16_t conn_handle,
                          uint16_t value_handle);
    int32_t (*gattc_read_long)(module_ble_session_t session, uint16_t conn_handle,
                               uint16_t value_handle, uint16_t offset);
    int32_t (*gattc_write_long)(module_ble_session_t session, uint16_t conn_handle,
                                uint16_t value_handle, const void *data, size_t data_len);
    int32_t (*gap_get_rssi)(module_ble_session_t session, uint16_t conn_handle,
                            int16_t *out_rssi);
    int32_t (*gattc_write)(module_ble_session_t session, uint16_t conn_handle,
                           uint16_t value_handle, const void *data, size_t data_len,
                           uint32_t mode);
    int32_t (*gattc_exchange_mtu)(module_ble_session_t session, uint16_t conn_handle);
    int32_t (*event_poll)(module_ble_session_t session, module_ble_event_t *out_event);
    int32_t (*gap_set_connection_params)(module_ble_session_t session, uint16_t conn_handle,
                                         uint16_t min_interval, uint16_t max_interval,
                                         uint16_t latency, uint16_t supervision_timeout);
    int32_t (*gap_forget_device)(module_ble_session_t session, uint8_t addr_type,
                                 const char *address);
    int32_t (*gap_clear_bonds)(module_ble_session_t session);
} hidpad_ble_api_t;

typedef struct hidpad_host_api_t {
    hidpad_serial_api_t serial;
    hidpad_time_api_t time;
    hidpad_heap_api_t heap;
    hidpad_task_api_t task;
    hidpad_sync_api_t sync;
    hidpad_lua_api_t lua;
    hidpad_ble_api_t ble;
    hidpad_runtime_api_t runtime;
} hidpad_host_api_t;

#define UUID_HID 0x1812u
#define UUID_REPORT_MAP 0x2a4bu
#define UUID_HID_CONTROL_POINT 0x2a4cu
#define UUID_REPORT 0x2a4du
#define UUID_PROTOCOL_MODE 0x2a4eu
#define UUID_CCCD 0x2902u
#define UUID_REPORT_REFERENCE 0x2908u
#define UUID_FLYDIGI_VENDOR_SERVICE 0x1204u
#define UUID_FLYDIGI_VENDOR_NOTIFY 0x1203u
#define UUID_BTP_VENDOR_SERVICE 0x7310u
#define UUID_BTP_VENDOR_INPUT 0x7311u
#define UUID_BTP_VENDOR_WRITE 0x7312u
#define UUID_BTP_VENDOR_REPLY 0x7313u

#define UUID_FLYDIGI_VENDOR_SERVICE_TEXT "1204"
#define UUID_BTP_VENDOR_SERVICE_TEXT "7310"
#define UUID_FLYDIGI_NUS_SERVICE_TEXT "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define UUID_FLYDIGI_NUS_WRITE_TEXT "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define UUID_FLYDIGI_NUS_NOTIFY_TEXT "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define UUID_HID_TEXT_16 "1812"
#define UUID_HID_TEXT_128 "00001812-0000-1000-8000-00805f9b34fb"
#define HIDPAD_Q36_INIT_ATTEMPTS 2u

#define BTN_UP (1u << 0)
#define BTN_DOWN (1u << 1)
#define BTN_LEFT (1u << 2)
#define BTN_RIGHT (1u << 3)
#define BTN_A (1u << 4)
#define BTN_B (1u << 5)
#define BTN_X (1u << 6)
#define BTN_Y (1u << 7)
#define BTN_LB (1u << 8)
#define BTN_RB (1u << 9)
#define BTN_LS (1u << 10)
#define BTN_RS (1u << 11)
#define BTN_VIEW (1u << 12)
#define BTN_MENU (1u << 13)
#define BTN_SHARE (1u << 14)
#define BTN_HOME (1u << 15)
typedef enum driver_phase_t {
    PHASE_STOPPED = 0,
    PHASE_SCANNING,
    PHASE_SELECT_DEVICE,
    PHASE_CONNECTING,
    PHASE_PAIRING,
    PHASE_EXCHANGE_MTU,
    PHASE_DISCOVER_SERVICES,
    PHASE_DISCOVER_CHARACTERISTICS,
    PHASE_SET_PROTOCOL_MODE,
    PHASE_DISCOVER_DESCRIPTORS,
    PHASE_READ_REPORT_MAP,
    PHASE_READ_REPORT_REFERENCES,
    PHASE_SUBSCRIBE,
    PHASE_VENDOR_DISCOVER_SERVICES,
    PHASE_VENDOR_DISCOVER_CHARACTERISTICS,
    PHASE_VENDOR_DISCOVER_DESCRIPTORS,
    PHASE_VENDOR_SUBSCRIBE,
    PHASE_READY,
    PHASE_WAIT_RESCAN,
    PHASE_ERROR,
} driver_phase_t;

typedef enum device_profile_t {
    DEVICE_PROFILE_HID = 0,
    DEVICE_PROFILE_Q36,
    DEVICE_PROFILE_XBOX,
} device_profile_t;

typedef enum pending_read_t {
    PENDING_READ_NONE = 0,
    PENDING_READ_MAP,
    PENDING_READ_REFERENCE,
    PENDING_READ_INPUT,
    PENDING_READ_KEEPALIVE,
} pending_read_t;

typedef enum worker_command_t {
    WORKER_COMMAND_NONE = 0,
    WORKER_COMMAND_START,
    WORKER_COMMAND_RESCAN,
    WORKER_COMMAND_SCAN,
    WORKER_COMMAND_CONNECT,
    WORKER_COMMAND_DISCONNECT,
    WORKER_COMMAND_PAIR,
    WORKER_COMMAND_FORGET,
} worker_command_t;

typedef struct report_characteristic_t {
    uint16_t value_handle;
    uint16_t descriptor_end_handle;
    uint8_t properties;
    uint16_t cccd_handle;
    uint16_t reference_handle;
    uint8_t report_id;
    uint8_t report_type;
    uint8_t subscribed;
    uint8_t last_report_len;
    uint8_t last_report_valid;
    uint8_t last_decode_ok;
    uint32_t notify_count;
    uint32_t game_buttons;
    uint32_t consumer_buttons;
    uint32_t raw_buttons;
    uint8_t last_report[HIDPAD_REPORT_CACHE_SIZE];
} report_characteristic_t;

typedef struct driver_state_t {
    uint32_t seq;
    uint32_t timestamp_ms;
    uint32_t buttons;
    uint32_t raw_buttons;
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;
    uint16_t lt;
    uint16_t rt;
    uint8_t report_id;
    uint8_t connected;
    uint8_t connecting;
    uint8_t encrypted;
    uint16_t disconnect_reason;
    char address[18];
    char name[40];
} driver_state_t;

typedef struct advertisement_t {
    char name[40];
    uint16_t appearance;
    uint16_t company;
    uint8_t has_hid;
} advertisement_t;

typedef struct discovered_device_t {
    char address[18];
    char name[40];
    int16_t rssi;
    uint8_t addr_type;
    uint8_t profile;
    uint8_t score;
} discovered_device_t;

typedef struct vendor_service_candidate_t {
    uint16_t start_handle;
    uint16_t end_handle;
} vendor_service_candidate_t;

typedef struct vendor_channel_t {
    uint16_t value_handle;
    uint16_t descriptor_end_handle;
    uint16_t cccd_handle;
    uint8_t properties;
    uint8_t subscribed;
    uint8_t flydigi_nus_notify;
} vendor_channel_t;

/* Scan/discovery/configuration data is not touched by the ready input path. */
typedef struct hidpad_cold_state_t {
    /* Configuration/diagnostics and command bookkeeping are not input state. */
    uint32_t command_id;
    uint32_t command_deadline_ms;
    worker_command_t command_kind;
    uint8_t command_status; /* 0=none, 1=pending, 2=succeeded, 3=failed */
    const char *command_error;
    discovered_device_t command_device;
    uint8_t command_device_valid;
    uint8_t last_report[HIDPAD_REPORT_CACHE_SIZE];
    uint8_t btp_last_vendor[HIDPAD_REPORT_CACHE_SIZE];
    uint8_t btp_last_vendor_len;
    uint8_t btp_last_handshake[HIDPAD_REPORT_CACHE_SIZE];
    uint8_t btp_last_handshake_len;
    uint32_t connection_params_attempt_count;
    int32_t connection_params_last_error;
    uint32_t btp_keepalive_attempt_count;
    uint32_t btp_keepalive_error_count;
    int32_t btp_keepalive_last_error;
    uint32_t btp_vendor_notify_count;
    uint32_t btp_heartbeat_reply_count;
    uint32_t btp_handshake_reply_count;
    uint32_t btp_watchdog_command_count;
    uint32_t btp_watchdog_reply_count;
    uint32_t btp_input_read_count;
    uint32_t ble_non_notify_event_count;
    uint32_t ble_last_non_notify_ms;
    uint32_t ble_done_error_count;
    uint32_t ble_last_non_notify_irq;
    int32_t ble_last_non_notify_status;
    uint32_t driver_poll_count;
    uint16_t last_report_handle;
    uint8_t last_report_len;
    uint16_t btp_last_vendor_handle;

    discovered_device_t scan_results[HIDPAD_MAX_SCAN_RESULTS];
    module_ble_config_t config_work;
    module_ble_scan_config_t scan_work;
    advertisement_t advertisement_work;
    uint8_t report_map_work[HIDPAD_REPORT_MAP_MAX_SIZE];
    vendor_service_candidate_t vendor_services[HIDPAD_MAX_VENDOR_SERVICES];
    char preferred_address[18];
    char preferred_name[40];
    device_profile_t preferred_profile;
    uint8_t preferred_addr_type;
    uint8_t preferred_metadata_valid;
} hidpad_cold_state_t;

typedef struct hidpad_instance_t {
    const hidpad_host_api_t *host;
    hidpad_cold_state_t *cold;
    uint32_t owner_token;
    module_ble_session_t session;
    driver_phase_t phase;
    device_profile_t profile;
    uint8_t started;
    uint8_t scan_active;
    uint8_t manual_scan;
    uint8_t auto_connect;
    uint8_t auto_connect_dirty;
    uint8_t scan_result_count;
    uint8_t state_dirty;
    uint8_t status_dirty;
    uint8_t peer_addr_type;
    uint8_t forget_pending;
    uint32_t scan_ms;
    uint32_t rescan_backoff_ms;
    uint32_t next_scan_ms;
    uint32_t next_input_poll_ms;
    uint32_t next_notification_reconnect_ms;
    uint32_t next_keepalive_ms;
    uint32_t next_btp_keepalive_ms;
    uint32_t keepalive_count;
    uint32_t btp_last_vendor_notify_ms;
    uint32_t input_notify_count;
    uint16_t conn_handle;
    uint16_t hid_start;
    uint16_t hid_end;
    uint16_t report_map_handle;
    uint16_t control_point_handle;
    uint8_t control_point_properties;
    uint16_t protocol_mode_handle;
    uint8_t protocol_mode_properties;
    report_characteristic_t reports[HIDPAD_MAX_REPORTS];
    uint8_t report_count;
    uint8_t descriptor_index;
    uint8_t open_report_index;
    uint8_t reference_index;
    uint8_t subscribe_index;
    uint8_t subscribed_count;
    uint8_t notification_reconnect_count;
    uint8_t notification_reconnect_pending;
    uint8_t input_poll_index;
    uint8_t hid_init_attempt;
    uint8_t service_uuid_variant;
    uint8_t report_map_valid;
    uint8_t report_map_overflow;
    uint16_t report_map_len;
    uint16_t controls_report_handle;
    uint16_t vendor_start;
    uint16_t vendor_end;
    uint16_t vendor_write_handle;
    uint16_t btp_input_handle;
    uint8_t vendor_write_properties;
    uint8_t vendor_service_variant;
    uint8_t vendor_service_count;
    uint8_t vendor_service_index;
    uint8_t vendor_channel_count;
    uint8_t vendor_service_channel_start;
    uint8_t vendor_service_channel_end;
    uint8_t vendor_open_channel_index;
    uint8_t vendor_descriptor_index;
    uint8_t vendor_subscribe_index;
    uint8_t vendor_subscribed_count;
    uint8_t vendor_subscribed;
    uint8_t flydigi_init_stage;
    uint8_t btp_init_stage;
    uint8_t btp_missed_heartbeats;
    uint8_t btp_handshake_pending;
    uint8_t btp_handshake_sent;
    uint8_t btp_watchdog_sent;
    uint8_t btp_seen_battery_reply;
    uint8_t btp_seen_info_reply;
    vendor_channel_t vendor_channels[HIDPAD_MAX_VENDOR_CHANNELS];
    pending_read_t pending_read;
    uint8_t pending_report_index;
    hidpad_report_parser_t parser;
    uint32_t game_buttons;
    uint32_t consumer_buttons;
    driver_state_t state;
    /* Reused heap work buffers: internal RAM is preferred for the hot input path. */
    module_ble_event_t event_work;
    hidpad_decoded_report_t decoded_work;
    const char *last_error;
    lua_State *lua;
    int32_t event_ref;
    void *worker_task;
    module_sync_handle_t worker_mutex;
    module_sync_handle_t worker_wake;
    module_sync_handle_t worker_stopped;
    volatile uint8_t worker_running;
    volatile uint8_t worker_stop;
    worker_command_t worker_command;
} hidpad_instance_t;

static hidpad_host_api_t s_host;

static const module_manifest_t s_manifest = {
    MODULE_MANIFEST_MAGIC,
    MODULE_SDK_VERSION,
    sizeof(module_manifest_t),
    "hidpad",
    HIDPAD_VERSION,
    "BLE HID gamepad driver with Xbox, Q34 and Q36 compatibility",
    0,
    MODULE_BOOTSTRAP_ABI_VERSION,
};

static void zero_bytes(void *ptr, size_t len)
{
    size_t i;
    uint8_t *bytes = (uint8_t *)ptr;
    if (!bytes) return;
    for (i = 0; i < len; ++i) bytes[i] = 0;
}

#define HIDPAD_RESOLVE_REQUIRED(proc_id, slot)                                      \
    do {                                                                            \
        void *proc = NULL;                                                          \
        err = module_sdk_resolve_required_v2(resolve, resolve_ctx, proc_id, &proc); \
        if (err != MODULE_OK) return err;                                            \
        slot = (__typeof__(slot))proc;                                               \
    } while (0)

#define HIDPAD_RESOLVE_OPTIONAL(proc_id, slot)                                      \
    do {                                                                            \
        void *proc = NULL;                                                          \
        err = module_sdk_resolve_optional_v2(resolve, resolve_ctx, proc_id, &proc); \
        if (err != MODULE_OK) return err;                                            \
        slot = (__typeof__(slot))proc;                                               \
    } while (0)

static int32_t resolve_host(module_host_resolve_v2_fn resolve, void *resolve_ctx)
{
    int32_t err;
    zero_bytes(&s_host, sizeof(s_host));

    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_SERIAL_PRINTLN_V1, s_host.serial.println);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_TIME_MILLIS_V1, s_host.time.millis);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_HEAP_CALLOC_V1, s_host.heap.calloc);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_HEAP_FREE_V1, s_host.heap.free);

    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_GETTOP_V1, s_host.lua.gettop);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_ISNIL_V1, s_host.lua.isnil);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_ISTABLE_V1, s_host.lua.istable);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_ISNUMBER_V1, s_host.lua.isnumber);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_ISSTRING_V1, s_host.lua.isstring);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_TOINTEGER_V1, s_host.lua.tointeger);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_TOSTRING_V1, s_host.lua.tostring);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_TOUSERDATA_V1, s_host.lua.touserdata);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHNIL_V1, s_host.lua.pushnil);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHBOOLEAN_V1, s_host.lua.pushboolean);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHINTEGER_V1, s_host.lua.pushinteger);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHSTRING_V1, s_host.lua.pushstring);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHLIGHTUSERDATA_V1, s_host.lua.pushlightuserdata);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHCCLOSURE_V1, s_host.lua.pushcclosure);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHVALUE_V1, s_host.lua.pushvalue);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_CREATETABLE_V1, s_host.lua.createtable);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_SETFIELD_V1, s_host.lua.setfield);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_REGISTRY_REF_V1, s_host.lua.registry_ref);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_REGISTRY_UNREF_V1, s_host.lua.registry_unref);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_LUA_UPVALUE_INDEX_V1, s_host.lua.upvalue_index);

    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_OPEN_V1, s_host.ble.open);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_CLOSE_V1, s_host.ble.close);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GAP_SCAN_V1, s_host.ble.gap_scan);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GAP_SCAN_STOP_V1, s_host.ble.gap_scan_stop);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GAP_CONNECT_V1, s_host.ble.gap_connect);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GAP_DISCONNECT_V1, s_host.ble.gap_disconnect);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GAP_PAIR_V1, s_host.ble.gap_pair);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GATTC_DISCOVER_SERVICES_V1,
                            s_host.ble.gattc_discover_services);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GATTC_DISCOVER_CHARACTERISTICS_V1,
                            s_host.ble.gattc_discover_characteristics);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GATTC_DISCOVER_DESCRIPTORS_V1,
                            s_host.ble.gattc_discover_descriptors);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GATTC_READ_V1, s_host.ble.gattc_read);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_BLE_GATTC_READ_LONG_V1,
                            s_host.ble.gattc_read_long);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_BLE_GATTC_WRITE_LONG_V1,
                            s_host.ble.gattc_write_long);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_BLE_GAP_GET_RSSI_V1,
                            s_host.ble.gap_get_rssi);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GATTC_WRITE_V1, s_host.ble.gattc_write);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_BLE_GATTC_EXCHANGE_MTU_V1,
                            s_host.ble.gattc_exchange_mtu);
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_EVENT_POLL_V1, s_host.ble.event_poll);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_BLE_GAP_SET_CONNECTION_PARAMS_V1,
                            s_host.ble.gap_set_connection_params);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_BLE_GAP_FORGET_DEVICE_V1,
                            s_host.ble.gap_forget_device);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_BLE_GAP_CLEAR_BONDS_V1,
                            s_host.ble.gap_clear_bonds);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_TASK_CREATE_EX_V1, s_host.task.create_ex);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_SYNC_CREATE_COUNTING_V1, s_host.sync.create_counting);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_SYNC_CREATE_MUTEX_V1, s_host.sync.create_mutex);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_SYNC_TAKE_V1, s_host.sync.take);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_SYNC_GIVE_V1, s_host.sync.give);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_SYNC_DESTROY_V1, s_host.sync.destroy);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_RUNTIME_EVENT_POST_V1, s_host.runtime.event_post);
    HIDPAD_RESOLVE_OPTIONAL(MODULE_PROC_RUNTIME_EVENT_CANCEL_V1, s_host.runtime.event_cancel);
    return MODULE_OK;
}

#undef HIDPAD_RESOLVE_OPTIONAL
#undef HIDPAD_RESOLVE_REQUIRED

void *memset(void *dst, int value, size_t len)
{
    volatile uint8_t *out = (volatile uint8_t *)dst;
    while (len > 0) {
        *out++ = (uint8_t)value;
        --len;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t len)
{
    size_t i;
    uint8_t *out = (uint8_t *)dst;
    const uint8_t *in = (const uint8_t *)src;
    for (i = 0; i < len; ++i) out[i] = in[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t len)
{
    size_t i;
    uint8_t *out = (uint8_t *)dst;
    const uint8_t *in = (const uint8_t *)src;
    if (out < in) {
        for (i = 0; i < len; ++i) out[i] = in[i];
    } else if (out > in) {
        for (i = len; i > 0; --i) out[i - 1] = in[i - 1];
    }
    return dst;
}

size_t strlen(const char *text)
{
    const volatile char *cursor;
    if (!text) return 0;
    cursor = (const volatile char *)text;
    while (*cursor) ++cursor;
    return (size_t)(cursor - (const volatile char *)text);
}

static uint32_t now_ms(const hidpad_instance_t *inst)
{
    return inst && inst->host && inst->host->time.millis ? inst->host->time.millis() : 0;
}

static char ascii_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static void copy_text(char *dst, size_t capacity, const char *src, size_t src_len)
{
    size_t i;
    if (!dst || capacity == 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    if (src_len >= capacity) src_len = capacity - 1;
    for (i = 0; i < src_len; ++i) dst[i] = src[i];
    dst[src_len] = 0;
}

static int text_contains(const char *text, const char *needle)
{
    size_t i;
    size_t j;
    size_t text_len = strlen(text);
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || text_len < needle_len) return 0;
    for (i = 0; i + needle_len <= text_len; ++i) {
        for (j = 0; j < needle_len; ++j) {
            if (ascii_lower(text[i + j]) != ascii_lower(needle[j])) break;
        }
        if (j == needle_len) return 1;
    }
    return 0;
}

static int is_q36_compatible_name(const char *name)
{
    return text_contains(name, "q36") ||
           text_contains(name, "q34") ||
           text_contains(name, "shanwan");
}

static int is_gamepad_name(const char *name)
{
    return text_contains(name, "gamepad") ||
           text_contains(name, "controller") ||
           text_contains(name, "joystick") ||
           text_contains(name, "8bitdo") ||
           text_contains(name, "gamesir") ||
           text_contains(name, "gulikit") ||
           text_contains(name, "mocute") ||
           text_contains(name, "dualsense") ||
           text_contains(name, "dualshock") ||
           text_contains(name, "btp-") ||
           text_contains(name, "betop") ||
           text_contains(name, "flydigi");
}

static int is_auto_connect_name(const char *name)
{
    return text_contains(name, "xbox") ||
           is_q36_compatible_name(name) ||
           text_contains(name, "btp-") ||
           text_contains(name, "flydigi");
}

static int is_btp_mapping_mode_name(const char *name)
{
    return (text_contains(name, "btp-") || text_contains(name, "betop")) &&
           text_contains(name, "bfm");
}

static int btp_uses_periodic_heartbeat(const char *name)
{
    /* Mirrors JoyU 6.7.9's !u.g(name) && u.h(name) guard. These newer
     * KunPeng variants complete a one-shot PC-handle handshake; sending 21
     * every second makes KP20D close both vendor and HID traffic after about
     * 30 commands. KP50B/KP50C also skip this legacy heartbeat loop. */
    if (text_contains(name, "kp50b") || text_contains(name, "kp50c") ||
        text_contains(name, "kp70a1") || text_contains(name, "kp70a") ||
        text_contains(name, "kp_20dl") || text_contains(name, "kp20d") ||
        text_contains(name, "kp40dk") || text_contains(name, "kp40df") ||
        text_contains(name, "kp40d")) {
        return 0;
    }
    return 1;
}

static int is_flydigi_mapping_mode_name(const char *name)
{
    /* Flydigi's BLE HID mode may expose its physical controls through a
     * vendor GATT channel while the standard HID service describes touch. */
    return text_contains(name, "flydigi");
}

static int text_equal(const char *left, const char *right)
{
    size_t i = 0;
    if (!left || !right) return 0;
    while (left[i] && right[i]) {
        if (ascii_lower(left[i]) != ascii_lower(right[i])) return 0;
        ++i;
    }
    return left[i] == 0 && right[i] == 0;
}

static int hex_digit(char c)
{
    c = ascii_lower(c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int uuid_is16(const char *text, uint16_t expected)
{
    size_t count = 0;
    size_t i;
    uint32_t prefix = 0;
    if (!text) return 0;
    if (text[0] == '0' && ascii_lower(text[1]) == 'x') text += 2;
    for (i = 0; text[i]; ++i) {
        int digit = hex_digit(text[i]);
        if (digit < 0) continue;
        if (count < 8) prefix = (prefix << 4) | (uint32_t)digit;
        count++;
    }
    if (count == 4) return (uint16_t)prefix == expected;
    if (count == 32 && (prefix >> 16) == 0) return (uint16_t)prefix == expected;
    return 0;
}

static const char *phase_text(driver_phase_t phase)
{
    switch (phase) {
    case PHASE_STOPPED: return "stopped";
    case PHASE_SCANNING: return "scanning";
    case PHASE_SELECT_DEVICE: return "select_device";
    case PHASE_CONNECTING: return "connecting";
    case PHASE_PAIRING: return "pairing";
    case PHASE_EXCHANGE_MTU: return "exchange_mtu";
    case PHASE_DISCOVER_SERVICES: return "discover_services";
    case PHASE_DISCOVER_CHARACTERISTICS: return "discover_characteristics";
    case PHASE_SET_PROTOCOL_MODE: return "set_protocol_mode";
    case PHASE_DISCOVER_DESCRIPTORS: return "discover_descriptors";
    case PHASE_READ_REPORT_MAP: return "read_report_map";
    case PHASE_READ_REPORT_REFERENCES: return "read_report_references";
    case PHASE_SUBSCRIBE: return "subscribe";
    case PHASE_VENDOR_DISCOVER_SERVICES: return "vendor_discover_services";
    case PHASE_VENDOR_DISCOVER_CHARACTERISTICS: return "vendor_discover_characteristics";
    case PHASE_VENDOR_DISCOVER_DESCRIPTORS: return "vendor_discover_descriptors";
    case PHASE_VENDOR_SUBSCRIBE: return "vendor_subscribe";
    case PHASE_READY: return "ready";
    case PHASE_WAIT_RESCAN: return "wait_rescan";
    default: return "error";
    }
}

static const char *profile_text(device_profile_t profile)
{
    if (profile == DEVICE_PROFILE_XBOX) return "xbox";
    if (profile == DEVICE_PROFILE_Q36) return "q36-hid";
    return "hid";
}

static int parse_profile_text(const char *text, device_profile_t *profile)
{
    if (!text || !text[0] || !profile) return 0;
    if (text_equal(text, "xbox")) *profile = DEVICE_PROFILE_XBOX;
    else if (text_equal(text, "q36") || text_equal(text, "q36-hid")) *profile = DEVICE_PROFILE_Q36;
    else if (text_equal(text, "hid")) *profile = DEVICE_PROFILE_HID;
    else return 0;
    return 1;
}

static void mark_dirty(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->state.seq++;
    inst->state.timestamp_ms = now_ms(inst);
    inst->state_dirty = 1;
}

static void mark_status_dirty(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->status_dirty = 1;
    mark_dirty(inst);
}

static void finish_command(hidpad_instance_t *inst, const char *error)
{
    if (!inst || inst->cold->command_status != 1) return;
    inst->cold->command_status = error ? 3 : 2;
    inst->cold->command_error = error;
    if (error) inst->last_error = error;
    mark_status_dirty(inst);
}

static const char *command_text(worker_command_t command)
{
    switch (command) {
    case WORKER_COMMAND_START: return "start";
    case WORKER_COMMAND_SCAN: return "scan";
    case WORKER_COMMAND_RESCAN: return "rescan";
    case WORKER_COMMAND_CONNECT: return "connect";
    case WORKER_COMMAND_DISCONNECT: return "disconnect";
    case WORKER_COMMAND_PAIR: return "pair";
    case WORKER_COMMAND_FORGET: return "forget";
    default: return "none";
    }
}

static void finish_command_kind(hidpad_instance_t *inst, worker_command_t kind,
                                const char *error)
{
    if (inst->cold->command_kind == kind) finish_command(inst, error);
}

static void set_error(hidpad_instance_t *inst, const char *error)
{
    if (!inst) return;
    inst->last_error = error;
    inst->phase = PHASE_ERROR;
    finish_command(inst, error);
    mark_status_dirty(inst);
}

static void clear_controls(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->game_buttons = 0;
    inst->consumer_buttons = 0;
    inst->state.buttons = 0;
    inst->state.raw_buttons = 0;
    inst->state.lx = 0;
    inst->state.ly = 0;
    inst->state.rx = 0;
    inst->state.ry = 0;
    inst->state.lt = 0;
    inst->state.rt = 0;
    inst->state.report_id = 0;
}

static void reset_gatt(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->hid_start = 0;
    inst->hid_end = 0;
    inst->report_map_handle = 0;
    inst->control_point_handle = 0;
    inst->control_point_properties = 0;
    inst->protocol_mode_handle = 0;
    inst->protocol_mode_properties = 0;
    inst->next_keepalive_ms = 0;
    inst->next_btp_keepalive_ms = 0;
    inst->cold->connection_params_attempt_count = 0;
    inst->cold->connection_params_last_error = MODULE_ERR_NOT_FOUND;
    inst->btp_last_vendor_notify_ms = 0;
    inst->cold->btp_heartbeat_reply_count = 0;
    inst->cold->btp_handshake_reply_count = 0;
    inst->cold->btp_watchdog_command_count = 0;
    inst->cold->btp_watchdog_reply_count = 0;
    inst->cold->btp_input_read_count = 0;
    inst->cold->ble_non_notify_event_count = 0;
    inst->cold->ble_last_non_notify_ms = 0;
    inst->cold->ble_done_error_count = 0;
    inst->cold->ble_last_non_notify_irq = 0;
    inst->cold->ble_last_non_notify_status = 0;
    inst->input_notify_count = 0;
    inst->cold->last_report_handle = 0;
    inst->cold->last_report_len = 0;

    zero_bytes(inst->reports, sizeof(inst->reports));
    inst->report_count = 0;
    inst->descriptor_index = 0;
    inst->open_report_index = 0xff;
    inst->reference_index = 0;
    inst->subscribe_index = 0;
    inst->subscribed_count = 0;
    inst->next_notification_reconnect_ms = 0;
    inst->input_poll_index = 0;
    inst->hid_init_attempt = 0;
    inst->service_uuid_variant = 0;
    inst->report_map_valid = 0;
    inst->report_map_overflow = 0;
    inst->report_map_len = 0;
    inst->controls_report_handle = 0;
    inst->vendor_start = 0;
    inst->vendor_end = 0;
    inst->vendor_write_handle = 0;
    inst->btp_input_handle = 0;
    inst->cold->btp_last_vendor_handle = 0;
    inst->vendor_write_properties = 0;
    inst->vendor_service_variant = 0;
    inst->vendor_service_count = 0;
    inst->vendor_service_index = 0;
    inst->vendor_channel_count = 0;
    inst->vendor_service_channel_start = 0;
    inst->vendor_service_channel_end = 0;
    inst->vendor_open_channel_index = 0xff;
    inst->vendor_descriptor_index = 0;
    inst->vendor_subscribe_index = 0;
    inst->vendor_subscribed_count = 0;
    inst->vendor_subscribed = 0;
    inst->flydigi_init_stage = 0;
    inst->btp_init_stage = 0;
    inst->btp_missed_heartbeats = 0;
    inst->btp_handshake_pending = 0;
    inst->btp_handshake_sent = 0;
    inst->btp_watchdog_sent = 0;
    inst->btp_seen_battery_reply = 0;
    inst->btp_seen_info_reply = 0;
    inst->cold->btp_last_vendor_len = 0;
    inst->cold->btp_last_handshake_len = 0;
    zero_bytes(inst->vendor_channels, sizeof(inst->vendor_channels));
    zero_bytes(inst->cold->vendor_services, sizeof(inst->cold->vendor_services));
    inst->pending_read = PENDING_READ_NONE;
    inst->pending_report_index = 0;
    hidpad_parser_clear(&inst->parser);
}

static report_characteristic_t *find_report(hidpad_instance_t *inst, uint16_t value_handle)
{
    uint8_t i;
    if (!inst) return NULL;
    for (i = 0; i < inst->report_count; ++i) {
        if (inst->reports[i].value_handle == value_handle) return &inst->reports[i];
    }
    return NULL;
}

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t len)
{
    size_t i;
    if (!left || !right) return 0;
    for (i = 0; i < len; ++i) {
        if (left[i] != right[i]) return 0;
    }
    return 1;
}

static int report_is_duplicate(const report_characteristic_t *report,
                               const uint8_t *data, size_t len)
{
    return report && report->last_report_valid && len <= HIDPAD_REPORT_CACHE_SIZE &&
           report->last_report_len == len && bytes_equal(report->last_report, data, len);
}

static void remember_report(report_characteristic_t *report,
                            const uint8_t *data, size_t len, int decoded)
{
    size_t i;
    if (!report) return;
    report->last_report_valid = 0;
    report->last_decode_ok = decoded ? 1 : 0;
    if (!data || len > HIDPAD_REPORT_CACHE_SIZE) return;
    for (i = 0; i < len; ++i) report->last_report[i] = data[i];
    report->last_report_len = (uint8_t)len;
    report->last_report_valid = 1;
}

static void bytes_to_hex(char *out, size_t out_size, const uint8_t *data, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    size_t i;
    size_t stored_len = len;
    size_t capacity;
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!data) return;
    capacity = (out_size - 1u) / 2u;
    if (stored_len > capacity) stored_len = capacity;
    for (i = 0; i < stored_len; ++i) {
        out[i * 2u] = hex[data[i] >> 4];
        out[i * 2u + 1u] = hex[data[i] & 0x0fu];
    }
    out[stored_len * 2u] = 0;
}

static void remember_input_packet(hidpad_instance_t *inst, uint16_t value_handle,
                                  const uint8_t *data, size_t len)
{
    if (!inst || !data) return;
    size_t cached = len < HIDPAD_REPORT_CACHE_SIZE ? len : HIDPAD_REPORT_CACHE_SIZE;
    memcpy(inst->cold->last_report, data, cached);
    inst->cold->last_report_handle = value_handle;
    inst->cold->last_report_len = (uint8_t)len;
}

static void apply_decoded(hidpad_instance_t *inst,
                          report_characteristic_t *report,
                          const hidpad_decoded_report_t *decoded)
{
    uint32_t old_buttons;
    uint32_t old_raw_buttons;
    int16_t old_lx;
    int16_t old_ly;
    int16_t old_rx;
    int16_t old_ry;
    uint16_t old_lt;
    uint16_t old_rt;
    uint8_t i;
    if (!inst || !decoded) return;
    old_buttons = inst->state.buttons;
    old_raw_buttons = inst->state.raw_buttons;
    old_lx = inst->state.lx;
    old_ly = inst->state.ly;
    old_rx = inst->state.rx;
    old_ry = inst->state.ry;
    old_lt = inst->state.lt;
    old_rt = inst->state.rt;
    if (inst->profile == DEVICE_PROFILE_Q36) {
        uint32_t decoded_buttons = decoded->buttons | decoded->consumer_buttons;
        /* Match the old Q36 controller: Report ID 3 is merged as Consumer
         * state; every other report replaces the complete gamepad state. */
        if (decoded->report_id == 3) {
            inst->consumer_buttons = decoded_buttons;
        } else {
            inst->game_buttons = decoded_buttons;
            inst->state.raw_buttons = decoded->raw_buttons;
            inst->state.lx = decoded->lx;
            inst->state.ly = decoded->ly;
            inst->state.rx = decoded->rx;
            inst->state.ry = decoded->ry;
            inst->state.lt = decoded->lt;
            inst->state.rt = decoded->rt;
        }
        inst->state.buttons = inst->game_buttons | inst->consumer_buttons;
        inst->state.report_id = decoded->report_id;
        if (old_buttons != inst->state.buttons || old_raw_buttons != inst->state.raw_buttons ||
            old_lx != inst->state.lx || old_ly != inst->state.ly ||
            old_rx != inst->state.rx || old_ry != inst->state.ry ||
            old_lt != inst->state.lt || old_rt != inst->state.rt) {
            mark_dirty(inst);
        }
        return;
    }
    if (report) {
        if ((decoded->valid_mask & HIDPAD_VALID_GAME_BUTTONS) != 0) {
            report->game_buttons = decoded->buttons;
            report->raw_buttons = decoded->raw_buttons;
        }
        if ((decoded->valid_mask & HIDPAD_VALID_CONSUMER_BUTTONS) != 0) {
            report->consumer_buttons = decoded->consumer_buttons;
        }
        inst->game_buttons = 0;
        inst->consumer_buttons = 0;
        inst->state.raw_buttons = 0;
        for (i = 0; i < inst->report_count; ++i) {
            inst->game_buttons |= inst->reports[i].game_buttons;
            inst->consumer_buttons |= inst->reports[i].consumer_buttons;
            inst->state.raw_buttons |= inst->reports[i].raw_buttons;
        }
    } else {
        if ((decoded->valid_mask & HIDPAD_VALID_GAME_BUTTONS) != 0) {
            inst->game_buttons = decoded->buttons;
            inst->state.raw_buttons = decoded->raw_buttons;
        }
        if ((decoded->valid_mask & HIDPAD_VALID_CONSUMER_BUTTONS) != 0) {
            inst->consumer_buttons = decoded->consumer_buttons;
        }
    }
    if ((decoded->valid_mask & HIDPAD_VALID_LX) != 0) inst->state.lx = decoded->lx;
    if ((decoded->valid_mask & HIDPAD_VALID_LY) != 0) inst->state.ly = decoded->ly;
    if ((decoded->valid_mask & HIDPAD_VALID_RX) != 0) inst->state.rx = decoded->rx;
    if ((decoded->valid_mask & HIDPAD_VALID_RY) != 0) inst->state.ry = decoded->ry;
    if ((decoded->valid_mask & HIDPAD_VALID_LT) != 0) inst->state.lt = decoded->lt;
    if ((decoded->valid_mask & HIDPAD_VALID_RT) != 0) inst->state.rt = decoded->rt;
    inst->state.buttons = inst->game_buttons | inst->consumer_buttons;
    inst->state.report_id = decoded->report_id;
    if (old_buttons != inst->state.buttons || old_raw_buttons != inst->state.raw_buttons ||
        old_lx != inst->state.lx || old_ly != inst->state.ly ||
        old_rx != inst->state.rx || old_ry != inst->state.ry ||
        old_lt != inst->state.lt || old_rt != inst->state.rt) {
        mark_dirty(inst);
    }
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static int decode_xbox(hidpad_decoded_report_t *decoded, const uint8_t *data, size_t len)
{
    uint16_t lt_raw;
    uint16_t rt_raw;
    uint8_t dpad;
    /* Xbox Elite Series 2 reuses the common 16-byte BLE controls payload and
     * may append profile, trigger-mode and paddle metadata. Basic input only
     * needs the shared prefix; optional Elite data is deliberately ignored. */
    if (!decoded || !data || len < 16) return 0;
    zero_bytes(decoded, sizeof(*decoded));
    decoded->valid_mask = HIDPAD_VALID_GAME_BUTTONS | HIDPAD_VALID_LX | HIDPAD_VALID_LY |
                          HIDPAD_VALID_RX | HIDPAD_VALID_RY | HIDPAD_VALID_LT | HIDPAD_VALID_RT;
    decoded->lx = (int16_t)((int32_t)read_u16(data) - 32768);
    decoded->ly = (int16_t)(32767 - (int32_t)read_u16(data + 2));
    decoded->rx = (int16_t)((int32_t)read_u16(data + 4) - 32768);
    decoded->ry = (int16_t)(32767 - (int32_t)read_u16(data + 6));
    lt_raw = read_u16(data + 8);
    rt_raw = read_u16(data + 10);
    if (lt_raw > 1023u) lt_raw = 1023u;
    if (rt_raw > 1023u) rt_raw = 1023u;
    decoded->lt = (uint16_t)(((uint32_t)lt_raw * 65535u) / 1023u);
    decoded->rt = (uint16_t)(((uint32_t)rt_raw * 65535u) / 1023u);
    dpad = data[12];
    if (dpad == 1 || dpad == 2 || dpad == 8) decoded->buttons |= BTN_UP;
    if (dpad >= 4 && dpad <= 6) decoded->buttons |= BTN_DOWN;
    if (dpad >= 2 && dpad <= 4) decoded->buttons |= BTN_RIGHT;
    if (dpad >= 6 && dpad <= 8) decoded->buttons |= BTN_LEFT;
    if (data[13] & (1u << 0)) decoded->buttons |= BTN_A;
    if (data[13] & (1u << 1)) decoded->buttons |= BTN_B;
    if (data[13] & (1u << 3)) decoded->buttons |= BTN_X;
    if (data[13] & (1u << 4)) decoded->buttons |= BTN_Y;
    if (data[13] & (1u << 6)) decoded->buttons |= BTN_LB;
    if (data[13] & (1u << 7)) decoded->buttons |= BTN_RB;
    if (data[14] & (1u << 2)) decoded->buttons |= BTN_VIEW;
    if (data[14] & (1u << 3)) decoded->buttons |= BTN_MENU;
    if (data[14] & (1u << 4)) decoded->buttons |= BTN_HOME;
    if (data[14] & (1u << 5)) decoded->buttons |= BTN_LS;
    if (data[14] & (1u << 6)) decoded->buttons |= BTN_RS;
    if (data[15] & 1u) decoded->buttons |= BTN_SHARE;
    return 1;
}

static int decode_hid(hidpad_instance_t *inst, report_characteristic_t *report,
                      const uint8_t *data, size_t len)
{
    hidpad_decoded_report_t *decoded;
    hidpad_profile_t profile;
    uint8_t report_id = report ? report->report_id : 0;
    int decoded_ok;
    if (!inst || !data || len == 0) return 0;
    if (report_is_duplicate(report, data, len)) return report->last_decode_ok;
    decoded = &inst->decoded_work;
    if (inst->profile == DEVICE_PROFILE_XBOX && decode_xbox(decoded, data, len)) {
        decoded->report_id = report_id;
        apply_decoded(inst, report, decoded);
        remember_report(report, data, len, 1);
        return 1;
    }
    if (inst->profile == DEVICE_PROFILE_Q36 && inst->parser.field_count == 0 &&
        hidpad_q36_decode_android(report_id, data, len, decoded)) {
        apply_decoded(inst, report, decoded);
        remember_report(report, data, len, 1);
        return 1;
    }
    /* BTP BFM reports are standards-compliant HID, but their physical button
     * labels use the same Usage numbering as Q34/Q36 (1/2/4/5 face buttons,
     * 7/8 shoulders, 9/10 triggers, 11/12 system buttons). Keep the generic
     * Report Map layout while selecting that established button semantic. */
    profile = (inst->profile == DEVICE_PROFILE_Q36 ||
               is_btp_mapping_mode_name(inst->state.name)) ?
              HIDPAD_PROFILE_Q36 : HIDPAD_PROFILE_GENERIC;
    if (inst->profile == DEVICE_PROFILE_Q36 && !inst->parser.has_report_id) report_id = 0;
    decoded_ok = hidpad_parser_decode(&inst->parser, report_id, data, len, profile, decoded);
    if (!decoded_ok) {
        inst->state.report_id = report_id;
        remember_report(report, data, len, 0);
        return 0;
    }
    apply_decoded(inst, report, decoded);
    remember_report(report, data, len, 1);
    return 1;
}

static void parse_advertisement(const uint8_t *data, size_t len, advertisement_t *out)
{
    size_t index = 0;
    zero_bytes(out, sizeof(*out));
    while (index < len) {
        uint8_t field_len = data[index];
        uint8_t type;
        const uint8_t *value;
        size_t value_len;
        size_t i;
        if (field_len == 0 || index + field_len >= len) break;
        type = data[index + 1];
        value = data + index + 2;
        value_len = field_len - 1u;
        if ((type == 0x08 || type == 0x09) && value_len > 0) {
            copy_text(out->name, sizeof(out->name), (const char *)value, value_len);
        } else if (type == 0x19 && value_len >= 2) {
            out->appearance = read_u16(value);
        } else if ((type == 0x02 || type == 0x03) && value_len >= 2) {
            for (i = 0; i + 1 < value_len; i += 2) {
                if (read_u16(value + i) == UUID_HID) out->has_hid = 1;
            }
        } else if (type == 0xff && value_len >= 2) {
            out->company = read_u16(value);
        }
        index += (size_t)field_len + 1u;
    }
}

static int score_advertisement(const advertisement_t *adv, device_profile_t *profile)
{
    int score = 0;
    int q36_compatible;
    if (text_contains(adv->name, "xbox") ||
        (adv->appearance == 0x03c4 && adv->company == 0x0006)) {
        *profile = DEVICE_PROFILE_XBOX;
        return 240;
    }
    q36_compatible = is_q36_compatible_name(adv->name);
    if (q36_compatible) {
        *profile = DEVICE_PROFILE_Q36;
    } else {
        *profile = DEVICE_PROFILE_HID;
    }
    if (q36_compatible) score += 100;
    /* 0x1812 alone also identifies keyboards, mice and other HID devices.
     * Keep it as supporting evidence, but require a gamepad appearance/name
     * before presenting an unpaired device as a controller. */
    if (adv->has_hid) score += 20;
    if (adv->appearance == 0x03c4 || adv->appearance == 0x03c3) score += 80;
    if (is_gamepad_name(adv->name)) score += 45;
    return score;
}

static void schedule_rescan(hidpad_instance_t *inst, uint32_t delay_ms);
static void schedule_rescan_with_backoff(hidpad_instance_t *inst);

static int start_scan(hidpad_instance_t *inst)
{
    module_ble_scan_config_t *scan;
    int32_t err;
    if (!inst || !inst->started || !inst->host->ble.gap_scan) return 0;
    if (!inst->auto_connect && !inst->manual_scan && !inst->notification_reconnect_pending) {
        inst->phase = PHASE_SELECT_DEVICE;
        mark_status_dirty(inst);
        return 1;
    }
    /* Auto-connect decisions must only use advertisements observed in this
     * scan. Keeping results from an earlier round makes a powered-off device
     * look present and causes an endless connect/timeout loop. Manual scan
     * callers already clear the selection list before entering here. */
    if (!inst->manual_scan) {
        inst->scan_result_count = 0;
        zero_bytes(inst->cold->scan_results, sizeof(inst->cold->scan_results));
    }
    scan = &inst->cold->scan_work;
    zero_bytes(scan, sizeof(*scan));
    scan->size = sizeof(*scan);
    scan->duration_ms = inst->manual_scan || inst->scan_ms <= 3000u ? inst->scan_ms : 3000u;
    scan->interval_us = inst->manual_scan ? 48000u : 160000u;
    scan->window_us = inst->manual_scan ? 30000u : 20000u;
    scan->active = 1;
    err = inst->host->ble.gap_scan(inst->session, scan);
    if (err != MODULE_OK) {
        schedule_rescan_with_backoff(inst);
        return 0;
    }
    inst->scan_active = 1;
    if (!inst->state.connected) inst->phase = PHASE_SCANNING;
    mark_status_dirty(inst);
    return 1;
}

static void schedule_rescan(hidpad_instance_t *inst, uint32_t delay_ms)
{
    if (!inst) return;
    inst->scan_active = 0;
    inst->next_scan_ms = now_ms(inst) + delay_ms;
    inst->phase = (!inst->auto_connect && !inst->manual_scan && !inst->notification_reconnect_pending) ?
                  PHASE_SELECT_DEVICE : PHASE_WAIT_RESCAN;
    mark_status_dirty(inst);
}

static void schedule_rescan_with_backoff(hidpad_instance_t *inst)
{
    uint32_t next;
    if (!inst) return;
    schedule_rescan(inst, inst->rescan_backoff_ms);
    if (inst->rescan_backoff_ms >= HIDPAD_RESCAN_MAX_MS) return;
    next = inst->rescan_backoff_ms * 2u;
    inst->rescan_backoff_ms = next > HIDPAD_RESCAN_MAX_MS ? HIDPAD_RESCAN_MAX_MS : next;
}

static int start_read(hidpad_instance_t *inst, uint16_t handle,
                      pending_read_t kind, uint8_t report_index)
{
    int32_t err;
    if (!inst || !handle || inst->pending_read != PENDING_READ_NONE) return 0;
    err = inst->host->ble.gattc_read(inst->session, inst->conn_handle, handle);
    if (err != MODULE_OK) return 0;
    inst->pending_read = kind;
    inst->pending_report_index = report_index;
    return 1;
}

static int decode_flydigi_v2(hidpad_decoded_report_t *decoded,
                             const uint8_t *data, size_t len)
{
    const uint8_t *packet = data;
    uint8_t buttons1;
    uint8_t buttons2;
    int16_t axis;
    if (!decoded || !data || len == 0) return 0;
    /* USB transports prepend report ID 0x03. The BLE vendor characteristic
     * may preserve it, so accept both framed forms. */
    if (packet[0] != 0x5au) {
        packet++;
        len--;
    }
    if (len < 17 || packet[0] != 0x5au || packet[1] != 0xa5u ||
        packet[2] != 0xefu) return 0;
    zero_bytes(decoded, sizeof(*decoded));
    decoded->valid_mask = HIDPAD_VALID_GAME_BUTTONS | HIDPAD_VALID_LX | HIDPAD_VALID_LY |
                          HIDPAD_VALID_RX | HIDPAD_VALID_RY | HIDPAD_VALID_LT | HIDPAD_VALID_RT;
    decoded->report_id = 0xefu;
    decoded->lx = (int16_t)read_u16(packet + 3);
    axis = (int16_t)read_u16(packet + 5);
    decoded->ly = axis == (int16_t)0x8000 ? 32767 : (int16_t)-axis;
    decoded->rx = (int16_t)read_u16(packet + 7);
    axis = (int16_t)read_u16(packet + 9);
    decoded->ry = axis == (int16_t)0x8000 ? 32767 : (int16_t)-axis;
    decoded->lt = (uint16_t)((uint16_t)packet[15] * 257u);
    decoded->rt = (uint16_t)((uint16_t)packet[16] * 257u);
    buttons1 = packet[11];
    buttons2 = packet[12];
    if (buttons1 & 0x01u) decoded->buttons |= BTN_UP;
    if (buttons1 & 0x02u) decoded->buttons |= BTN_RIGHT;
    if (buttons1 & 0x04u) decoded->buttons |= BTN_DOWN;
    if (buttons1 & 0x08u) decoded->buttons |= BTN_LEFT;
    if (buttons1 & 0x10u) decoded->buttons |= BTN_A;
    if (buttons1 & 0x20u) decoded->buttons |= BTN_B;
    if (buttons1 & 0x40u) decoded->buttons |= BTN_VIEW;
    if (buttons1 & 0x80u) decoded->buttons |= BTN_X;
    if (buttons2 & 0x01u) decoded->buttons |= BTN_Y;
    if (buttons2 & 0x02u) decoded->buttons |= BTN_MENU;
    if (buttons2 & 0x04u) decoded->buttons |= BTN_LB;
    if (buttons2 & 0x08u) decoded->buttons |= BTN_RB;
    if (buttons2 & 0x40u) decoded->buttons |= BTN_LS;
    if (buttons2 & 0x80u) decoded->buttons |= BTN_RS;
    if (packet[14] & 0x08u) decoded->buttons |= BTN_HOME;
    decoded->raw_buttons = decoded->buttons;
    return 1;
}

static int16_t flydigi_smart_axis(uint8_t value)
{
    int16_t centered = (int16_t)value - 128;
    /* APEX 5 idles between 0x80 and 0x81. Keep that one-count transport
     * jitter from making a centered stick look permanently displaced. */
    if (centered >= -1 && centered <= 1) return 0;
    return (int16_t)(centered * 256);
}

static int decode_flydigi_smart(hidpad_decoded_report_t *decoded,
                                 const uint8_t *data, size_t len)
{
    uint8_t buttons0;
    uint8_t buttons1;
    if (!decoded || !data) return 0;
    /* Game Center cb/b.M accepts the 14-byte legacy packet and the 20-byte
     * operation packet ending in FE 00. In this format bytes 0..3 are the
     * four axes, 4..5 are key bitmaps, and 6..7 are the linear triggers. */
    if (len != 14 &&
        (len != 20 || data[18] != 0xfeu || data[19] != 0x00u)) return 0;

    zero_bytes(decoded, sizeof(*decoded));
    decoded->valid_mask = HIDPAD_VALID_GAME_BUTTONS | HIDPAD_VALID_LX | HIDPAD_VALID_LY |
                          HIDPAD_VALID_RX | HIDPAD_VALID_RY | HIDPAD_VALID_LT | HIDPAD_VALID_RT;
    decoded->report_id = 0xfeu;
    decoded->lx = flydigi_smart_axis(data[0]);
    decoded->ly = flydigi_smart_axis(data[1]);
    decoded->rx = flydigi_smart_axis(data[2]);
    decoded->ry = flydigi_smart_axis(data[3]);
    decoded->lt = (uint16_t)((uint16_t)data[6] * 257u);
    decoded->rt = (uint16_t)((uint16_t)data[7] * 257u);

    /* APEX 5 Android smart-mode captures use the same two-byte key bitmap as
     * the Flydigi V2 report: d-pad/A/B/View/X in byte 4, then
     * Y/Menu/LB/RB/LT/RT/L3/R3 in byte 5. Triggers also carry their analog
     * values in bytes 6..7, and Home is byte 8 bit 3. */
    buttons0 = data[4];
    buttons1 = data[5];
    if (buttons0 & 0x01u) decoded->buttons |= BTN_UP;
    if (buttons0 & 0x02u) decoded->buttons |= BTN_RIGHT;
    if (buttons0 & 0x04u) decoded->buttons |= BTN_DOWN;
    if (buttons0 & 0x08u) decoded->buttons |= BTN_LEFT;
    if (buttons0 & 0x10u) decoded->buttons |= BTN_A;
    if (buttons0 & 0x20u) decoded->buttons |= BTN_B;
    if (buttons0 & 0x40u) decoded->buttons |= BTN_VIEW;
    if (buttons0 & 0x80u) decoded->buttons |= BTN_X;
    if (buttons1 & 0x01u) decoded->buttons |= BTN_Y;
    if (buttons1 & 0x02u) decoded->buttons |= BTN_MENU;
    if (buttons1 & 0x04u) decoded->buttons |= BTN_LB;
    if (buttons1 & 0x08u) decoded->buttons |= BTN_RB;
    if (buttons1 & 0x40u) decoded->buttons |= BTN_LS;
    if (buttons1 & 0x80u) decoded->buttons |= BTN_RS;
    if (len > 8 && (data[8] & 0x08u)) decoded->buttons |= BTN_HOME;
    decoded->raw_buttons = decoded->buttons;
    return 1;
}

static void decode_vendor_input(hidpad_instance_t *inst,
                                const uint8_t *data, size_t len)
{
    hidpad_decoded_report_t *decoded;
    if (!inst || !data || !is_flydigi_mapping_mode_name(inst->state.name)) return;
    decoded = &inst->decoded_work;
    if (decode_flydigi_smart(decoded, data, len) ||
        decode_flydigi_v2(decoded, data, len)) {
        apply_decoded(inst, NULL, decoded);
    }
}

static int start_report_map_read(hidpad_instance_t *inst)
{
    int32_t err;
    if (!inst || !inst->report_map_handle || inst->pending_read != PENDING_READ_NONE) return 0;
    inst->report_map_len = 0;
    inst->report_map_overflow = 0;
    if (inst->profile == DEVICE_PROFILE_HID && inst->host->ble.gattc_read_long) {
        err = inst->host->ble.gattc_read_long(inst->session, inst->conn_handle,
                                              inst->report_map_handle, 0);
    } else {
        err = inst->host->ble.gattc_read(inst->session, inst->conn_handle,
                                         inst->report_map_handle);
    }
    if (err != MODULE_OK) return 0;
    inst->pending_read = PENDING_READ_MAP;
    inst->pending_report_index = 0;
    return 1;
}

static void begin_subscribe(hidpad_instance_t *inst);
static void start_hid_service_discovery(hidpad_instance_t *inst);
static void read_next_reference(hidpad_instance_t *inst);
static void discover_next_report_descriptors(hidpad_instance_t *inst);
static int begin_vendor_service_discovery(hidpad_instance_t *inst);

static void fail_hid_initialization(hidpad_instance_t *inst, const char *error)
{
    uint8_t next_attempt;
    if (!inst) return;
    if (inst->profile == DEVICE_PROFILE_Q36 &&
        inst->hid_init_attempt + 1u < HIDPAD_Q36_INIT_ATTEMPTS) {
        next_attempt = (uint8_t)(inst->hid_init_attempt + 1u);
        reset_gatt(inst);
        inst->hid_init_attempt = next_attempt;
        start_hid_service_discovery(inst);
        return;
    }
    set_error(inst, error);
    inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
}

static int report_is_subscription_candidate(const hidpad_instance_t *inst,
                                            const report_characteristic_t *report)
{
    if (!inst || !report || !report->cccd_handle) return 0;
    if (inst->profile == DEVICE_PROFILE_XBOX) {
        return report->value_handle == inst->controls_report_handle;
    }
    /* Match Android HOGP: enable every characteristic identified as an Input
     * Report. Some controllers expose a normally silent secondary report but
     * still expect the host to configure all input CCCDs. */
    return (report->report_type == 0 || report->report_type == 1) &&
           (report->properties &
            (MODULE_BLE_CHAR_PROP_NOTIFY | MODULE_BLE_CHAR_PROP_INDICATE)) != 0;
}

static int report_is_read_candidate(const report_characteristic_t *report)
{
    return report && (report->report_type == 0 || report->report_type == 1) &&
           (report->properties & MODULE_BLE_CHAR_PROP_READ) != 0;
}

static int has_readable_input_report(const hidpad_instance_t *inst)
{
    uint8_t i;
    if (!inst) return 0;
    for (i = 0; i < inst->report_count; ++i) {
        if (report_is_read_candidate(&inst->reports[i])) return 1;
    }
    return 0;
}

static int uuid_to16(const char *text, uint16_t *out)
{
    size_t count = 0;
    size_t i;
    uint32_t prefix = 0;
    if (!text || !out) return 0;
    if (text[0] == '0' && ascii_lower(text[1]) == 'x') text += 2;
    for (i = 0; text[i]; ++i) {
        int digit = hex_digit(text[i]);
        if (digit < 0) continue;
        if (count < 8) prefix = (prefix << 4) | (uint32_t)digit;
        count++;
    }
    if (count == 4) {
        *out = (uint16_t)prefix;
        return 1;
    }
    if (count == 32 && (prefix >> 16) == 0) {
        *out = (uint16_t)prefix;
        return 1;
    }
    return 0;
}

static int is_standard_service_uuid(const char *uuid)
{
    uint16_t short_uuid = 0;
    return uuid_to16(uuid, &short_uuid) && short_uuid >= 0x1800u && short_uuid <= 0x18ffu;
}

static void complete_ready(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->phase = PHASE_READY;
    finish_command_kind(inst, WORKER_COMMAND_CONNECT, NULL);
    finish_command_kind(inst, WORKER_COMMAND_RESCAN, NULL);
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->last_error = NULL;
    inst->state.disconnect_reason = 0;
    /* Record whether a generic HID link update request was accepted instead
     * of silently discarding its result. BTP/BFM must keep its negotiated
     * parameters: forcing 7.5-10 ms was accepted locally but stopped KP20D
     * input immediately, and JoyU does not request high connection priority. */
    if (!is_btp_mapping_mode_name(inst->state.name) &&
        inst->host->ble.gap_set_connection_params) {
        inst->cold->connection_params_attempt_count++;
        inst->cold->connection_params_last_error =
            inst->host->ble.gap_set_connection_params(
            inst->session, inst->conn_handle,
            HIDPAD_CONN_INTERVAL_MIN, HIDPAD_CONN_INTERVAL_MAX,
            HIDPAD_CONN_LATENCY, HIDPAD_CONN_SUPERVISION_TIMEOUT);
    } else {
        inst->cold->connection_params_last_error = MODULE_ERR_UNSUPPORTED;
    }
    inst->next_input_poll_ms = now_ms(inst) + 80;
    /* Q34/Q36 sometimes needs a clean second link before its first report.
     * Generic HOGP, including BTP BFM, leaves a successful CCCD subscription
     * undisturbed because quiet notification intervals are valid. */
    inst->next_notification_reconnect_ms =
        HIDPAD_ENABLE_NOTIFY_RECONNECT && inst->profile == DEVICE_PROFILE_Q36 &&
        inst->notification_reconnect_count == 0 ?
        now_ms(inst) + HIDPAD_NOTIFY_RECONNECT_MS : 0;
    if (is_btp_mapping_mode_name(inst->state.name)) {
        inst->next_keepalive_ms = 0;
        inst->btp_init_stage = 0;
        inst->btp_last_vendor_notify_ms = now_ms(inst);
        inst->next_btp_keepalive_ms =
            inst->vendor_subscribed && inst->vendor_write_handle ?
            now_ms(inst) + HIDPAD_BTP_START_DELAY_MS : 0;
    } else {
        inst->next_keepalive_ms = now_ms(inst);
        inst->next_btp_keepalive_ms = 0;
    }
    mark_status_dirty(inst);
}

static void finish_subscribe(hidpad_instance_t *inst)
{
    if (!inst) return;
    if (inst->subscribed_count == 0 &&
        (inst->profile != DEVICE_PROFILE_HID || !has_readable_input_report(inst)) &&
        !is_btp_mapping_mode_name(inst->state.name) &&
        !is_flydigi_mapping_mode_name(inst->state.name)) {
        fail_hid_initialization(inst, "No usable HID input report");
        return;
    }
    /* Flydigi carries controls on its vendor stream. KP20D/BFM keeps standard
     * HID input alive through its separate 7310 command/reply service. */
    if ((is_flydigi_mapping_mode_name(inst->state.name) ||
         is_btp_mapping_mode_name(inst->state.name)) &&
        begin_vendor_service_discovery(inst)) {
        return;
    }
    complete_ready(inst);
}

static void begin_report_map_and_reference_reads(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->reference_index = 0;
    inst->report_map_valid = 0;
    if (inst->report_map_handle && start_report_map_read(inst)) {
        inst->phase = PHASE_READ_REPORT_MAP;
    } else if (inst->profile != DEVICE_PROFILE_XBOX) {
        fail_hid_initialization(inst, "HID report map not readable");
    } else {
        read_next_reference(inst);
    }
}

static void begin_protocol_mode(hidpad_instance_t *inst)
{
    uint8_t report_protocol = 1;
    uint32_t mode;
    int32_t err;
    if (!inst) return;
    if (inst->profile == DEVICE_PROFILE_HID &&
        !is_btp_mapping_mode_name(inst->state.name) &&
        inst->protocol_mode_handle &&
        (inst->protocol_mode_properties &
         (MODULE_BLE_CHAR_PROP_WRITE | MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE)) != 0) {
        /* HOGP Protocol Mode uses a Write Command. Prefer it when both write
         * properties are advertised, matching Android's HID host. */
        mode = (inst->protocol_mode_properties & MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE) != 0 ?
               MODULE_BLE_WRITE_NO_RESPONSE : MODULE_BLE_WRITE_WITH_RESPONSE;
        inst->phase = PHASE_SET_PROTOCOL_MODE;
        mark_status_dirty(inst);
        err = inst->host->ble.gattc_write(inst->session, inst->conn_handle,
                                          inst->protocol_mode_handle,
                                          &report_protocol, sizeof(report_protocol), mode);
        if (err == MODULE_OK && mode == MODULE_BLE_WRITE_WITH_RESPONSE) return;
        /* Write Without Response completes synchronously here. Report mode is
         * the HOGP default, so failure of this optional hint is non-fatal. */
    }
    inst->descriptor_index = 0;
    discover_next_report_descriptors(inst);
}

static void discover_next_report_descriptors(hidpad_instance_t *inst)
{
    if (!inst) return;
    while (inst->descriptor_index < inst->report_count) {
        report_characteristic_t *report = &inst->reports[inst->descriptor_index];
        uint16_t end_handle = report->descriptor_end_handle;
        if (end_handle < report->value_handle) end_handle = report->value_handle;
        if (inst->host->ble.gattc_discover_descriptors(
                inst->session, inst->conn_handle,
                report->value_handle, end_handle) == MODULE_OK) {
            inst->phase = PHASE_DISCOVER_DESCRIPTORS;
            return;
        }
        inst->descriptor_index++;
    }
    begin_report_map_and_reference_reads(inst);
}

static void read_next_reference(hidpad_instance_t *inst)
{
    while (inst->reference_index < inst->report_count) {
        uint8_t index = inst->reference_index;
        report_characteristic_t *report = &inst->reports[index];
        if (report->reference_handle && start_read(inst, report->reference_handle,
                                                   PENDING_READ_REFERENCE, index)) {
            inst->phase = PHASE_READ_REPORT_REFERENCES;
            return;
        }
        inst->reference_index++;
    }
    begin_subscribe(inst);
}

static void subscribe_next(hidpad_instance_t *inst)
{
    while (inst->subscribe_index < inst->report_count) {
        report_characteristic_t *report = &inst->reports[inst->subscribe_index];
        uint8_t value[2] = {1, 0};
        int32_t err;
        if (!report_is_subscription_candidate(inst, report)) {
            inst->subscribe_index++;
            continue;
        }
        if ((report->properties & MODULE_BLE_CHAR_PROP_NOTIFY) == 0) value[0] = 2;
        err = inst->host->ble.gattc_write(inst->session, inst->conn_handle,
                                          report->cccd_handle, value, sizeof(value),
                                          MODULE_BLE_WRITE_WITH_RESPONSE);
        if (err == MODULE_OK) return;
        inst->subscribe_index++;
    }
    finish_subscribe(inst);
}

static void begin_subscribe(hidpad_instance_t *inst)
{
    uint8_t i;
    inst->phase = PHASE_SUBSCRIBE;
    inst->subscribe_index = 0;
    inst->subscribed_count = 0;
    inst->controls_report_handle = 0;
    if (inst->profile == DEVICE_PROFILE_XBOX) {
        /* The old LiteXboxController selected only the first 0x2A4D
         * characteristic that can notify. */
        for (i = 0; i < inst->report_count; ++i) {
            report_characteristic_t *report = &inst->reports[i];
            if (report->cccd_handle &&
                (report->properties & MODULE_BLE_CHAR_PROP_NOTIFY) != 0) {
                inst->controls_report_handle = report->value_handle;
                break;
            }
        }
    }
    subscribe_next(inst);
}

static const char *vendor_service_uuid(const hidpad_instance_t *inst)
{
    if (is_btp_mapping_mode_name(inst ? inst->state.name : NULL)) {
        return UUID_BTP_VENDOR_SERVICE_TEXT;
    }
    if (is_flydigi_mapping_mode_name(inst ? inst->state.name : NULL)) {
        if (inst->vendor_service_variant == 0) return UUID_FLYDIGI_VENDOR_SERVICE_TEXT;
        if (inst->vendor_service_variant == 1) return UUID_FLYDIGI_NUS_SERVICE_TEXT;
    }
    return NULL;
}

static int vendor_all_services_mode(const hidpad_instance_t *inst)
{
    if (!inst) return 0;
    if (is_flydigi_mapping_mode_name(inst->state.name)) return inst->vendor_service_variant >= 2;
    return 0;
}

static int vendor_service_matches(const hidpad_instance_t *inst, const char *uuid)
{
    if (!inst || !uuid) return 0;
    if (is_btp_mapping_mode_name(inst->state.name)) {
        return uuid_is16(uuid, UUID_BTP_VENDOR_SERVICE);
    }
    if (vendor_all_services_mode(inst)) return !is_standard_service_uuid(uuid);
    if (inst->vendor_service_variant == 0) {
        return uuid_is16(uuid, UUID_FLYDIGI_VENDOR_SERVICE);
    }
    return text_equal(uuid, UUID_FLYDIGI_NUS_SERVICE_TEXT);
}

static int vendor_notify_matches(const hidpad_instance_t *inst, const char *uuid)
{
    if (!inst || !uuid) return 0;
    if (is_btp_mapping_mode_name(inst->state.name)) {
        return uuid_is16(uuid, UUID_BTP_VENDOR_INPUT) ||
               uuid_is16(uuid, UUID_BTP_VENDOR_REPLY);
    }
    if (vendor_all_services_mode(inst)) return 1;
    if (inst->vendor_service_variant == 0) {
        return uuid_is16(uuid, UUID_FLYDIGI_VENDOR_NOTIFY);
    }
    return text_equal(uuid, UUID_FLYDIGI_NUS_NOTIFY_TEXT);
}

static int request_vendor_service_discovery(hidpad_instance_t *inst)
{
    const char *uuid = vendor_service_uuid(inst);
    if (!inst || (!uuid && !vendor_all_services_mode(inst))) return 0;
    inst->vendor_start = 0;
    inst->vendor_end = 0;
    inst->vendor_write_handle = 0;
    inst->vendor_write_properties = 0;
    inst->vendor_channel_count = 0;
    inst->vendor_service_channel_start = 0;
    inst->vendor_service_channel_end = 0;
    inst->vendor_open_channel_index = 0xff;
    inst->vendor_descriptor_index = 0;
    inst->vendor_subscribe_index = 0;
    inst->vendor_subscribed_count = 0;
    zero_bytes(inst->vendor_channels, sizeof(inst->vendor_channels));
    if (vendor_all_services_mode(inst)) {
        inst->vendor_service_count = 0;
        inst->vendor_service_index = 0;
        zero_bytes(inst->cold->vendor_services, sizeof(inst->cold->vendor_services));
    }
    inst->phase = PHASE_VENDOR_DISCOVER_SERVICES;
    mark_status_dirty(inst);
    return inst->host->ble.gattc_discover_services(
               inst->session, inst->conn_handle, uuid) == MODULE_OK;
}

static int request_vendor_characteristic_discovery(hidpad_instance_t *inst)
{
    if (!inst || !inst->vendor_start || !inst->vendor_end) return 0;
    inst->vendor_service_channel_start = inst->vendor_channel_count;
    inst->vendor_service_channel_end = inst->vendor_channel_count;
    inst->vendor_open_channel_index = 0xff;
    inst->phase = PHASE_VENDOR_DISCOVER_CHARACTERISTICS;
    mark_status_dirty(inst);
    return inst->host->ble.gattc_discover_characteristics(
               inst->session, inst->conn_handle,
               inst->vendor_start, inst->vendor_end, NULL) == MODULE_OK;
}

static int select_vendor_service_candidate(hidpad_instance_t *inst, uint8_t index)
{
    vendor_service_candidate_t *service;
    if (!inst || index >= inst->vendor_service_count) return 0;
    service = &inst->cold->vendor_services[index];
    inst->vendor_service_index = index;
    inst->vendor_start = service->start_handle;
    inst->vendor_end = service->end_handle;
    return request_vendor_characteristic_discovery(inst);
}

static int advance_vendor_service_discovery(hidpad_instance_t *inst)
{
    if (!inst) return 0;
    if (vendor_all_services_mode(inst)) {
        uint8_t next = (uint8_t)(inst->vendor_service_index + 1u);
        return next < inst->vendor_service_count ?
               select_vendor_service_candidate(inst, next) : 0;
    }
    if (is_flydigi_mapping_mode_name(inst->state.name) &&
        inst->vendor_service_variant < 2) {
        inst->vendor_service_variant++;
        return request_vendor_service_discovery(inst);
    }
    return 0;
}

static vendor_channel_t *find_vendor_channel(hidpad_instance_t *inst,
                                             uint16_t value_handle)
{
    uint8_t i;
    if (!inst) return NULL;
    for (i = 0; i < inst->vendor_channel_count; ++i) {
        if (inst->vendor_channels[i].value_handle == value_handle) {
            return &inst->vendor_channels[i];
        }
    }
    return NULL;
}

static void begin_vendor_subscribe(hidpad_instance_t *inst);

static void finish_vendor_service(hidpad_instance_t *inst)
{
    if (!inst) return;
    if (advance_vendor_service_discovery(inst)) return;
    begin_vendor_subscribe(inst);
}

static void discover_next_vendor_descriptor(hidpad_instance_t *inst)
{
    while (inst && inst->vendor_descriptor_index < inst->vendor_service_channel_end) {
        vendor_channel_t *channel =
            &inst->vendor_channels[inst->vendor_descriptor_index];
        inst->phase = PHASE_VENDOR_DISCOVER_DESCRIPTORS;
        mark_status_dirty(inst);
        if (inst->host->ble.gattc_discover_descriptors(
                inst->session, inst->conn_handle, channel->value_handle,
                channel->descriptor_end_handle) == MODULE_OK) {
            return;
        }
        inst->vendor_descriptor_index++;
    }
    finish_vendor_service(inst);
}

static void subscribe_next_vendor_channel(hidpad_instance_t *inst)
{
    while (inst && inst->vendor_subscribe_index < inst->vendor_channel_count) {
        vendor_channel_t *channel =
            &inst->vendor_channels[inst->vendor_subscribe_index];
        uint8_t value[2] = {1, 0};
        if (!channel->cccd_handle) {
            inst->vendor_subscribe_index++;
            continue;
        }
        /* JoyU explicitly enables notifications on both 7311 and 7313 even
         * when KP20D omits the Notify property bit from one declaration. */
        if (!is_btp_mapping_mode_name(inst->state.name) &&
            (channel->properties & MODULE_BLE_CHAR_PROP_NOTIFY) == 0) {
            value[0] = 2;
        }
        inst->phase = PHASE_VENDOR_SUBSCRIBE;
        mark_status_dirty(inst);
        if (inst->host->ble.gattc_write(
                inst->session, inst->conn_handle, channel->cccd_handle,
                value, sizeof(value), MODULE_BLE_WRITE_WITH_RESPONSE) == MODULE_OK) {
            return;
        }
        inst->vendor_subscribe_index++;
    }
    inst->vendor_subscribed = inst->vendor_subscribed_count > 0 ? 1u : 0u;
    if (is_btp_mapping_mode_name(inst->state.name) &&
        inst->btp_input_handle && inst->host->ble.gattc_read &&
        inst->host->ble.gattc_read(inst->session, inst->conn_handle,
                                   inst->btp_input_handle) == MODULE_OK) {
        inst->cold->btp_input_read_count++;
    }
    complete_ready(inst);
}

static void begin_vendor_subscribe(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->vendor_subscribe_index = 0;
    inst->vendor_subscribed_count = 0;
    inst->vendor_subscribed = 0;
    subscribe_next_vendor_channel(inst);
}

static int begin_vendor_service_discovery(hidpad_instance_t *inst)
{
    int btp_mode;
    if (!inst) return 0;
    btp_mode = is_btp_mapping_mode_name(inst->state.name);
    if (!btp_mode && !is_flydigi_mapping_mode_name(inst->state.name)) return 0;
    /* Flydigi Android BLE mode may expose more than one proprietary transport.
     * Enumerate all of them so a successfully subscribed configuration
     * channel cannot hide the actual controller input channel. */
    inst->vendor_service_variant = btp_mode ? 0u : 2u;
    inst->vendor_subscribed = 0;
    inst->vendor_service_count = 0;
    inst->vendor_service_index = 0;
    inst->vendor_channel_count = 0;
    inst->vendor_subscribed_count = 0;
    return request_vendor_service_discovery(inst);
}

static discovered_device_t *remember_device(hidpad_instance_t *inst,
                                             const module_ble_event_t *event,
                                             const advertisement_t *adv,
                                             device_profile_t profile,
                                             int score)
{
    discovered_device_t *device = NULL;
    uint8_t i;
    if (!inst || !event || !adv) return NULL;
    for (i = 0; i < inst->scan_result_count; ++i) {
        if (text_equal(inst->cold->scan_results[i].address, event->address)) {
            device = &inst->cold->scan_results[i];
            break;
        }
    }
    if (!device && inst->scan_result_count < HIDPAD_MAX_SCAN_RESULTS) {
        device = &inst->cold->scan_results[inst->scan_result_count++];
        zero_bytes(device, sizeof(*device));
    }
    if (!device && inst->scan_result_count == HIDPAD_MAX_SCAN_RESULTS) {
        uint8_t weakest = 0;
        for (i = 1; i < HIDPAD_MAX_SCAN_RESULTS; ++i) {
            if (inst->cold->scan_results[i].score < inst->cold->scan_results[weakest].score) weakest = i;
        }
        if ((inst->cold->preferred_address[0] &&
             text_equal(inst->cold->preferred_address, event->address)) ||
            score > inst->cold->scan_results[weakest].score ||
            (inst->manual_scan && score == inst->cold->scan_results[weakest].score &&
             event->rssi > inst->cold->scan_results[weakest].rssi)) {
            device = &inst->cold->scan_results[weakest];
            zero_bytes(device, sizeof(*device));
        }
    }
    if (!device) return NULL;
    copy_text(device->address, sizeof(device->address), event->address, strlen(event->address));
    if (adv->name[0]) {
        copy_text(device->name, sizeof(device->name), adv->name, strlen(adv->name));
    } else if (!device->name[0]) {
        int preferred = inst->cold->preferred_address[0] &&
                        text_equal(inst->cold->preferred_address, event->address);
        const char *fallback =
            preferred && inst->cold->preferred_metadata_valid &&
            inst->cold->preferred_name[0] ? inst->cold->preferred_name :
            (profile == DEVICE_PROFILE_XBOX ?
             "Xbox Wireless Controller" : "BLE HID Gamepad");
        copy_text(device->name, sizeof(device->name), fallback, strlen(fallback));
    }
    device->rssi = event->rssi;
    device->addr_type = event->addr_type;
    if (device->score == 0 || score >= device->score) {
        device->profile = (uint8_t)profile;
        device->score = (uint8_t)(score > 255 ? 255 : score);
    }
    return device;
}

static int connect_peer(hidpad_instance_t *inst, uint8_t addr_type, const char *address,
                        const char *name, device_profile_t profile, uint32_t timeout_ms)
{
    int32_t err;
    if (!inst || !address || !address[0]) return 0;
    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
    inst->scan_active = 0;
    inst->profile = profile;
    inst->peer_addr_type = addr_type;
    copy_text(inst->state.address, sizeof(inst->state.address), address, strlen(address));
    copy_text(inst->state.name, sizeof(inst->state.name), name, strlen(name));
    inst->state.connecting = 1;
    inst->last_error = NULL;
    inst->state.disconnect_reason = 0;
    inst->phase = PHASE_CONNECTING;
    mark_status_dirty(inst);
    err = inst->host->ble.gap_connect(inst->session, addr_type, address, timeout_ms);
    if (err != MODULE_OK) {
        inst->state.connecting = 0;
        inst->phase = inst->manual_scan ? PHASE_SELECT_DEVICE : PHASE_WAIT_RESCAN;
        mark_status_dirty(inst);
        return 0;
    }
    inst->manual_scan = 0;
    return 1;
}

static int should_auto_connect(const hidpad_instance_t *inst,
                               const discovered_device_t *device)
{
    if (!inst || !device) return 0;
    /* A user-initiated scan is selection-only. Never race the Connect button
     * by automatically pairing a preferred/Xbox/Q36 device from its results. */
    if (inst->notification_reconnect_pending)
        return text_equal(inst->state.address, device->address);
    if (!inst->auto_connect || inst->manual_scan) return 0;
    /* A saved controller is preferred, not an exclusive address filter. */
    if (inst->cold->preferred_address[0] &&
        text_equal(inst->cold->preferred_address, device->address)) return 1;
    return is_auto_connect_name(device->name);
}

static int connect_device(hidpad_instance_t *inst, const discovered_device_t *device)
{
    int connected;
    if (!inst || !device) return 0;
    connected = connect_peer(inst, device->addr_type, device->address, device->name,
                             (device_profile_t)device->profile, 15000u);
    if (connected && inst->cold->preferred_address[0] &&
        text_equal(inst->cold->preferred_address, device->address)) {
        inst->cold->preferred_addr_type = device->addr_type;
        inst->cold->preferred_profile = (device_profile_t)device->profile;
        copy_text(inst->cold->preferred_name, sizeof(inst->cold->preferred_name),
                  device->name, strlen(device->name));
        inst->cold->preferred_metadata_valid = 1;
    }
    return connected;
}

static discovered_device_t *select_auto_device(hidpad_instance_t *inst)
{
    discovered_device_t *best = NULL;
    uint8_t i;
    if (!inst || inst->manual_scan) return NULL;
    for (i = 0; i < inst->scan_result_count; ++i) {
        discovered_device_t *device = &inst->cold->scan_results[i];
        if (!should_auto_connect(inst, device)) continue;
        if (inst->cold->preferred_address[0] &&
            text_equal(inst->cold->preferred_address, device->address)) {
            return device;
        }
        if (!best || device->score > best->score ||
            (device->score == best->score && device->rssi > best->rssi)) {
            best = device;
        }
    }
    return best;
}

static void handle_scan_result(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    advertisement_t *adv;
    device_profile_t profile = DEVICE_PROFILE_HID;
    int score;
    int known = 0;
    int preferred;
    uint8_t i;
    if (!inst->scan_active || inst->state.connecting) return;
    adv = &inst->cold->advertisement_work;
    parse_advertisement(event->data, event->data_len, adv);
    score = score_advertisement(adv, &profile);
    for (i = 0; i < inst->scan_result_count; ++i) {
        if (!text_equal(inst->cold->scan_results[i].address, event->address)) continue;
        known = 1;
        break;
    }
    preferred = inst->cold->preferred_address[0] &&
                text_equal(inst->cold->preferred_address, event->address);
    /* Never add a nameless peer to the picker or auto-connect it. A later
     * scan response carrying the name can still admit the same address. */
    if (!adv->name[0] && !known && !preferred) return;
    /* Do not label arbitrary BLE/HID peers as gamepads. Known candidates may
     * still merge a later scan-response packet, and a saved preferred device
     * remains reconnectable even if this advertising packet is incomplete. */
    if (score < 40 && !known && !preferred) return;
    if (preferred && score < 40 && inst->cold->preferred_metadata_valid) {
        profile = inst->cold->preferred_profile;
        /* Migrate standard HOGP pads saved by 1.0.0, which classified every
         * 0x1812 advertiser as Q36. Explicit Q34/Q36 names keep that profile. */
        if (profile == DEVICE_PROFILE_Q36 &&
            !is_q36_compatible_name(inst->cold->preferred_name)) {
            profile = DEVICE_PROFILE_HID;
        }
    }
    (void)remember_device(inst, event, adv, profile, score);
}

static int advance_hid_service_discovery(hidpad_instance_t *inst)
{
    uint8_t next_attempt;
    if (!inst || (inst->profile != DEVICE_PROFILE_Q36 &&
                  inst->profile != DEVICE_PROFILE_HID)) return 0;
    if (inst->service_uuid_variant == 0) {
        inst->service_uuid_variant = 1;
        return 1;
    }
    if (inst->profile == DEVICE_PROFILE_HID) {
        if (inst->service_uuid_variant == 1) {
            /* Some otherwise standard controllers do not return their HID
             * service through this host's UUID-filtered discovery. Fall back
             * to discovering all primary services and match 0x1812 locally. */
            inst->service_uuid_variant = 2;
            return 1;
        }
        return 0;
    }
    /* Q34/Q36 retain their original second full initialization attempt. */
    if (inst->hid_init_attempt + 1u >= HIDPAD_Q36_INIT_ATTEMPTS) return 0;
    next_attempt = (uint8_t)(inst->hid_init_attempt + 1u);
    reset_gatt(inst);
    inst->hid_init_attempt = next_attempt;
    return 1;
}

static int request_hid_service_discovery(hidpad_instance_t *inst)
{
    const char *uuid;
    if (!inst) return 0;
    for (;;) {
        inst->hid_start = 0;
        inst->hid_end = 0;
        uuid = inst->service_uuid_variant == 0 ? UUID_HID_TEXT_16 :
               (inst->service_uuid_variant == 1 ? UUID_HID_TEXT_128 : NULL);
        if (inst->host->ble.gattc_discover_services(
                inst->session, inst->conn_handle, uuid) == MODULE_OK) return 1;
        if (!advance_hid_service_discovery(inst)) return 0;
    }
}

static void start_hid_service_discovery(hidpad_instance_t *inst)
{
    if (!inst || !inst->state.connected || inst->conn_handle == 0xffff) return;
    inst->last_error = NULL;
    inst->state.disconnect_reason = 0;
    inst->phase = PHASE_DISCOVER_SERVICES;
    mark_status_dirty(inst);
    if (!request_hid_service_discovery(inst)) {
        set_error(inst, "HID service discovery failed");
        inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
    }
}

static void begin_mtu_exchange(hidpad_instance_t *inst)
{
    if (!inst) return;
    /* Preserve the established Xbox/Q34/Q36 path. Generic HOGP may carry
     * reports larger than the default ATT payload, so it exchanges MTU after
     * security completes and before GATT discovery. */
    if (inst->profile == DEVICE_PROFILE_HID &&
        inst->host->ble.gattc_exchange_mtu &&
        inst->host->ble.gattc_exchange_mtu(inst->session, inst->conn_handle) == MODULE_OK) {
        inst->phase = PHASE_EXCHANGE_MTU;
        mark_status_dirty(inst);
        return;
    }
    start_hid_service_discovery(inst);
}

static void handle_connected(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    inst->conn_handle = event->conn_handle;
    inst->state.connected = 1;
    inst->state.connecting = 0;
    inst->notification_reconnect_pending = 0;
    reset_gatt(inst);
    inst->phase = PHASE_PAIRING;
    mark_status_dirty(inst);
    if (inst->host->ble.gap_pair(inst->session, inst->conn_handle, 1) != MODULE_OK) {
        set_error(inst, "BLE pairing request failed");
        inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
    }
}

static uint16_t normalized_disconnect_reason(uint16_t reason)
{
    return reason >= 0x0200u && reason <= 0x02ffu ? (uint16_t)(reason - 0x0200u) : reason;
}

static const char *pairing_disconnect_error(uint16_t reason)
{
    switch (normalized_disconnect_reason(reason)) {
    case 0x05: return "Pairing failed: authentication rejected";
    case 0x06: return "Pairing failed: stale bond or key missing";
    case 0x08: return "Pairing timeout: keep controller in pairing mode";
    case 0x13: return "Pairing canceled by controller";
    case 0x17: return "Pairing failed: too many repeated attempts";
    case 0x18: return "Pairing is not supported by controller";
    case 0x3d: return "Pairing failed: encryption key mismatch";
    default: return "Pairing disconnected; see disconnect_reason";
    }
}

static void handle_disconnected(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    int pairing = inst->phase == PHASE_PAIRING;
    int was_ready = inst->phase == PHASE_READY;
    int notification_recovery = inst->notification_reconnect_pending;
    int32_t forget_err = MODULE_OK;
    inst->state.disconnect_reason = event ? event->status : 0;
    if (pairing) inst->last_error = pairing_disconnect_error(inst->state.disconnect_reason);
    inst->conn_handle = 0xffff;
    inst->state.connected = 0;
    inst->state.connecting = 0;
    inst->state.encrypted = 0;
    if (!notification_recovery) inst->notification_reconnect_count = 0;
    if (notification_recovery && !was_ready) inst->notification_reconnect_pending = 0;
    clear_controls(inst);
    reset_gatt(inst);
    if (inst->forget_pending) {
        inst->forget_pending = 0;
        if (inst->host->ble.gap_forget_device) {
            forget_err = inst->host->ble.gap_forget_device(
                inst->session, inst->peer_addr_type, inst->state.address);
        } else {
            forget_err = MODULE_ERR_UNSUPPORTED;
        }
        if (forget_err == MODULE_ERR_NOT_FOUND && inst->host->ble.gap_clear_bonds) {
            forget_err = inst->host->ble.gap_clear_bonds(inst->session);
            if (forget_err == MODULE_ERR_NOT_FOUND) forget_err = MODULE_OK;
        }
        if (forget_err == MODULE_OK) {
            inst->last_error = NULL;
            inst->state.disconnect_reason = 0;
        } else {
            inst->last_error = "Failed to forget controller bond";
        }
    }
    if (inst->cold->command_kind == WORKER_COMMAND_FORGET && inst->cold->command_status == 1) {
        if (forget_err == MODULE_OK) {
            inst->cold->preferred_address[0] = 0;
            inst->cold->preferred_metadata_valid = 0;
        }
        finish_command(inst, forget_err == MODULE_OK ? NULL : "Failed to forget controller bond");
    } else if (inst->cold->command_kind == WORKER_COMMAND_DISCONNECT) {
        finish_command(inst, NULL);
    } else if (inst->cold->command_kind == WORKER_COMMAND_RESCAN && was_ready) {
        /* Explicit reconnect continues through scan, pairing and ready. */
    } else {
        finish_command(inst, inst->last_error ? inst->last_error : "controller disconnected");
    }
    if (inst->manual_scan) schedule_rescan(inst, 0);
    else if (was_ready) schedule_rescan(inst, 1200);
    else schedule_rescan_with_backoff(inst);
}

static void handle_event(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    report_characteristic_t *report;
    discovered_device_t *scan_device;
    if (event->irq != MODULE_BLE_IRQ_GATTC_NOTIFY) {
        inst->cold->ble_non_notify_event_count++;
        inst->cold->ble_last_non_notify_ms = now_ms(inst);
        inst->cold->ble_last_non_notify_irq = event->irq;
        inst->cold->ble_last_non_notify_status = event->status;
        if (event->status != 0) inst->cold->ble_done_error_count++;
        mark_status_dirty(inst);
    }
    switch (event->irq) {
    case MODULE_BLE_IRQ_SCAN_RESULT:
        handle_scan_result(inst, event);
        break;
    case MODULE_BLE_IRQ_SCAN_DONE:
        if (!inst->scan_active) break; /* Completion of a cancelled scan. */
        inst->scan_active = 0;
        finish_command_kind(inst, WORKER_COMMAND_SCAN, event->status ? "scan failed" : NULL);
        if (inst->state.connected || inst->state.connecting) {
            inst->manual_scan = 0;
            mark_status_dirty(inst);
            break;
        }
        if (inst->manual_scan) {
            inst->phase = PHASE_SELECT_DEVICE;
            mark_status_dirty(inst);
        } else {
            scan_device = event->status ? NULL : select_auto_device(inst);
            if (!scan_device || !connect_device(inst, scan_device)) {
                /* A manual Q34/Q36 connection gets one recovery scan, not an
                 * unlimited auto-connect loop when the global switch is off. */
                inst->notification_reconnect_pending = 0;
                schedule_rescan_with_backoff(inst);
            }
        }
        break;
    case MODULE_BLE_IRQ_PERIPHERAL_CONNECT:
        handle_connected(inst, event);
        break;
    case MODULE_BLE_IRQ_PERIPHERAL_DISCONNECT:
        handle_disconnected(inst, event);
        break;
    case MODULE_BLE_IRQ_GATTC_SERVICE_RESULT:
        if (inst->phase == PHASE_VENDOR_DISCOVER_SERVICES &&
            vendor_service_matches(inst, event->uuid)) {
            if (vendor_all_services_mode(inst)) {
                if (inst->vendor_service_count < HIDPAD_MAX_VENDOR_SERVICES) {
                    vendor_service_candidate_t *service =
                        &inst->cold->vendor_services[inst->vendor_service_count++];
                    service->start_handle = event->start_handle;
                    service->end_handle = event->end_handle;
                }
            } else {
                inst->vendor_start = event->start_handle;
                inst->vendor_end = event->end_handle;
            }
        } else if (uuid_is16(event->uuid, UUID_HID)) {
            inst->hid_start = event->start_handle;
            inst->hid_end = event->end_handle;
        }
        break;
    case MODULE_BLE_IRQ_GATTC_SERVICE_DONE:
        if (inst->phase == PHASE_VENDOR_DISCOVER_SERVICES) {
            if (vendor_all_services_mode(inst) && inst->vendor_service_count > 0) {
                if (!select_vendor_service_candidate(inst, 0)) {
                    complete_ready(inst);
                }
                break;
            }
            if (!inst->vendor_start || !inst->vendor_end) {
                if (!advance_vendor_service_discovery(inst)) complete_ready(inst);
            } else {
                if (!request_vendor_characteristic_discovery(inst)) {
                    finish_vendor_service(inst);
                }
            }
        } else if (!inst->hid_start || !inst->hid_end) {
            if (!advance_hid_service_discovery(inst) ||
                !request_hid_service_discovery(inst)) {
                set_error(inst, "HID 0x1812 service not found");
                inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
            }
        } else {
            inst->phase = PHASE_DISCOVER_CHARACTERISTICS;
            if (inst->host->ble.gattc_discover_characteristics(inst->session, inst->conn_handle,
                    inst->hid_start, inst->hid_end, NULL) != MODULE_OK) {
                fail_hid_initialization(inst, "HID characteristic discovery failed");
            }
        }
        break;
    case MODULE_BLE_IRQ_GATTC_CHARACTERISTIC_RESULT:
        if (inst->phase == PHASE_VENDOR_DISCOVER_CHARACTERISTICS) {
            if (inst->vendor_open_channel_index < inst->vendor_channel_count &&
                event->def_handle > 0) {
                inst->vendor_channels[inst->vendor_open_channel_index].descriptor_end_handle =
                    (uint16_t)(event->def_handle - 1u);
                inst->vendor_open_channel_index = 0xff;
            }
            if (is_btp_mapping_mode_name(inst->state.name) &&
                uuid_is16(event->uuid, UUID_BTP_VENDOR_INPUT)) {
                inst->btp_input_handle = event->value_handle;
            }
            if ((event->properties &
                 (MODULE_BLE_CHAR_PROP_WRITE | MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE)) != 0) {
                int btp_write = is_btp_mapping_mode_name(inst->state.name) &&
                                uuid_is16(event->uuid, UUID_BTP_VENDOR_WRITE);
                if (btp_write ||
                    (!is_btp_mapping_mode_name(inst->state.name) &&
                     (!inst->vendor_write_handle ||
                      text_equal(event->uuid, UUID_FLYDIGI_NUS_WRITE_TEXT)))) {
                    inst->vendor_write_handle = event->value_handle;
                    inst->vendor_write_properties = event->properties;
                }
            }
            if (vendor_notify_matches(inst, event->uuid) &&
                (is_btp_mapping_mode_name(inst->state.name) ||
                 (event->properties &
                  (MODULE_BLE_CHAR_PROP_NOTIFY | MODULE_BLE_CHAR_PROP_INDICATE)) != 0) &&
                inst->vendor_channel_count < HIDPAD_MAX_VENDOR_CHANNELS) {
                vendor_channel_t *channel =
                    &inst->vendor_channels[inst->vendor_channel_count++];
                channel->value_handle = event->value_handle;
                channel->descriptor_end_handle = inst->vendor_end;
                channel->properties = event->properties;
                channel->flydigi_nus_notify = text_equal(event->uuid, UUID_FLYDIGI_NUS_NOTIFY_TEXT);
                inst->vendor_open_channel_index =
                    (uint8_t)(inst->vendor_channel_count - 1u);
            }
            break;
        }
        /* ble_gattc_disc_all_dscs associates every result with the start
         * handle supplied by the caller. Close the preceding Report range
         * at the next characteristic definition so each Report can be
         * discovered separately below. */
        if (inst->open_report_index < inst->report_count && event->def_handle > 0) {
            inst->reports[inst->open_report_index].descriptor_end_handle =
                (uint16_t)(event->def_handle - 1u);
            inst->open_report_index = 0xff;
        }
        if (uuid_is16(event->uuid, UUID_REPORT_MAP)) {
            inst->report_map_handle = event->value_handle;
        } else if (uuid_is16(event->uuid, UUID_HID_CONTROL_POINT)) {
            inst->control_point_handle = event->value_handle;
            inst->control_point_properties = event->properties;
        } else if (uuid_is16(event->uuid, UUID_PROTOCOL_MODE)) {
            inst->protocol_mode_handle = event->value_handle;
            inst->protocol_mode_properties = event->properties;
        } else if (uuid_is16(event->uuid, UUID_REPORT) && inst->report_count < HIDPAD_MAX_REPORTS) {
            report = &inst->reports[inst->report_count++];
            report->value_handle = event->value_handle;
            report->descriptor_end_handle = inst->hid_end;
            report->properties = event->properties;
            inst->open_report_index = (uint8_t)(inst->report_count - 1u);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_CHARACTERISTIC_DONE:
        if (inst->phase == PHASE_VENDOR_DISCOVER_CHARACTERISTICS) {
            inst->vendor_open_channel_index = 0xff;
            inst->vendor_service_channel_end = inst->vendor_channel_count;
            inst->vendor_descriptor_index = inst->vendor_service_channel_start;
            if (inst->vendor_descriptor_index < inst->vendor_service_channel_end) {
                discover_next_vendor_descriptor(inst);
            } else {
                finish_vendor_service(inst);
            }
        } else if (inst->report_count == 0) {
            fail_hid_initialization(inst, "HID input report not found");
        } else {
            begin_protocol_mode(inst);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_DESCRIPTOR_RESULT:
        if (inst->phase == PHASE_VENDOR_DISCOVER_DESCRIPTORS) {
            if (inst->vendor_descriptor_index < inst->vendor_channel_count &&
                uuid_is16(event->uuid, UUID_CCCD)) {
                vendor_channel_t *channel =
                    &inst->vendor_channels[inst->vendor_descriptor_index];
                channel->cccd_handle = event->descriptor_handle;
            }
        } else {
            report = find_report(inst, event->value_handle);
            if (report && uuid_is16(event->uuid, UUID_CCCD)) report->cccd_handle = event->descriptor_handle;
            if (report && uuid_is16(event->uuid, UUID_REPORT_REFERENCE)) report->reference_handle = event->descriptor_handle;
        }
        break;
    case MODULE_BLE_IRQ_GATTC_DESCRIPTOR_DONE:
        if (inst->phase == PHASE_VENDOR_DISCOVER_DESCRIPTORS) {
            inst->vendor_descriptor_index++;
            discover_next_vendor_descriptor(inst);
        } else {
            inst->descriptor_index++;
            discover_next_report_descriptors(inst);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_READ_RESULT:
        if (inst->pending_read == PENDING_READ_MAP) {
            size_t available = HIDPAD_REPORT_MAP_MAX_SIZE - inst->report_map_len;
            size_t copy_len = event->data_len < available ? event->data_len : available;
            size_t map_index;
            for (map_index = 0; map_index < copy_len; ++map_index) {
                inst->cold->report_map_work[inst->report_map_len + map_index] =
                    event->data[map_index];
            }
            inst->report_map_len = (uint16_t)(inst->report_map_len + copy_len);
            if (copy_len < event->data_len || event->data_truncated) inst->report_map_overflow = 1;
        } else if (inst->pending_read == PENDING_READ_REFERENCE &&
                   inst->pending_report_index < inst->report_count && event->data_len >= 2) {
            report = &inst->reports[inst->pending_report_index];
            report->report_id = event->data[0];
            report->report_type = event->data[1];
        } else if (inst->pending_read == PENDING_READ_INPUT &&
                   inst->pending_report_index < inst->report_count) {
            report = &inst->reports[inst->pending_report_index];
            int had_baseline = report->last_report_valid;
            if (!report_is_duplicate(report, event->data, event->data_len)) {
                remember_input_packet(inst, report->value_handle,
                                      event->data, event->data_len);
                mark_dirty(inst);
                /* A first READ is only a baseline. Some controllers expose an
                 * all-zero cache that is not a real neutral gamepad report. */
                if (had_baseline) {
                    decode_hid(inst, report, event->data, event->data_len);
                } else {
                    remember_report(report, event->data, event->data_len, 0);
                }
            }
        }
        break;
    case MODULE_BLE_IRQ_GATTC_READ_DONE:
        if (inst->pending_read == PENDING_READ_MAP) {
            inst->pending_read = PENDING_READ_NONE;
            if (event->status != 0 || inst->report_map_len == 0) {
                fail_hid_initialization(inst, "HID report map read failed");
                break;
            }
            inst->report_map_valid =
                (!inst->report_map_overflow &&
                 hidpad_parser_parse(&inst->parser, inst->cold->report_map_work,
                                     inst->report_map_len)) ? 1 : 0;
            /* ShanWan Q34/Q36 Android mode exposes a keyboard-like map with no
             * gamepad fields understood by the generic parser. Its fixed
             * 10-byte input report remains decoded by the dedicated path. */
            if (!inst->report_map_valid && inst->report_map_len > 0 &&
                is_q36_compatible_name(inst->state.name)) {
                inst->report_map_valid = 1;
            }
            if (inst->profile == DEVICE_PROFILE_HID && !inst->report_map_valid &&
                is_flydigi_mapping_mode_name(inst->state.name)) {
                /* This mode is a valid HID Digitizer rather than a gamepad.
                 * Keep the link available for vendor-service input, but never
                 * interpret its touch coordinates as sticks. */
                read_next_reference(inst);
            } else if (inst->profile != DEVICE_PROFILE_XBOX && !inst->report_map_valid) {
                fail_hid_initialization(inst, "HID report map parse failed");
            } else {
                read_next_reference(inst);
            }
        } else if (inst->pending_read == PENDING_READ_REFERENCE) {
            inst->pending_read = PENDING_READ_NONE;
            inst->reference_index++;
            read_next_reference(inst);
        } else if (inst->pending_read == PENDING_READ_INPUT) {
            inst->pending_read = PENDING_READ_NONE;
        } else if (inst->pending_read == PENDING_READ_KEEPALIVE) {
            inst->pending_read = PENDING_READ_NONE;
        }
        break;
    case MODULE_BLE_IRQ_GATTC_WRITE_DONE:
        if (inst->phase == PHASE_VENDOR_SUBSCRIBE) {
            if (inst->vendor_subscribe_index < inst->vendor_channel_count) {
                vendor_channel_t *channel =
                    &inst->vendor_channels[inst->vendor_subscribe_index];
                channel->subscribed = event->status == 0 ? 1u : 0u;
                if (channel->subscribed) inst->vendor_subscribed_count++;
            }
            inst->vendor_subscribe_index++;
            subscribe_next_vendor_channel(inst);
        } else if (inst->phase == PHASE_SET_PROTOCOL_MODE) {
            inst->descriptor_index = 0;
            discover_next_report_descriptors(inst);
        } else if (inst->phase == PHASE_SUBSCRIBE) {
            if (inst->subscribe_index < inst->report_count && event->status == 0) {
                inst->reports[inst->subscribe_index].subscribed = 1;
                inst->subscribed_count++;
            }
            inst->subscribe_index++;
            subscribe_next(inst);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_NOTIFY:
        {
            vendor_channel_t *vendor_channel =
                find_vendor_channel(inst, event->value_handle);
        if (vendor_channel) {
            if (is_btp_mapping_mode_name(inst->state.name)) {
                inst->cold->btp_vendor_notify_count++;
                inst->btp_last_vendor_notify_ms = now_ms(inst);
                inst->cold->btp_last_vendor_handle = event->value_handle;
                inst->cold->btp_last_vendor_len = event->data_len < HIDPAD_REPORT_CACHE_SIZE ?
                    event->data_len : HIDPAD_REPORT_CACHE_SIZE;
                memcpy(inst->cold->btp_last_vendor, event->data, inst->cold->btp_last_vendor_len);
                if (event->data_len > 0) {
                    if (event->data_len >= 4u &&
                        event->data[0] == 0x11u &&
                        event->data[1] == 0x57u &&
                        event->data[2] == 0x44u &&
                        event->data[3] == 0x54u) {
                        inst->cold->btp_watchdog_reply_count++;
                    } else if (event->data[0] == 0x11u) {
                        inst->cold->btp_handshake_reply_count++;
                        inst->cold->btp_last_handshake_len = inst->cold->btp_last_vendor_len;
                        memcpy(inst->cold->btp_last_handshake, event->data,
                               inst->cold->btp_last_handshake_len);
                    } else if (event->data[0] == 0x21u) {
                        inst->cold->btp_heartbeat_reply_count++;
                        inst->btp_missed_heartbeats = 0;
                        if (!inst->btp_handshake_sent &&
                            !inst->btp_handshake_pending) {
                            inst->btp_handshake_pending = 1;
                            inst->next_btp_keepalive_ms = now_ms(inst);
                        }
                    } else if (event->data[0] == 0x15u) {
                        inst->btp_seen_battery_reply = 1;
                    } else if (event->data[0] == 0x55u) {
                        inst->btp_seen_info_reply = 1;
                    }
                }
                mark_status_dirty(inst);
                break;
            }
            inst->input_notify_count++;
            if (is_flydigi_mapping_mode_name(inst->state.name) &&
                vendor_channel->flydigi_nus_notify) {
                if (event->data_len >= 2 && event->data[0] == 0xac &&
                    event->data[1] == 0xc0 && inst->flydigi_init_stage < 2) {
                    /* Official client follows the AC C0 device-info response
                     * with the switch-chip and UUID queries. */
                    inst->flydigi_init_stage = 2;
                    inst->next_keepalive_ms = now_ms(inst) + 10u;
                } else if (event->data_len >= 2 && event->data[0] == 0xa5 &&
                           event->data[1] == 0xa0 && inst->flydigi_init_stage < 4) {
                    inst->flydigi_init_stage = 4;
                    inst->next_keepalive_ms = now_ms(inst) + 10u;
                }
            }
            remember_input_packet(inst, event->value_handle,
                                  event->data, event->data_len);
            decode_vendor_input(inst, event->data, event->data_len);
            break;
        }
        }
        report = find_report(inst, event->value_handle);
        if (report && (inst->profile != DEVICE_PROFILE_XBOX ||
                       report->value_handle == inst->controls_report_handle)) {
            inst->input_notify_count++;
            report->notify_count++;
            inst->notification_reconnect_count = 0;
            inst->next_notification_reconnect_ms = 0;
            if (!report_is_duplicate(report, event->data, event->data_len)) {
                remember_input_packet(inst, report->value_handle,
                                      event->data, event->data_len);
            }
            decode_hid(inst, report, event->data, event->data_len);
        }
        break;
    case MODULE_BLE_IRQ_ENCRYPTION_UPDATE:
        finish_command_kind(inst, WORKER_COMMAND_PAIR,
                            event->encrypted ? NULL : "BLE pairing/encryption failed");
        inst->state.encrypted = event->encrypted;
        mark_status_dirty(inst);
        if (inst->phase == PHASE_PAIRING) {
            if (event->encrypted) {
                begin_mtu_exchange(inst);
            } else {
                set_error(inst, "BLE pairing/encryption failed");
                inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
            }
        }
        break;
    case MODULE_BLE_IRQ_MTU_EXCHANGED:
        if (inst->phase == PHASE_EXCHANGE_MTU) start_hid_service_discovery(inst);
        break;
    default:
        break;
    }
}

static void poll_input_fallback(hidpad_instance_t *inst)
{
    uint8_t checked = 0;
    uint32_t now = now_ms(inst);
    int btp_mode;
    int q36_read_fallback;
    if (!inst) return;
    /* Flydigi smart mode uses its vendor stream as the real input source;
     * polling the placeholder HID reports would starve its acquire exchange.
     * KP20D/BFM's readable values are stale all-zero caches, not an input
     * fallback, so never poll them once their notification CCCDs are armed. */
    if (inst->vendor_subscribed &&
        is_flydigi_mapping_mode_name(inst->state.name)) return;
    btp_mode = is_btp_mapping_mode_name(inst->state.name);
    q36_read_fallback = inst->profile == DEVICE_PROFILE_Q36;
    if (inst->profile == DEVICE_PROFILE_XBOX ||
        inst->phase != PHASE_READY || inst->pending_read != PENDING_READ_NONE ||
        (int32_t)(now - inst->next_input_poll_ms) < 0) return;
    inst->next_input_poll_ms = now + 80;
    /* KP20D input is notification-only. In particular, Report ID 4 is a
     * readable but silent secondary input report. Polling that unsubscribed
     * report every 80 ms floods the controller with GATT transactions until
     * it deliberately drops and reconnects the link. */
    if (btp_mode) return;
    while (checked++ < inst->report_count) {
        uint8_t index = inst->input_poll_index++;
        report_characteristic_t *report;
        if (inst->input_poll_index >= inst->report_count) inst->input_poll_index = 0;
        if (index >= inst->report_count) index = 0;
        report = &inst->reports[index];
        if (report_is_read_candidate(report) &&
             (!report->subscribed ||
             (!btp_mode && inst->profile == DEVICE_PROFILE_HID &&
              report->notify_count == 0) ||
             (q36_read_fallback && report->notify_count == 0))) {
            start_read(inst, report->value_handle, PENDING_READ_INPUT, index);
            return;
        }
    }
}

static void poll_keepalive(hidpad_instance_t *inst)
{
    uint32_t now;
    uint8_t i;
    if (!inst || inst->profile == DEVICE_PROFILE_Q36 || inst->profile == DEVICE_PROFILE_XBOX ||
        inst->phase != PHASE_READY || !inst->state.connected || !inst->next_keepalive_ms) return;
    now = now_ms(inst);
    if ((int32_t)(now - inst->next_keepalive_ms) < 0) return;
    inst->next_keepalive_ms = now + HIDPAD_KEEPALIVE_MS;
    if (is_flydigi_mapping_mode_name(inst->state.name) &&
        inst->vendor_subscribed && inst->vendor_write_handle &&
        (inst->vendor_write_properties &
         (MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE | MODULE_BLE_CHAR_PROP_WRITE)) != 0) {
        /* Flydigi Game Center's normal-connect v0() command is cb/e.b =
         * BA C0 00 00. The nearby 14 01 array is cb/e.n and is not the
         * connection acquire command. Android sends v0() as a Write Command
         * even though this firmware advertises the NUS RX characteristic as
         * WRITE-only. The SDL 5A/A5 acquire packet is USB/dongle-specific. */
        uint8_t acquire[20] = {0xba, 0xc0, 0x00, 0x00};
        size_t acquire_len = 4;
        uint32_t mode = MODULE_BLE_WRITE_NO_RESPONSE;
        uint8_t stage = inst->flydigi_init_stage;
        if (stage == 2) {
            acquire[0] = 0xa5;
            acquire[1] = 0x01;
        } else if (stage == 3) {
            acquire[0] = 0xa5;
            acquire[1] = 0xa0;
        } else if (stage == 4) {
            /* cb/e.e(2): official "T mode, GATT only" command. Byte 19 is
             * the low-byte sum of bytes 0..18 (0x41 + 0x02 = 0x43). This
             * switches Android smart mode from mapped touch output to raw
             * operation notifications without using XInput. */
            zero_bytes(acquire, sizeof(acquire));
            acquire[0] = 0x41;
            acquire[1] = 0x02;
            acquire[19] = 0x43;
            acquire_len = sizeof(acquire);
        } else if (stage >= 5) {
            inst->next_keepalive_ms = 0;
            return;
        }
        if (inst->host->ble.gattc_write(
                inst->session, inst->conn_handle, inst->vendor_write_handle,
                acquire, acquire_len, mode) == MODULE_OK) {
            inst->keepalive_count++;
            if (stage == 0) {
                inst->flydigi_init_stage = 1;
                inst->next_keepalive_ms = now + 1000u;
            } else if (stage == 2) {
                inst->flydigi_init_stage = 3;
                inst->next_keepalive_ms = now + 50u;
            } else if (stage == 4) {
                inst->flydigi_init_stage = 5;
                inst->next_keepalive_ms = 0;
            } else {
                inst->next_keepalive_ms = now + HIDPAD_FLYDIGI_ACQUIRE_RETRY_MS;
            }
            return;
        }
        inst->next_keepalive_ms = now + HIDPAD_FLYDIGI_ACQUIRE_RETRY_MS;
        return;
    }
    if (inst->control_point_handle && inst->host->ble.gattc_write &&
        (inst->control_point_properties &
         (MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE | MODULE_BLE_CHAR_PROP_WRITE)) != 0) {
        /* HID Control Point is 0 = Suspend, 1 = Exit Suspend. Generic HOGP
         * needs this only once when the link becomes ready. BTP/BFM uses its
         * separate 7312 session in poll_btp_keepalive(). */
        uint8_t exit_suspend = 1;
        uint32_t mode = (inst->control_point_properties & MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE) != 0 ?
                        MODULE_BLE_WRITE_NO_RESPONSE : MODULE_BLE_WRITE_WITH_RESPONSE;
        if (inst->host->ble.gattc_write(inst->session, inst->conn_handle,
                                        inst->control_point_handle, &exit_suspend,
                                        sizeof(exit_suspend), mode) == MODULE_OK) {
            inst->keepalive_count++;
            inst->next_keepalive_ms = 0;
            return;
        }
    }
    if (inst->pending_read != PENDING_READ_NONE) {
        inst->next_keepalive_ms = now + HIDPAD_KEEPALIVE_RETRY_MS;
        return;
    }
    for (i = 0; i < inst->report_count; ++i) {
        report_characteristic_t *report = &inst->reports[i];
        if (report_is_read_candidate(report) &&
            start_read(inst, report->value_handle, PENDING_READ_KEEPALIVE, i)) {
            inst->keepalive_count++;
            return;
        }
    }
}

static void poll_btp_keepalive(hidpad_instance_t *inst)
{
    uint32_t now;
    uint32_t next_delay;
    uint32_t scheduled_next;
    int32_t err;
    uint8_t command[6] = {0x21u, 0x00u, 0x00u};
    size_t command_len = 3u;
    int sent_watchdog_now = 0;
    if (!inst || !is_btp_mapping_mode_name(inst->state.name) ||
        inst->phase != PHASE_READY || !inst->state.connected ||
        !inst->next_btp_keepalive_ms) return;
    now = now_ms(inst);
    if ((int32_t)(now - inst->next_btp_keepalive_ms) < 0) return;
    scheduled_next = inst->next_btp_keepalive_ms;
    next_delay = HIDPAD_BTP_HEARTBEAT_MS;
    if (!inst->btp_watchdog_sent) {
        /* JoyU 6.8.1 GattManager.c(true), called by every controller-test
         * screen onResume(), sends 10 "WDT" 01 01 over 7312. Its paired
         * onPause() command ends in 00. This is the official BFM session
         * watchdog enable command, not the legacy 21 probe heartbeat. */
        command[0] = 0x10u;
        command[1] = 0x57u;
        command[2] = 0x44u;
        command[3] = 0x54u;
        command[4] = 0x01u;
        command[5] = 0x01u;
        command_len = sizeof(command);
        next_delay = HIDPAD_BTP_INIT_GAP_MS;
        sent_watchdog_now = 1;
    } else if (inst->btp_handshake_pending) {
        /* JoyU sends 11 00 20 immediately from the first 21 response
         * handler. It completes the BFM session handshake; omitting it makes
         * KP20D stop GATT traffic when the short session deadline expires. */
        command[0] = 0x11u;
        command[1] = 0x00u;
        command[2] = 0x20u;
    } else if (inst->btp_init_stage == 0) {
        next_delay = HIDPAD_BTP_INIT_GAP_MS;
    } else if (inst->btp_init_stage == 1) {
        command[0] = 0x15u;
        command_len = 1;
        next_delay = HIDPAD_BTP_INIT_GAP_MS;
    } else if (inst->btp_init_stage == 2) {
        command[0] = 0x55u;
        command_len = 1;
        next_delay = HIDPAD_BTP_INIT_WAIT_MS;
    } else if (inst->btp_init_stage == 3 &&
               !btp_uses_periodic_heartbeat(inst->state.name)) {
        if (!inst->btp_seen_info_reply) {
            command[0] = 0x55u;
            command_len = 1;
        } else if (!inst->btp_seen_battery_reply) {
            command[0] = 0x15u;
            command_len = 1;
        } else {
            /* KP20D's official session is now complete. Additional 21
             * commands eventually make its firmware stop all notifications. */
            inst->next_btp_keepalive_ms = 0;
            mark_status_dirty(inst);
            return;
        }
    } else if (inst->btp_init_stage == 3 &&
               inst->btp_missed_heartbeats >= 3u) {
        command[0] = 0x11u;
        command[1] = 0x00u;
        command[2] = 0x20u;
        next_delay = HIDPAD_BTP_INIT_GAP_MS;
    } else if (inst->btp_init_stage == 5) {
        command[0] = 0x55u;
        command_len = 1;
        next_delay = HIDPAD_BTP_INIT_GAP_MS;
    } else if (inst->btp_init_stage == 6) {
        command[0] = 0x15u;
        command_len = 1;
    }
    /* Keep this timer independent of generic HOGP traffic. These are the
     * commands issued by JoyU 6.7.9's KP20 PC-handle session over 7312. The
     * initial order is 21 00 00, 15, 55; the first 21 opens the session. */
    if (!inst->btp_handshake_pending) {
        inst->next_btp_keepalive_ms = now + next_delay;
    }
    inst->cold->btp_keepalive_attempt_count++;
    mark_status_dirty(inst);
    if (!inst->vendor_subscribed || !inst->vendor_write_handle ||
        !inst->host->ble.gattc_write ||
        (inst->vendor_write_properties &
         (MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE | MODULE_BLE_CHAR_PROP_WRITE)) == 0) {
        inst->cold->btp_keepalive_error_count++;
        inst->cold->btp_keepalive_last_error = MODULE_ERR_UNSUPPORTED;
        return;
    }
    err = inst->host->ble.gattc_write(
        inst->session, inst->conn_handle, inst->vendor_write_handle,
        command, command_len, MODULE_BLE_WRITE_NO_RESPONSE);
    inst->cold->btp_keepalive_last_error = err;
    if (err == MODULE_OK) {
        inst->keepalive_count++;
        if (sent_watchdog_now) {
            inst->btp_watchdog_sent = 1;
            inst->cold->btp_watchdog_command_count++;
        } else if (inst->btp_handshake_pending) {
            inst->btp_handshake_pending = 0;
            inst->btp_handshake_sent = 1;
            /* Resume the initial 21 -> 15 -> 55 schedule interrupted by the
             * response-triggered handshake. */
            inst->next_btp_keepalive_ms = scheduled_next;
        } else if (inst->btp_init_stage == 0) {
            inst->btp_init_stage = 1;
        } else if (inst->btp_init_stage == 1) {
            inst->btp_init_stage = 2;
        } else if (inst->btp_init_stage == 2) {
            inst->btp_init_stage = 3;
        } else if (inst->btp_init_stage == 3 && command[0] == 0x11u) {
            /* The official client follows recovery with the normal heartbeat. */
            inst->btp_init_stage = 4;
        } else if (inst->btp_init_stage == 3 &&
                   !btp_uses_periodic_heartbeat(inst->state.name)) {
            /* Retry missing one-shot info/battery replies on the next tick,
             * then the branch above disables this timer. */
            inst->btp_init_stage = 3;
        } else if (inst->btp_init_stage == 3 || inst->btp_init_stage == 4) {
            inst->btp_missed_heartbeats++;
            if (!inst->btp_seen_battery_reply) {
                inst->btp_init_stage = 5;
                inst->next_btp_keepalive_ms = now + HIDPAD_BTP_INIT_GAP_MS;
            } else if (!inst->btp_seen_info_reply) {
                inst->btp_init_stage = 6;
                inst->next_btp_keepalive_ms = now + HIDPAD_BTP_INIT_GAP_MS;
            } else {
                inst->btp_init_stage = 3;
            }
        } else if (inst->btp_init_stage == 5) {
            if (!inst->btp_seen_info_reply) {
                inst->btp_init_stage = 6;
            } else {
                inst->btp_init_stage = 3;
                inst->next_btp_keepalive_ms = now + HIDPAD_BTP_HEARTBEAT_MS;
            }
        } else if (inst->btp_init_stage == 6) {
            inst->btp_init_stage = 3;
        }
    } else {
        inst->cold->btp_keepalive_error_count++;
        inst->next_btp_keepalive_ms = now + HIDPAD_BTP_INIT_GAP_MS;
    }
}

static void poll_notification_reconnect(hidpad_instance_t *inst)
{
    uint32_t now;
    if (!HIDPAD_ENABLE_NOTIFY_RECONNECT) return;
    if (!inst || inst->manual_scan || inst->profile != DEVICE_PROFILE_Q36 ||
        inst->phase != PHASE_READY || !inst->state.connected ||
        inst->input_notify_count != 0 || inst->subscribed_count == 0 ||
        inst->notification_reconnect_count != 0 ||
        !inst->next_notification_reconnect_ms) return;
    now = now_ms(inst);
    if ((int32_t)(now - inst->next_notification_reconnect_ms) < 0) return;
    inst->next_notification_reconnect_ms = 0;
    /* Q34/Q36-specific recovery: perform one full encrypted reconnect if the
     * first link arms its CCCDs but produces no input. */
    if (inst->host->ble.gap_disconnect(inst->session, inst->conn_handle) == MODULE_OK) {
        inst->notification_reconnect_count = 1;
        inst->notification_reconnect_pending = 1;
    }
}

static void driver_poll(hidpad_instance_t *inst)
{
    uint32_t i;
    module_ble_event_t *event;
    if (!inst || !inst->started) return;
    /* Apply policy on the BLE worker, under its mutex. A current connection
     * (including a manual connection in progress) is never interrupted. */
    if (inst->auto_connect_dirty) {
        inst->auto_connect_dirty = 0;
        if (!inst->state.connected && !inst->state.connecting) {
            if (!inst->auto_connect) {
                if (!inst->manual_scan) {
                    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
                    inst->scan_active = 0;
                    inst->phase = PHASE_SELECT_DEVICE;
                }
            } else {
                inst->manual_scan = 0;
                if (!inst->scan_active) schedule_rescan(inst, 0);
            }
        }
        mark_status_dirty(inst);
    }
    inst->cold->driver_poll_count++;
    event = &inst->event_work;
    event->size = sizeof(*event);
    for (i = 0; i < HIDPAD_EVENT_BUDGET; ++i) {
        int32_t err;
        err = inst->host->ble.event_poll(inst->session, event);
        if (err == MODULE_ERR_NOT_FOUND) break;
        if (err != MODULE_OK) {
            set_error(inst, "BLE event poll failed");
            break;
        }
        handle_event(inst, event);
    }
    if (inst->phase == PHASE_WAIT_RESCAN && !inst->state.connected && !inst->state.connecting &&
        (int32_t)(now_ms(inst) - inst->next_scan_ms) >= 0) {
        start_scan(inst);
    }
    if (inst->cold->command_status == 1 &&
        (int32_t)(now_ms(inst) - inst->cold->command_deadline_ms) >= 0) {
        worker_command_t timed_out = inst->cold->command_kind;
        finish_command(inst, "controller operation timed out");
        if (timed_out == WORKER_COMMAND_CONNECT || timed_out == WORKER_COMMAND_PAIR) {
            if (inst->state.connected) (void)inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
        }
    }
    poll_notification_reconnect(inst);
    poll_btp_keepalive(inst);
    poll_keepalive(inst);
    poll_input_fallback(inst);
}

static int driver_start(hidpad_instance_t *inst);
static void driver_stop(hidpad_instance_t *inst);

static int runtime_event_mode_supported(const hidpad_instance_t *inst)
{
    return inst && inst->host->runtime.event_post && inst->host->runtime.event_cancel &&
           inst->host->task.create_ex && inst->host->sync.create_counting &&
           inst->host->sync.create_mutex && inst->host->sync.take &&
           inst->host->sync.give && inst->host->sync.destroy;
}

static int instance_lock(hidpad_instance_t *inst, uint32_t timeout_ms)
{
    if (!inst || !inst->worker_mutex) return 1;
    return inst->host->sync.take(inst->worker_mutex, timeout_ms) == MODULE_OK;
}

static void instance_unlock(hidpad_instance_t *inst)
{
    if (inst && inst->worker_mutex) (void)inst->host->sync.give(inst->worker_mutex);
}

static void post_lua_event(hidpad_instance_t *inst)
{
    if (!inst || !inst->lua || inst->event_ref <= -1 || !inst->host->runtime.event_post) return;
    (void)inst->host->runtime.event_post(inst->lua, inst->event_ref);
}

static int link_initializing(const hidpad_instance_t *inst)
{
    return inst->state.connecting || (inst->state.connected && inst->phase != PHASE_READY);
}

static void worker_command_failed(hidpad_instance_t *inst, const char *error)
{
    finish_command(inst, error);
}

static int forget_peer(hidpad_instance_t *inst)
{
    int32_t err;
    if (!inst->host->ble.gap_forget_device) return MODULE_ERR_UNSUPPORTED;
    if (!inst->state.address[0]) {
        return inst->host->ble.gap_clear_bonds ?
            inst->host->ble.gap_clear_bonds(inst->session) : MODULE_ERR_UNSUPPORTED;
    }
    err = inst->host->ble.gap_forget_device(inst->session, inst->peer_addr_type, inst->state.address);
    return err == MODULE_ERR_NOT_FOUND ? MODULE_OK : err;
}

static void execute_worker_command(hidpad_instance_t *inst, worker_command_t command)
{
    int32_t err;
    uint8_t was_manual;
    if (!inst || command == WORKER_COMMAND_NONE) return;
    if (command == WORKER_COMMAND_START) {
        err = driver_start(inst);
        finish_command(inst, err == MODULE_OK ? NULL : "BLE transport busy");
        return;
    }
    if (!inst->started) { worker_command_failed(inst, "hidpad is not started"); return; }
    switch (command) {
    case WORKER_COMMAND_RESCAN:
        if (link_initializing(inst) || inst->scan_active || !inst->auto_connect) {
            worker_command_failed(inst, "rescan rejected: busy or auto-connect disabled"); break;
        }
        inst->manual_scan = 0;
        inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
        if (inst->state.connected) {
            if (inst->host->ble.gap_disconnect(inst->session, inst->conn_handle) != MODULE_OK)
                worker_command_failed(inst, "disconnect failed");
        } else schedule_rescan(inst, 0);
        break;
    case WORKER_COMMAND_SCAN:
        if (link_initializing(inst) || inst->scan_active) {
            worker_command_failed(inst, "scan rejected while driver is busy"); break;
        }
        inst->manual_scan = 1;
        inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
        inst->scan_result_count = 0;
        zero_bytes(inst->cold->scan_results, sizeof(inst->cold->scan_results));
        if (!start_scan(inst)) {
            if (inst->manual_scan) inst->phase = PHASE_SELECT_DEVICE;
            worker_command_failed(inst, "scan start failed");
        }
        break;
    case WORKER_COMMAND_CONNECT:
        if (inst->state.connected || inst->state.connecting || !inst->cold->command_device_valid) {
            worker_command_failed(inst, "connect rejected while driver is busy"); break;
        }
        if (!connect_device(inst, &inst->cold->command_device)) {
            worker_command_failed(inst, "connect failed");
            schedule_rescan_with_backoff(inst);
        }
        break;
    case WORKER_COMMAND_DISCONNECT:
    case WORKER_COMMAND_FORGET:
        if (inst->state.connecting) {
            worker_command_failed(inst, "connection is still in progress"); break;
        }
        was_manual = inst->manual_scan;
        inst->manual_scan = 1; /* Explicit user action suspends auto-reconnect. */
        inst->notification_reconnect_pending = 0;
        if (inst->scan_active) (void)inst->host->ble.gap_scan_stop(inst->session);
        inst->scan_active = 0;
        if (inst->state.connected) {
            inst->forget_pending = command == WORKER_COMMAND_FORGET;
            err = inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
            if (err != MODULE_OK) {
                inst->forget_pending = 0;
                inst->manual_scan = was_manual;
                worker_command_failed(inst, "disconnect failed");
            }
        } else if (command == WORKER_COMMAND_FORGET) {
            err = forget_peer(inst);
            if (err == MODULE_OK || err == MODULE_ERR_NOT_FOUND) {
                inst->cold->preferred_address[0] = 0;
                inst->cold->preferred_metadata_valid = 0;
                inst->phase = PHASE_SELECT_DEVICE;
                finish_command(inst, NULL);
            } else {
                inst->manual_scan = was_manual;
                worker_command_failed(inst, "forget controller failed");
            }
        } else {
            inst->phase = PHASE_SELECT_DEVICE;
            finish_command(inst, NULL); /* Disconnect is idempotent. */
        }
        break;
    case WORKER_COMMAND_PAIR:
        if (!inst->state.connected || inst->phase != PHASE_READY ||
            inst->host->ble.gap_pair(inst->session, inst->conn_handle, 1) != MODULE_OK)
            worker_command_failed(inst, "pair failed");
        break;
    default: worker_command_failed(inst, "unsupported command"); break;
    }
}

/* Called with the instance mutex held (or without a worker in legacy mode). */
static int32_t prepare_command(hidpad_instance_t *inst, worker_command_t command,
                               const char *address)
{
    uint8_t i;
    if (inst->cold->command_status == 1) return MODULE_ERR_BUSY;
    inst->cold->command_device_valid = 0;
    if (command == WORKER_COMMAND_CONNECT) {
        for (i = 0; i < inst->scan_result_count; ++i) {
            if (text_equal(inst->cold->scan_results[i].address, address)) {
                inst->cold->command_device = inst->cold->scan_results[i];
                inst->cold->command_device_valid = 1;
                break;
            }
        }
        if (!inst->cold->command_device_valid) return MODULE_ERR_NOT_FOUND;
    }
    if (++inst->cold->command_id == 0) ++inst->cold->command_id;
    inst->cold->command_kind = command;
    inst->cold->command_status = 1;
    inst->cold->command_error = NULL;
    inst->cold->command_deadline_ms = now_ms(inst) +
        (command == WORKER_COMMAND_SCAN ? inst->scan_ms + 5000u : 45000u);
    inst->last_error = NULL;
    mark_status_dirty(inst);
    return MODULE_OK;
}

static uint32_t worker_wait_ms(const hidpad_instance_t *inst)
{
    int32_t remaining;
    if (inst->scan_active) return 20;
    if (inst->state.connected) return inst->phase == PHASE_READY ? 10 : 50;
    if (inst->state.connecting || inst->cold->command_status == 1) return 50;
    if (inst->phase == PHASE_WAIT_RESCAN) {
        remaining = (int32_t)(inst->next_scan_ms - now_ms(inst));
        return remaining <= 0 ? 1u : (remaining > 1000 ? 1000u : (uint32_t)remaining);
    }
    return 1000; /* Commands/stop wake the semaphore immediately. */
}

static void worker_main(void *arg)
{
    hidpad_instance_t *inst = (hidpad_instance_t *)arg;
    uint32_t wait_ms = 10;
    int should_post = 0;
    if (!inst) return;

    if (instance_lock(inst, 1000)) {
        execute_worker_command(inst, WORKER_COMMAND_START);
        should_post = inst->state_dirty != 0;
        instance_unlock(inst);
    }
    if (should_post) post_lua_event(inst);

    while (!inst->worker_stop) {
        if (instance_lock(inst, 1000)) {
            worker_command_t command = inst->worker_command;
            inst->worker_command = WORKER_COMMAND_NONE;
            execute_worker_command(inst, command);
            driver_poll(inst);
            should_post = inst->state_dirty != 0;
            wait_ms = worker_wait_ms(inst);
            instance_unlock(inst);
        }
        if (should_post) post_lua_event(inst);
        should_post = 0;
        if (inst->worker_stop) break;
        (void)inst->host->sync.take(inst->worker_wake, wait_ms);
    }

    if (instance_lock(inst, 1000)) {
        driver_stop(inst);
        should_post = inst->state_dirty != 0;
        instance_unlock(inst);
    }
    if (should_post) post_lua_event(inst);
    inst->worker_task = NULL;
    (void)inst->host->sync.give(inst->worker_stopped);
}

static void destroy_worker_sync(hidpad_instance_t *inst)
{
    if (!inst || !inst->host->sync.destroy) return;
    if (inst->worker_stopped) inst->host->sync.destroy(inst->worker_stopped);
    if (inst->worker_wake) inst->host->sync.destroy(inst->worker_wake);
    if (inst->worker_mutex) inst->host->sync.destroy(inst->worker_mutex);
    inst->worker_stopped = NULL;
    inst->worker_wake = NULL;
    inst->worker_mutex = NULL;
}

static int32_t start_worker(hidpad_instance_t *inst)
{
    int32_t err;
    if (!inst || !runtime_event_mode_supported(inst)) return MODULE_ERR_UNSUPPORTED;
    if (inst->worker_running) {
        if (!instance_lock(inst, 1000)) return MODULE_ERR_BUSY;
        err = inst->started ? MODULE_OK : prepare_command(inst, WORKER_COMMAND_START, NULL);
        if (err == MODULE_OK && !inst->started) inst->worker_command = WORKER_COMMAND_START;
        instance_unlock(inst);
        (void)inst->host->sync.give(inst->worker_wake);
        return err;
    }
    destroy_worker_sync(inst);
    err = inst->host->sync.create_mutex(&inst->worker_mutex);
    if (err != MODULE_OK) goto failed;
    err = inst->host->sync.create_counting(1, 0, &inst->worker_wake);
    if (err != MODULE_OK) goto failed;
    err = inst->host->sync.create_counting(1, 0, &inst->worker_stopped);
    if (err != MODULE_OK) goto failed;
    err = prepare_command(inst, WORKER_COMMAND_START, NULL);
    if (err != MODULE_OK) goto failed;
    inst->worker_stop = 0;
    inst->worker_running = 1;
    err = inst->host->task.create_ex(
        "hidpad_worker", worker_main, inst, HIDPAD_WORKER_STACK_BYTES,
        HIDPAD_WORKER_PRIORITY, HIDPAD_WORKER_CORE,
        MODULE_HEAP_PSRAM | MODULE_HEAP_8BIT, &inst->worker_task);
    if (err == MODULE_OK) return MODULE_OK;
    inst->worker_running = 0;
failed:
    finish_command(inst, "hidpad worker start failed");
    destroy_worker_sync(inst);
    return err;
}

static void stop_worker(hidpad_instance_t *inst)
{
    if (!inst || !inst->worker_running) return;
    inst->worker_stop = 1;
    (void)inst->host->sync.give(inst->worker_wake);
    (void)inst->host->sync.take(inst->worker_stopped, MODULE_WAIT_FOREVER);
    inst->worker_running = 0;
    destroy_worker_sync(inst);
}

static int driver_start(hidpad_instance_t *inst)
{
    module_ble_config_t *config;
    int32_t err;
    if (!inst) return MODULE_ERR_INVALID_ARG;
    if (inst->started) return MODULE_OK;
    config = &inst->cold->config_work;
    zero_bytes(config, sizeof(*config));
    config->size = sizeof(*config);
    /* Generic HOGP explicitly exchanges this MTU after pairing. 245 yields a
     * 244-byte ATT value, matching module_ble_event_t::data and allowing maps
     * larger than the previous 184-byte ceiling. Xbox/Q34/Q36 keep their
     * established no-exchange path. */
    config->mtu = 245;
    /* Keep the legacy host buffer request: deployed firmware versions may
     * still use this field even though newer hosts no longer depend on it. */
    config->rxbuf = 2048;
    config->bond = 1;
    config->mitm = 0;
    config->le_secure = 1;
    config->io_capability = MODULE_BLE_IO_NO_INPUT_OUTPUT;
    config->own_addr_type = MODULE_BLE_OWN_ADDR_PUBLIC;
    copy_text(config->gap_name, sizeof(config->gap_name), "Cubic-HIDPad", 12);
    err = inst->host->ble.open(inst->owner_token, config, &inst->session);
    if (err != MODULE_OK) {
        inst->last_error = "BLE transport busy";
        return err;
    }
    inst->started = 1;
    inst->manual_scan = 0;
    inst->auto_connect_dirty = 0;
    inst->conn_handle = 0xffff;
    inst->state_dirty = 1;
    inst->status_dirty = 1;
    inst->last_error = NULL;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->keepalive_count = 0;
    inst->cold->connection_params_attempt_count = 0;
    inst->cold->connection_params_last_error = MODULE_ERR_NOT_FOUND;
    inst->cold->driver_poll_count = 0;
    inst->cold->btp_keepalive_attempt_count = 0;
    inst->cold->btp_keepalive_error_count = 0;
    inst->cold->btp_keepalive_last_error = MODULE_OK;
    inst->cold->btp_vendor_notify_count = 0;
    inst->btp_last_vendor_notify_ms = 0;
    inst->cold->btp_heartbeat_reply_count = 0;
    inst->cold->btp_handshake_reply_count = 0;
    inst->cold->btp_watchdog_command_count = 0;
    inst->cold->btp_watchdog_reply_count = 0;
    inst->cold->btp_input_read_count = 0;
    inst->cold->ble_non_notify_event_count = 0;
    inst->cold->ble_last_non_notify_ms = 0;
    inst->cold->ble_done_error_count = 0;
    inst->cold->ble_last_non_notify_irq = 0;
    inst->cold->ble_last_non_notify_status = 0;
    inst->notification_reconnect_count = 0;
    inst->notification_reconnect_pending = 0;
    clear_controls(inst);
    reset_gatt(inst);
    /* Always rediscover before connecting. This keeps GATT initialization in
     * the same, reliable order for controllers such as Q34/Q36. */
    start_scan(inst);
    return MODULE_OK;
}

static void driver_stop(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->worker_command = WORKER_COMMAND_NONE;
    finish_command(inst, "service stopped");
    if (!inst->started) return;
    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
    if (inst->state.connected && inst->conn_handle != 0xffff) {
        inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
    }
    inst->host->ble.close(inst->session);
    inst->session = 0;
    inst->started = 0;
    inst->scan_active = 0;
    inst->state.connected = 0;
    inst->state.connecting = 0;
    inst->phase = PHASE_STOPPED;
    clear_controls(inst);
    mark_status_dirty(inst);
}

static void set_integer_at(lua_State *L, const hidpad_host_api_t *host, int table_index,
                           const char *name, int64_t value)
{
    host->lua.pushinteger(L, value);
    host->lua.setfield(L, table_index, name);
}

static void set_boolean_at(lua_State *L, const hidpad_host_api_t *host, int table_index,
                           const char *name, int value)
{
    host->lua.pushboolean(L, value);
    host->lua.setfield(L, table_index, name);
}

static void set_string_at(lua_State *L, const hidpad_host_api_t *host, int table_index,
                          const char *name, const char *value)
{
    host->lua.pushstring(L, value ? value : "");
    host->lua.setfield(L, table_index, name);
}

static void set_integer_field(lua_State *L, const hidpad_host_api_t *host,
                              const char *name, int64_t value)
{
    set_integer_at(L, host, -2, name, value);
}

static void set_string_field(lua_State *L, const hidpad_host_api_t *host,
                             const char *name, const char *value)
{
    set_string_at(L, host, -2, name, value);
}

static void fill_input_state(lua_State *L, hidpad_instance_t *inst, int table_index)
{
    const hidpad_host_api_t *host = inst->host;
    set_integer_at(L, host, table_index, "seq", inst->state.seq);
    set_integer_at(L, host, table_index, "timestamp_ms", inst->state.timestamp_ms);
    set_integer_at(L, host, table_index, "buttons", inst->state.buttons);
    set_integer_at(L, host, table_index, "raw_buttons", inst->state.raw_buttons);
    set_integer_at(L, host, table_index, "lx", inst->state.lx);
    set_integer_at(L, host, table_index, "ly", inst->state.ly);
    set_integer_at(L, host, table_index, "rx", inst->state.rx);
    set_integer_at(L, host, table_index, "ry", inst->state.ry);
    set_integer_at(L, host, table_index, "lt", inst->state.lt);
    set_integer_at(L, host, table_index, "rt", inst->state.rt);
    set_integer_at(L, host, table_index, "report_id", inst->state.report_id);
    set_integer_at(L, host, table_index, "notify_count", inst->input_notify_count);
}

static void fill_state(lua_State *L, hidpad_instance_t *inst, int table_index)
{
    const hidpad_host_api_t *host = inst->host;
    fill_input_state(L, inst, table_index);
    set_boolean_at(L, host, table_index, "started", inst->started);
    set_boolean_at(L, host, table_index, "ready", inst->state.connected && inst->phase == PHASE_READY);
    set_boolean_at(L, host, table_index, "scanning", inst->scan_active);
    set_boolean_at(L, host, table_index, "auto_connect", inst->auto_connect);
    set_integer_at(L, host, table_index, "command_id", inst->cold->command_id);
    set_string_at(L, host, table_index, "command_kind", command_text(inst->cold->command_kind));
    set_string_at(L, host, table_index, "command_status", inst->cold->command_status == 1 ? "pending" :
                  inst->cold->command_status == 2 ? "succeeded" : inst->cold->command_status == 3 ? "failed" : "none");
    set_string_at(L, host, table_index, "command_error", inst->cold->command_error);
    set_boolean_at(L, host, table_index, "connected", inst->state.connected);
    set_boolean_at(L, host, table_index, "connecting", inst->state.connecting);
    set_boolean_at(L, host, table_index, "encrypted", inst->state.encrypted);
    set_integer_at(L, host, table_index, "disconnect_reason", inst->state.disconnect_reason);
    set_boolean_at(L, host, table_index, "manual_scan", inst->manual_scan);
    set_integer_at(L, host, table_index, "scan_count", inst->scan_result_count);
    set_integer_at(L, host, table_index, "keepalive_count", inst->keepalive_count);
    set_boolean_at(L, host, table_index, "keepalive_supported",
                   is_btp_mapping_mode_name(inst->state.name) ?
                   (inst->vendor_subscribed && inst->vendor_write_handle != 0) :
                   inst->control_point_handle != 0);
    set_string_at(L, host, table_index, "phase", phase_text(inst->phase));
    set_string_at(L, host, table_index, "profile", profile_text(inst->profile));
    set_string_at(L, host, table_index, "address", inst->state.address);
    set_string_at(L, host, table_index, "name", inst->state.name);
    set_integer_at(L, host, table_index, "addr_type", inst->peer_addr_type);
    set_string_at(L, host, table_index, "last_error", inst->last_error);
}

static void fill_diagnostics(lua_State *L, hidpad_instance_t *inst, int table_index)
{
    const hidpad_host_api_t *host = inst->host;
    char report_hex[HIDPAD_REPORT_CACHE_SIZE * 2u + 1u];
    bytes_to_hex(report_hex, sizeof(report_hex), inst->cold->last_report,
                 inst->cold->last_report_len < HIDPAD_REPORT_CACHE_SIZE ? inst->cold->last_report_len : HIDPAD_REPORT_CACHE_SIZE);
    set_integer_at(L, host, table_index, "last_report_handle", inst->cold->last_report_handle);
    set_integer_at(L, host, table_index, "last_report_len", inst->cold->last_report_len);
    set_string_at(L, host, table_index, "last_report_hex", report_hex);
    if (inst->report_count > 0) {
        report_characteristic_t *report = &inst->reports[0];
        bytes_to_hex(report_hex, sizeof(report_hex), report->last_report,
                     report->last_report_valid ? report->last_report_len : 0);
        set_integer_at(L, host, table_index, "report0_handle", report->value_handle);
        set_integer_at(L, host, table_index, "report0_id", report->report_id);
        set_integer_at(L, host, table_index, "report0_type", report->report_type);
        set_integer_at(L, host, table_index, "report0_properties", report->properties);
        set_boolean_at(L, host, table_index, "report0_subscribed", report->subscribed);
        set_integer_at(L, host, table_index, "report0_len",
                       report->last_report_valid ? report->last_report_len : 0);
        set_integer_at(L, host, table_index, "report0_notify_count", report->notify_count);
        set_string_at(L, host, table_index, "report0_hex", report_hex);
    } else {
        set_integer_at(L, host, table_index, "report0_handle", 0);
        set_integer_at(L, host, table_index, "report0_id", 0);
        set_integer_at(L, host, table_index, "report0_type", 0);
        set_integer_at(L, host, table_index, "report0_properties", 0);
        set_boolean_at(L, host, table_index, "report0_subscribed", 0);
        set_integer_at(L, host, table_index, "report0_len", 0);
        set_integer_at(L, host, table_index, "report0_notify_count", 0);
        set_string_at(L, host, table_index, "report0_hex", "");
    }
    if (inst->report_count > 1) {
        report_characteristic_t *report = &inst->reports[1];
        bytes_to_hex(report_hex, sizeof(report_hex), report->last_report,
                     report->last_report_valid ? report->last_report_len : 0);
        set_integer_at(L, host, table_index, "report1_handle", report->value_handle);
        set_integer_at(L, host, table_index, "report1_id", report->report_id);
        set_integer_at(L, host, table_index, "report1_type", report->report_type);
        set_integer_at(L, host, table_index, "report1_properties", report->properties);
        set_boolean_at(L, host, table_index, "report1_subscribed", report->subscribed);
        set_integer_at(L, host, table_index, "report1_len",
                       report->last_report_valid ? report->last_report_len : 0);
        set_integer_at(L, host, table_index, "report1_notify_count", report->notify_count);
        set_string_at(L, host, table_index, "report1_hex", report_hex);
    } else {
        set_integer_at(L, host, table_index, "report1_handle", 0);
        set_integer_at(L, host, table_index, "report1_id", 0);
        set_integer_at(L, host, table_index, "report1_type", 0);
        set_integer_at(L, host, table_index, "report1_properties", 0);
        set_boolean_at(L, host, table_index, "report1_subscribed", 0);
        set_integer_at(L, host, table_index, "report1_len", 0);
        set_integer_at(L, host, table_index, "report1_notify_count", 0);
        set_string_at(L, host, table_index, "report1_hex", "");
    }
    set_integer_at(L, host, table_index, "connection_params_attempt_count",
                   inst->cold->connection_params_attempt_count);
    set_integer_at(L, host, table_index, "connection_params_last_error",
                   inst->cold->connection_params_last_error);
    set_integer_at(L, host, table_index, "connection_params_min",
                   is_btp_mapping_mode_name(inst->state.name) ?
                   0 : HIDPAD_CONN_INTERVAL_MIN);
    set_integer_at(L, host, table_index, "connection_params_max",
                   is_btp_mapping_mode_name(inst->state.name) ?
                   0 : HIDPAD_CONN_INTERVAL_MAX);
    set_integer_at(L, host, table_index, "btp_keepalive_attempt_count",
                   inst->cold->btp_keepalive_attempt_count);
    set_integer_at(L, host, table_index, "btp_keepalive_error_count",
                   inst->cold->btp_keepalive_error_count);
    set_integer_at(L, host, table_index, "btp_keepalive_last_error",
                   inst->cold->btp_keepalive_last_error);
    set_integer_at(L, host, table_index, "driver_poll_count", inst->cold->driver_poll_count);
    set_integer_at(L, host, table_index, "clock_ms", now_ms(inst));
    set_integer_at(L, host, table_index, "next_btp_keepalive_ms",
                   inst->next_btp_keepalive_ms);
    set_integer_at(L, host, table_index, "btp_vendor_notify_count",
                   inst->cold->btp_vendor_notify_count);
    set_integer_at(L, host, table_index, "btp_last_vendor_notify_ms",
                   inst->btp_last_vendor_notify_ms);
    set_integer_at(L, host, table_index, "btp_heartbeat_reply_count",
                   inst->cold->btp_heartbeat_reply_count);
    set_integer_at(L, host, table_index, "btp_handshake_reply_count",
                   inst->cold->btp_handshake_reply_count);
    set_integer_at(L, host, table_index, "btp_watchdog_command_count",
                   inst->cold->btp_watchdog_command_count);
    set_integer_at(L, host, table_index, "btp_watchdog_reply_count",
                   inst->cold->btp_watchdog_reply_count);
    bytes_to_hex(report_hex, sizeof(report_hex), inst->cold->btp_last_handshake, inst->cold->btp_last_handshake_len);
    set_string_at(L, host, table_index, "btp_last_handshake_hex", report_hex);
    set_integer_at(L, host, table_index, "btp_input_read_count",
                   inst->cold->btp_input_read_count);
    set_integer_at(L, host, table_index, "ble_non_notify_event_count",
                   inst->cold->ble_non_notify_event_count);
    set_integer_at(L, host, table_index, "ble_last_non_notify_ms",
                   inst->cold->ble_last_non_notify_ms);
    set_integer_at(L, host, table_index, "ble_done_error_count",
                   inst->cold->ble_done_error_count);
    set_integer_at(L, host, table_index, "ble_last_non_notify_irq",
                   inst->cold->ble_last_non_notify_irq);
    set_integer_at(L, host, table_index, "ble_last_non_notify_status",
                   inst->cold->ble_last_non_notify_status);
    set_integer_at(L, host, table_index, "btp_input_handle",
                   inst->btp_input_handle);
    set_integer_at(L, host, table_index, "btp_last_vendor_handle",
                   inst->cold->btp_last_vendor_handle);
    bytes_to_hex(report_hex, sizeof(report_hex), inst->cold->btp_last_vendor, inst->cold->btp_last_vendor_len);
    set_string_at(L, host, table_index, "btp_last_vendor_hex", report_hex);
    set_integer_at(L, host, table_index, "btp_missed_heartbeats",
                   inst->btp_missed_heartbeats);
    set_boolean_at(L, host, table_index, "btp_handshake_pending",
                   inst->btp_handshake_pending);
    set_boolean_at(L, host, table_index, "btp_handshake_sent",
                   inst->btp_handshake_sent);
    set_boolean_at(L, host, table_index, "btp_watchdog_sent",
                   inst->btp_watchdog_sent);
    set_boolean_at(L, host, table_index, "btp_seen_battery_reply",
                   inst->btp_seen_battery_reply);
    set_boolean_at(L, host, table_index, "btp_seen_info_reply",
                   inst->btp_seen_info_reply);
    set_integer_at(L, host, table_index, "btp_init_stage", inst->btp_init_stage);
    set_integer_at(L, host, table_index, "vendor_write_handle",
                   inst->vendor_write_handle);
    set_integer_at(L, host, table_index, "vendor_subscribed_count",
                   inst->vendor_subscribed_count);
}

static void push_state(lua_State *L, hidpad_instance_t *inst)
{
    inst->host->lua.createtable(L, 0, 32);
    fill_state(L, inst, -2);
}

static void push_input_state(lua_State *L, hidpad_instance_t *inst)
{
    inst->host->lua.createtable(L, 0, 11);
    fill_input_state(L, inst, -2);
}

static hidpad_instance_t *lua_instance(lua_State *L, const hidpad_host_api_t *host)
{
    int index = host->lua.upvalue_index(1);
    return (hidpad_instance_t *)host->lua.touserdata(L, index);
}

static int push_error(lua_State *L, const hidpad_host_api_t *host, const char *error)
{
    host->lua.pushnil(L);
    host->lua.pushstring(L, error ? error : "hidpad error");
    return 2;
}

static int l_start(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (!instance_lock(inst, 1000)) return push_error(L, &s_host, "hidpad state is busy");
    if (inst->started) {
        instance_unlock(inst);
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (s_host.lua.gettop(L) >= 1 && s_host.lua.isnumber(L, 1)) {
        int64_t scan_ms = s_host.lua.tointeger(L, 1);
        if (scan_ms >= 1000 && scan_ms <= 60000) inst->scan_ms = (uint32_t)scan_ms;
    }
    instance_unlock(inst);
    if (runtime_event_mode_supported(inst)) {
        int32_t err = start_worker(inst);
        if (err != MODULE_OK) return push_error(L, &s_host, "hidpad worker start failed");
    } else {
        if (prepare_command(inst, WORKER_COMMAND_START, NULL) != MODULE_OK)
            return push_error(L, &s_host, "controller operation is pending");
        execute_worker_command(inst, WORKER_COMMAND_START);
    }
    s_host.lua.pushboolean(L, 1);
    s_host.lua.pushinteger(L, inst->cold->command_id);
    return 2;
}

static int l_poll(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    int target_table;
    int status_update;
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    target_table = s_host.lua.gettop(L) >= 1 && s_host.lua.istable(L, 1);
    if (runtime_event_mode_supported(inst)) {
        if (!instance_lock(inst, 1000)) return push_error(L, &s_host, "hidpad state is busy");
    } else {
        driver_poll(inst);
    }
    if (!inst->state_dirty) {
        if (runtime_event_mode_supported(inst)) instance_unlock(inst);
        s_host.lua.pushnil(L);
        return 1;
    }
    inst->state_dirty = 0;
    status_update = inst->status_dirty != 0;
    if (status_update) {
        inst->status_dirty = 0;
        if (target_table) fill_state(L, inst, 1);
        else push_state(L, inst);
    } else {
        if (target_table) fill_input_state(L, inst, 1);
        else push_input_state(L, inst);
    }
    if (target_table) s_host.lua.pushvalue(L, 1);
    s_host.lua.pushboolean(L, status_update);
    if (runtime_event_mode_supported(inst)) instance_unlock(inst);
    return 2;
}

static int l_on_event(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (!runtime_event_mode_supported(inst)) {
        s_host.lua.pushboolean(L, 0);
        return 1;
    }
    if (inst->worker_running) {
        return push_error(L, &s_host, "cannot replace event callback while hidpad is running");
    }
    if (inst->event_ref > -1) {
        inst->host->runtime.event_cancel(inst->lua, inst->event_ref);
        inst->host->lua.registry_unref(inst->lua, inst->event_ref);
        inst->event_ref = -2;
    }
    if (inst->host->lua.gettop(L) < 1 || inst->host->lua.isnil(L, 1)) {
        inst->host->lua.pushboolean(L, 1);
        return 1;
    }
    inst->host->lua.pushvalue(L, 1);
    inst->event_ref = inst->host->lua.registry_ref(L);
    inst->host->lua.pushboolean(L, inst->event_ref > -1);
    return 1;
}

static int l_state(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (runtime_event_mode_supported(inst) && !instance_lock(inst, 1000)) {
        return push_error(L, &s_host, "hidpad state is busy");
    }
    push_state(L, inst);
    if (runtime_event_mode_supported(inst)) instance_unlock(inst);
    return 1;
}

static int submit_command(lua_State *L, worker_command_t command, const char *address)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    int32_t err;
    uint32_t id;
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (!instance_lock(inst, 1000)) return push_error(L, &s_host, "hidpad state is busy");
    err = prepare_command(inst, command, address);
    id = inst->cold->command_id;
    if (err == MODULE_OK) {
        if (runtime_event_mode_supported(inst) && inst->worker_running) inst->worker_command = command;
        else execute_worker_command(inst, command);
    }
    instance_unlock(inst);
    if (err != MODULE_OK) return push_error(L, &s_host,
        err == MODULE_ERR_NOT_FOUND ? "selected device is no longer available" : "controller operation is pending");
    if (inst->worker_wake) (void)inst->host->sync.give(inst->worker_wake);
    s_host.lua.pushboolean(L, 1);
    s_host.lua.pushinteger(L, id);
    return 2;
}

static int l_rescan(lua_State *L) { return submit_command(L, WORKER_COMMAND_RESCAN, NULL); }
static int l_scan(lua_State *L) { return submit_command(L, WORKER_COMMAND_SCAN, NULL); }

static int l_diagnostics(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst || !instance_lock(inst, 1000)) return push_error(L, &s_host, "hidpad state is busy");
    s_host.lua.createtable(L, 0, 48);
    fill_diagnostics(L, inst, -2);
    instance_unlock(inst);
    return 1;
}

static int l_scan_count(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (runtime_event_mode_supported(inst) && !instance_lock(inst, 1000)) {
        return push_error(L, &s_host, "hidpad state is busy");
    }
    s_host.lua.pushinteger(L, inst->scan_result_count);
    if (runtime_event_mode_supported(inst)) instance_unlock(inst);
    return 1;
}

static int l_scan_device(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    int64_t requested;
    discovered_device_t *device;
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (s_host.lua.gettop(L) < 1 || !s_host.lua.isnumber(L, 1)) {
        return push_error(L, &s_host, "device index missing");
    }
    requested = s_host.lua.tointeger(L, 1);
    if (runtime_event_mode_supported(inst) && !instance_lock(inst, 1000)) {
        return push_error(L, &s_host, "hidpad state is busy");
    }
    if (requested < 1 || requested > inst->scan_result_count) {
        if (runtime_event_mode_supported(inst)) instance_unlock(inst);
        s_host.lua.pushnil(L);
        return 1;
    }
    device = &inst->cold->scan_results[(uint8_t)requested - 1u];
    s_host.lua.createtable(L, 0, 6);
    set_string_field(L, &s_host, "address", device->address);
    set_string_field(L, &s_host, "name", device->name);
    set_string_field(L, &s_host, "profile", profile_text((device_profile_t)device->profile));
    set_integer_field(L, &s_host, "rssi", device->rssi);
    set_integer_field(L, &s_host, "addr_type", device->addr_type);
    if (runtime_event_mode_supported(inst)) instance_unlock(inst);
    return 1;
}

static int l_connect(lua_State *L)
{
    if (!s_host.lua.isstring(L, 1)) return push_error(L, &s_host, "device address missing");
    return submit_command(L, WORKER_COMMAND_CONNECT, s_host.lua.tostring(L, 1));
}

static int l_set_preferred(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    const char *address = "";
    const char *profile = "";
    const char *name = "";
    device_profile_t parsed_profile;
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (s_host.lua.gettop(L) >= 1 && s_host.lua.isstring(L, 1)) {
        address = s_host.lua.tostring(L, 1);
    }
    if (runtime_event_mode_supported(inst) && !instance_lock(inst, 1000)) {
        return push_error(L, &s_host, "hidpad state is busy");
    }
    copy_text(inst->cold->preferred_address, sizeof(inst->cold->preferred_address),
              address, strlen(address));
    inst->cold->preferred_metadata_valid = 0;
    inst->cold->preferred_name[0] = 0;
    if (s_host.lua.gettop(L) >= 2 && s_host.lua.isnumber(L, 2) &&
        s_host.lua.gettop(L) >= 3 && s_host.lua.isstring(L, 3)) {
        profile = s_host.lua.tostring(L, 3);
        if (parse_profile_text(profile, &parsed_profile)) {
            int64_t addr_type = s_host.lua.tointeger(L, 2);
            inst->cold->preferred_addr_type =
                (uint8_t)(addr_type < 0 ? 0 : (addr_type > 3 ? 3 : addr_type));
            inst->cold->preferred_profile = parsed_profile;
            if (s_host.lua.gettop(L) >= 4 && s_host.lua.isstring(L, 4)) {
                name = s_host.lua.tostring(L, 4);
            }
            copy_text(inst->cold->preferred_name, sizeof(inst->cold->preferred_name),
                      name, strlen(name));
            inst->cold->preferred_metadata_valid = 1;
        }
    }
    if (runtime_event_mode_supported(inst)) instance_unlock(inst);
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_set_auto_connect(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    uint8_t enabled;
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (!s_host.lua.isnumber(L, 1)) return push_error(L, &s_host, "expected 0 or 1");
    enabled = s_host.lua.tointeger(L, 1) != 0;
    if (!instance_lock(inst, 1000)) return push_error(L, &s_host, "hidpad state is busy");
    if (inst->auto_connect != enabled) {
        inst->auto_connect = enabled;
        inst->auto_connect_dirty = 1;
    }
    instance_unlock(inst);
    if (inst->worker_wake) (void)inst->host->sync.give(inst->worker_wake);
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_disconnect(lua_State *L) { return submit_command(L, WORKER_COMMAND_DISCONNECT, NULL); }
static int l_pair(lua_State *L) { return submit_command(L, WORKER_COMMAND_PAIR, NULL); }
static int l_forget(lua_State *L) { return submit_command(L, WORKER_COMMAND_FORGET, NULL); }

static int l_stop(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (runtime_event_mode_supported(inst)) stop_worker(inst);
    else driver_stop(inst);
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static void set_function(lua_State *L, const char *name,
                         module_lua_cfunction_t function, hidpad_instance_t *inst)
{
    s_host.lua.pushlightuserdata(L, inst);
    s_host.lua.pushcclosure(L, function, 1);
    s_host.lua.setfield(L, -2, name);
}

static void set_constant(lua_State *L, const char *name, uint32_t value)
{
    s_host.lua.pushinteger(L, value);
    s_host.lua.setfield(L, -2, name);
}

HIDPAD_EXPORT const module_manifest_t *module_query_v1(void)
{
    return &s_manifest;
}

HIDPAD_EXPORT int32_t module_create_v2(module_host_resolve_v2_fn resolve,
                                       void *resolve_ctx,
                                       const module_open_info_t *info,
                                       void **out_instance)
{
    hidpad_instance_t *inst;
    int32_t err;
    if (!out_instance || !info || info->size < sizeof(module_open_info_t) || info->owner_token == 0) {
        return MODULE_ERR_UNSUPPORTED;
    }
    *out_instance = NULL;
    err = resolve_host(resolve, resolve_ctx);
    if (err != MODULE_OK) return err;
    inst = (hidpad_instance_t *)s_host.heap.calloc(1, sizeof(*inst),
                                                   MODULE_HEAP_INTERNAL | MODULE_HEAP_8BIT);
    if (!inst) inst = (hidpad_instance_t *)s_host.heap.calloc(1, sizeof(*inst),
                                                              MODULE_HEAP_PSRAM | MODULE_HEAP_8BIT);
    if (!inst) inst = (hidpad_instance_t *)s_host.heap.calloc(1, sizeof(*inst), MODULE_HEAP_DEFAULT);
    if (!inst) return MODULE_ERR_NO_MEMORY;
    inst->host = &s_host;
    inst->cold = (hidpad_cold_state_t *)s_host.heap.calloc(
        1, sizeof(*inst->cold), MODULE_HEAP_PSRAM | MODULE_HEAP_8BIT);
    if (!inst->cold) {
        s_host.heap.free(inst);
        return MODULE_ERR_NO_MEMORY;
    }
    inst->owner_token = info->owner_token;
    inst->auto_connect = 1;
    inst->scan_ms = 8000;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->conn_handle = 0xffff;
    inst->phase = PHASE_STOPPED;
    inst->profile = DEVICE_PROFILE_HID;
    inst->event_ref = -2;
    if (!runtime_event_mode_supported(inst) && s_host.serial.println) {
        s_host.serial.println("runtime.event_post unsupported; please update to latest firmware");
    }
    *out_instance = inst;
    return MODULE_OK;
}

HIDPAD_EXPORT int32_t module_luaopen_v1(void *instance, lua_State *L)
{
    hidpad_instance_t *inst = (hidpad_instance_t *)instance;
    if (!inst || !L) return MODULE_ERR_INVALID_ARG;
    inst->lua = L;
    s_host.lua.createtable(L, 0, 32);
    set_string_field(L, &s_host, "VERSION", HIDPAD_VERSION);
    set_function(L, "start", l_start, inst);
    set_function(L, "poll", l_poll, inst);
    set_function(L, "on_event", l_on_event, inst);
    set_function(L, "state", l_state, inst);
    set_function(L, "diagnostics", l_diagnostics, inst);
    set_function(L, "rescan", l_rescan, inst);
    set_function(L, "scan", l_scan, inst);
    set_function(L, "scan_count", l_scan_count, inst);
    set_function(L, "scan_device", l_scan_device, inst);
    set_function(L, "connect", l_connect, inst);
    set_function(L, "set_preferred", l_set_preferred, inst);
    set_function(L, "set_auto_connect", l_set_auto_connect, inst);
    set_function(L, "disconnect", l_disconnect, inst);
    set_function(L, "pair", l_pair, inst);
    set_function(L, "forget", l_forget, inst);
    set_function(L, "stop", l_stop, inst);
    set_constant(L, "BTN_UP", BTN_UP);
    set_constant(L, "BTN_DOWN", BTN_DOWN);
    set_constant(L, "BTN_LEFT", BTN_LEFT);
    set_constant(L, "BTN_RIGHT", BTN_RIGHT);
    set_constant(L, "BTN_A", BTN_A);
    set_constant(L, "BTN_B", BTN_B);
    set_constant(L, "BTN_X", BTN_X);
    set_constant(L, "BTN_Y", BTN_Y);
    set_constant(L, "BTN_L", BTN_LB);
    set_constant(L, "BTN_R", BTN_RB);
    set_constant(L, "BTN_LS", BTN_LS);
    set_constant(L, "BTN_RS", BTN_RS);
    set_constant(L, "BTN_SELECT", BTN_VIEW);
    set_constant(L, "BTN_START", BTN_MENU);
    set_constant(L, "BTN_MENU", BTN_VIEW);
    set_constant(L, "BTN_HOME", BTN_HOME);
    return MODULE_OK;
}

HIDPAD_EXPORT void module_destroy_v1(void *instance)
{
    hidpad_instance_t *inst = (hidpad_instance_t *)instance;
    if (!inst) return;
    if (runtime_event_mode_supported(inst)) {
        stop_worker(inst);
        if (inst->event_ref > -1 && inst->lua) {
            inst->host->runtime.event_cancel(inst->lua, inst->event_ref);
            inst->host->lua.registry_unref(inst->lua, inst->event_ref);
            inst->event_ref = -2;
        }
    } else {
        driver_stop(inst);
    }
    if (inst->host && inst->host->heap.free) {
        inst->host->heap.free(inst->cold);
        inst->host->heap.free(inst);
    }
}
