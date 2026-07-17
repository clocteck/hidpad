#include "module_abi.h"
#include "hid_report_parser.h"

#include <stddef.h>
#include <stdint.h>

#define HIDPAD_VERSION "0.5.1"
#define HIDPAD_EXPORT __attribute__((visibility("default")))
#define HIDPAD_MAX_REPORTS 12
#define HIDPAD_MAX_SCAN_RESULTS 8
#define HIDPAD_EVENT_BUDGET 64
#define HIDPAD_REPORT_CACHE_SIZE 32
#define HIDPAD_KEEPALIVE_MS 15000u
#define HIDPAD_KEEPALIVE_RETRY_MS 3000u
#define HIDPAD_RESCAN_MIN_MS 1000u
#define HIDPAD_RESCAN_MAX_MS 8000u
#define HIDPAD_CONN_INTERVAL_MIN 7u
#define HIDPAD_CONN_INTERVAL_MAX 24u
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
    int32_t (*gattc_write)(module_ble_session_t session, uint16_t conn_handle,
                           uint16_t value_handle, const void *data, size_t data_len,
                           uint32_t mode);
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
#define UUID_CCCD 0x2902u
#define UUID_REPORT_REFERENCE 0x2908u

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
    PHASE_DISCOVER_SERVICES,
    PHASE_DISCOVER_CHARACTERISTICS,
    PHASE_DISCOVER_DESCRIPTORS,
    PHASE_READ_REPORT_MAP,
    PHASE_READ_REPORT_REFERENCES,
    PHASE_SUBSCRIBE,
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

/* Scan/discovery/configuration data is not touched by the ready input path. */
typedef struct hidpad_cold_state_t {
    discovered_device_t scan_results[HIDPAD_MAX_SCAN_RESULTS];
    module_ble_config_t config_work;
    module_ble_scan_config_t scan_work;
    advertisement_t advertisement_work;
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
    uint8_t scan_result_count;
    uint8_t state_dirty;
    uint8_t status_dirty;
    uint8_t peer_addr_type;
    uint8_t forget_pending;
    uint8_t direct_reconnect_pending;
    uint8_t force_scan_once;
    uint32_t scan_ms;
    uint32_t rescan_backoff_ms;
    uint32_t next_scan_ms;
    uint32_t next_input_poll_ms;
    uint32_t next_keepalive_ms;
    uint32_t keepalive_count;
    uint16_t conn_handle;
    uint16_t hid_start;
    uint16_t hid_end;
    uint16_t report_map_handle;
    uint16_t control_point_handle;
    uint8_t control_point_properties;
    report_characteristic_t reports[HIDPAD_MAX_REPORTS];
    uint8_t report_count;
    uint8_t descriptor_index;
    uint8_t open_report_index;
    uint8_t reference_index;
    uint8_t subscribe_index;
    uint8_t subscribed_count;
    uint8_t input_poll_index;
    uint8_t hid_init_attempt;
    uint8_t service_uuid_variant;
    uint8_t report_map_valid;
    uint16_t controls_report_handle;
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
    char worker_command_address[18];
} hidpad_instance_t;

static hidpad_host_api_t s_host;

static const module_manifest_t s_manifest = {
    MODULE_MANIFEST_MAGIC,
    MODULE_SDK_VERSION,
    sizeof(module_manifest_t),
    "hidpad",
    HIDPAD_VERSION,
    "BLE Xbox and HID 0x1812 gamepad driver",
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
    HIDPAD_RESOLVE_REQUIRED(MODULE_PROC_BLE_GATTC_WRITE_V1, s_host.ble.gattc_write);
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
    size_t i;
    uint8_t *out = (uint8_t *)dst;
    for (i = 0; i < len; ++i) out[i] = (uint8_t)value;
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
    size_t len = 0;
    if (!text) return 0;
    while (text[len]) ++len;
    return len;
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
    case PHASE_DISCOVER_SERVICES: return "discover_services";
    case PHASE_DISCOVER_CHARACTERISTICS: return "discover_characteristics";
    case PHASE_DISCOVER_DESCRIPTORS: return "discover_descriptors";
    case PHASE_READ_REPORT_MAP: return "read_report_map";
    case PHASE_READ_REPORT_REFERENCES: return "read_report_references";
    case PHASE_SUBSCRIBE: return "subscribe";
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

static void set_error(hidpad_instance_t *inst, const char *error)
{
    if (!inst) return;
    inst->last_error = error;
    inst->phase = PHASE_ERROR;
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
    inst->next_keepalive_ms = 0;
    zero_bytes(inst->reports, sizeof(inst->reports));
    inst->report_count = 0;
    inst->descriptor_index = 0;
    inst->open_report_index = 0xff;
    inst->reference_index = 0;
    inst->subscribe_index = 0;
    inst->subscribed_count = 0;
    inst->input_poll_index = 0;
    inst->hid_init_attempt = 0;
    inst->service_uuid_variant = 0;
    inst->report_map_valid = 0;
    inst->controls_report_handle = 0;
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
    if (!decoded || !data || len != 16) return 0;
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
    if (inst->profile == DEVICE_PROFILE_Q36 &&
        hidpad_q36_decode_android(report_id, data, len, decoded)) {
        apply_decoded(inst, report, decoded);
        remember_report(report, data, len, 1);
        return 1;
    }
    profile = inst->profile == DEVICE_PROFILE_Q36 ? HIDPAD_PROFILE_Q36 : HIDPAD_PROFILE_GENERIC;
    if (profile == HIDPAD_PROFILE_Q36 && !inst->parser.has_report_id) report_id = 0;
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
    if (adv->has_hid || q36_compatible) {
        *profile = DEVICE_PROFILE_Q36;
    } else {
        *profile = DEVICE_PROFILE_HID;
    }
    if (q36_compatible) score += 100;
    if (adv->has_hid) score += 100;
    if (adv->appearance == 0x03c4 || adv->appearance == 0x03c3) score += 80;
    if (text_contains(adv->name, "gamepad") || text_contains(adv->name, "controller") ||
        text_contains(adv->name, "joystick") || text_contains(adv->name, "8bitdo")) score += 45;
    return score;
}

static void schedule_rescan(hidpad_instance_t *inst, uint32_t delay_ms);
static void schedule_rescan_with_backoff(hidpad_instance_t *inst);

static int start_scan(hidpad_instance_t *inst)
{
    module_ble_scan_config_t *scan;
    int32_t err;
    if (!inst || !inst->started || !inst->host->ble.gap_scan) return 0;
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
    inst->phase = PHASE_SCANNING;
    mark_status_dirty(inst);
    return 1;
}

static void schedule_rescan(hidpad_instance_t *inst, uint32_t delay_ms)
{
    if (!inst) return;
    inst->scan_active = 0;
    inst->next_scan_ms = now_ms(inst) + delay_ms;
    inst->phase = PHASE_WAIT_RESCAN;
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

static void begin_subscribe(hidpad_instance_t *inst);
static void start_hid_service_discovery(hidpad_instance_t *inst);
static void read_next_reference(hidpad_instance_t *inst);

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
    return (report->report_type == 0 || report->report_type == 1) &&
           (report->properties &
            (MODULE_BLE_CHAR_PROP_NOTIFY | MODULE_BLE_CHAR_PROP_INDICATE)) != 0;
}

static void finish_subscribe(hidpad_instance_t *inst)
{
    if (!inst) return;
    if ((inst->profile == DEVICE_PROFILE_Q36 || inst->profile == DEVICE_PROFILE_XBOX) &&
        inst->subscribed_count == 0) {
        fail_hid_initialization(inst, "No notifiable HID input report");
        return;
    }
    inst->phase = PHASE_READY;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->last_error = NULL;
    inst->state.disconnect_reason = 0;
    if (inst->host->ble.gap_set_connection_params) {
        (void)inst->host->ble.gap_set_connection_params(
            inst->session, inst->conn_handle,
            HIDPAD_CONN_INTERVAL_MIN, HIDPAD_CONN_INTERVAL_MAX,
            HIDPAD_CONN_LATENCY, HIDPAD_CONN_SUPERVISION_TIMEOUT);
    }
    inst->next_input_poll_ms = now_ms(inst) + 80;
    inst->next_keepalive_ms = now_ms(inst);
    mark_status_dirty(inst);
}

static void begin_report_map_and_reference_reads(hidpad_instance_t *inst)
{
    if (!inst) return;
    inst->reference_index = 0;
    inst->report_map_valid = 0;
    if (inst->report_map_handle &&
        start_read(inst, inst->report_map_handle, PENDING_READ_MAP, 0)) {
        inst->phase = PHASE_READ_REPORT_MAP;
    } else if (inst->profile == DEVICE_PROFILE_Q36) {
        fail_hid_initialization(inst, "Q36 HID report map not readable");
    } else {
        read_next_reference(inst);
    }
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
            score > inst->cold->scan_results[weakest].score) {
            device = &inst->cold->scan_results[weakest];
            zero_bytes(device, sizeof(*device));
        }
    }
    if (!device) return NULL;
    copy_text(device->address, sizeof(device->address), event->address, strlen(event->address));
    if (adv->name[0]) {
        copy_text(device->name, sizeof(device->name), adv->name, strlen(adv->name));
    } else if (!device->name[0]) {
        const char *fallback = profile == DEVICE_PROFILE_XBOX ?
                               "Xbox Wireless Controller" : "BLE HID Gamepad";
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
    if (inst->cold->preferred_address[0] &&
        text_equal(inst->cold->preferred_address, device->address)) return 1;
    return text_contains(device->name, "xbox") ||
           is_q36_compatible_name(device->name);
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

static void handle_scan_result(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    advertisement_t *adv;
    discovered_device_t *device;
    device_profile_t profile = DEVICE_PROFILE_HID;
    int score;
    uint8_t i;
    if (!inst->scan_active || inst->state.connecting) return;
    for (i = 0; i < inst->scan_result_count; ++i) {
        if (!text_equal(inst->cold->scan_results[i].address, event->address)) continue;
        inst->cold->scan_results[i].rssi = event->rssi;
        if (inst->state.connected) return;
        if (should_auto_connect(inst, &inst->cold->scan_results[i]) &&
            !connect_device(inst, &inst->cold->scan_results[i])) {
            schedule_rescan_with_backoff(inst);
        }
        return;
    }
    adv = &inst->cold->advertisement_work;
    parse_advertisement(event->data, event->data_len, adv);
    score = score_advertisement(adv, &profile);
    if (score < 40) return;
    device = remember_device(inst, event, adv, profile, score);
    if (inst->state.connected) return;
    if (!device || !should_auto_connect(inst, device)) return;
    if (!connect_device(inst, device)) schedule_rescan_with_backoff(inst);
}

static int advance_q36_service_discovery(hidpad_instance_t *inst)
{
    uint8_t next_attempt;
    if (!inst || inst->profile != DEVICE_PROFILE_Q36) return 0;
    if (inst->service_uuid_variant == 0) {
        inst->service_uuid_variant = 1;
        return 1;
    }
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
        uuid = inst->service_uuid_variant == 0 ? UUID_HID_TEXT_16 : UUID_HID_TEXT_128;
        if (inst->host->ble.gattc_discover_services(
                inst->session, inst->conn_handle, uuid) == MODULE_OK) return 1;
        if (!advance_q36_service_discovery(inst)) return 0;
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

static void handle_connected(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    inst->conn_handle = event->conn_handle;
    inst->state.connected = 1;
    inst->state.connecting = 0;
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
    int32_t forget_err = MODULE_OK;
    inst->state.disconnect_reason = event ? event->status : 0;
    if (pairing) inst->last_error = pairing_disconnect_error(inst->state.disconnect_reason);
    inst->conn_handle = 0xffff;
    inst->state.connected = 0;
    inst->state.connecting = 0;
    inst->state.encrypted = 0;
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
    inst->direct_reconnect_pending = !inst->manual_scan && !inst->force_scan_once && was_ready &&
                                     inst->cold->preferred_metadata_valid;
    inst->force_scan_once = 0;
    if (inst->manual_scan) schedule_rescan(inst, 0);
    else if (was_ready) schedule_rescan(inst, 1200);
    else schedule_rescan_with_backoff(inst);
}

static void handle_event(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    report_characteristic_t *report;
    switch (event->irq) {
    case MODULE_BLE_IRQ_SCAN_RESULT:
        handle_scan_result(inst, event);
        break;
    case MODULE_BLE_IRQ_SCAN_DONE:
        inst->scan_active = 0;
        if (inst->state.connected) {
            inst->manual_scan = 0;
            inst->phase = PHASE_READY;
            mark_status_dirty(inst);
        } else if (!inst->state.connecting) {
            if (inst->manual_scan) {
                inst->phase = PHASE_SELECT_DEVICE;
                mark_status_dirty(inst);
            } else {
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
        if (uuid_is16(event->uuid, UUID_HID)) {
            inst->hid_start = event->start_handle;
            inst->hid_end = event->end_handle;
        }
        break;
    case MODULE_BLE_IRQ_GATTC_SERVICE_DONE:
        if (!inst->hid_start || !inst->hid_end) {
            if (!advance_q36_service_discovery(inst) ||
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
        } else if (uuid_is16(event->uuid, UUID_REPORT) && inst->report_count < HIDPAD_MAX_REPORTS) {
            report = &inst->reports[inst->report_count++];
            report->value_handle = event->value_handle;
            report->descriptor_end_handle = inst->hid_end;
            report->properties = event->properties;
            inst->open_report_index = (uint8_t)(inst->report_count - 1u);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_CHARACTERISTIC_DONE:
        if (inst->report_count == 0) {
            fail_hid_initialization(inst, "HID input report not found");
        } else {
            inst->descriptor_index = 0;
            discover_next_report_descriptors(inst);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_DESCRIPTOR_RESULT:
        report = find_report(inst, event->value_handle);
        if (report && uuid_is16(event->uuid, UUID_CCCD)) report->cccd_handle = event->descriptor_handle;
        if (report && uuid_is16(event->uuid, UUID_REPORT_REFERENCE)) report->reference_handle = event->descriptor_handle;
        break;
    case MODULE_BLE_IRQ_GATTC_DESCRIPTOR_DONE:
        inst->descriptor_index++;
        discover_next_report_descriptors(inst);
        break;
    case MODULE_BLE_IRQ_GATTC_READ_RESULT:
        if (inst->pending_read == PENDING_READ_MAP) {
            inst->report_map_valid = hidpad_parser_parse(
                &inst->parser, event->data, event->data_len) ? 1 : 0;
            /* ShanWan Q34/Q36 Android mode exposes a valid keyboard-like map
             * that has no fields understood by the gamepad-only parser. Its
             * fixed 10-byte input report is decoded separately. Q34U commonly
            * advertises as "GamepadSpace-Q34U", not "ShanWan". */
            if (!inst->report_map_valid && event->data_len > 0 &&
                is_q36_compatible_name(inst->state.name)) {
                inst->report_map_valid = 1;
            }
        } else if (inst->pending_read == PENDING_READ_REFERENCE &&
                   inst->pending_report_index < inst->report_count && event->data_len >= 2) {
            report = &inst->reports[inst->pending_report_index];
            report->report_id = event->data[0];
            report->report_type = event->data[1];
        } else if (inst->pending_read == PENDING_READ_INPUT &&
                   inst->pending_report_index < inst->report_count) {
            report = &inst->reports[inst->pending_report_index];
            decode_hid(inst, report, event->data, event->data_len);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_READ_DONE:
        if (inst->pending_read == PENDING_READ_MAP) {
            inst->pending_read = PENDING_READ_NONE;
            if (inst->profile == DEVICE_PROFILE_Q36 && !inst->report_map_valid) {
                fail_hid_initialization(inst, "Q36 HID report map parse failed");
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
        if (inst->phase == PHASE_SUBSCRIBE) {
            if (inst->subscribe_index < inst->report_count && event->status == 0) {
                inst->reports[inst->subscribe_index].subscribed = 1;
                inst->subscribed_count++;
            }
            inst->subscribe_index++;
            subscribe_next(inst);
        }
        break;
    case MODULE_BLE_IRQ_GATTC_NOTIFY:
        report = find_report(inst, event->value_handle);
        if (report && (inst->profile != DEVICE_PROFILE_XBOX ||
                       report->value_handle == inst->controls_report_handle)) {
            decode_hid(inst, report, event->data, event->data_len);
        }
        break;
    case MODULE_BLE_IRQ_ENCRYPTION_UPDATE:
        inst->state.encrypted = event->encrypted;
        mark_status_dirty(inst);
        if (inst->phase == PHASE_PAIRING) {
            if (event->encrypted) {
                start_hid_service_discovery(inst);
            } else {
                set_error(inst, "BLE pairing/encryption failed");
                inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
            }
        }
        break;
    default:
        break;
    }
}

static void poll_input_fallback(hidpad_instance_t *inst)
{
    uint8_t checked = 0;
    uint32_t now = now_ms(inst);
    if (inst->profile == DEVICE_PROFILE_Q36 || inst->profile == DEVICE_PROFILE_XBOX ||
        inst->phase != PHASE_READY || inst->pending_read != PENDING_READ_NONE ||
        (int32_t)(now - inst->next_input_poll_ms) < 0) return;
    inst->next_input_poll_ms = now + 80;
    while (checked++ < inst->report_count) {
        uint8_t index = inst->input_poll_index++;
        report_characteristic_t *report;
        if (inst->input_poll_index >= inst->report_count) inst->input_poll_index = 0;
        if (index >= inst->report_count) index = 0;
        report = &inst->reports[index];
        if ((report->report_type == 0 || report->report_type == 1) &&
            (report->properties & MODULE_BLE_CHAR_PROP_READ) != 0 && !report->subscribed) {
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
        inst->phase != PHASE_READY || !inst->state.connected) return;
    now = now_ms(inst);
    if ((int32_t)(now - inst->next_keepalive_ms) < 0) return;
    inst->next_keepalive_ms = now + HIDPAD_KEEPALIVE_MS;
    if (inst->control_point_handle && inst->host->ble.gattc_write &&
        (inst->control_point_properties &
         (MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE | MODULE_BLE_CHAR_PROP_WRITE)) != 0) {
        uint8_t exit_suspend = 1;
        uint32_t mode = (inst->control_point_properties & MODULE_BLE_CHAR_PROP_WRITE_NO_RESPONSE) != 0 ?
                        MODULE_BLE_WRITE_NO_RESPONSE : MODULE_BLE_WRITE_WITH_RESPONSE;
        if (inst->host->ble.gattc_write(inst->session, inst->conn_handle,
                                        inst->control_point_handle, &exit_suspend,
                                        sizeof(exit_suspend), mode) == MODULE_OK) {
            inst->keepalive_count++;
            return;
        }
    }
    if (inst->pending_read != PENDING_READ_NONE) {
        inst->next_keepalive_ms = now + HIDPAD_KEEPALIVE_RETRY_MS;
        return;
    }
    for (i = 0; i < inst->report_count; ++i) {
        report_characteristic_t *report = &inst->reports[i];
        if ((report->report_type == 0 || report->report_type == 1) &&
            (report->properties & MODULE_BLE_CHAR_PROP_READ) != 0 &&
            start_read(inst, report->value_handle, PENDING_READ_KEEPALIVE, i)) {
            inst->keepalive_count++;
            return;
        }
    }
}

static void driver_poll(hidpad_instance_t *inst)
{
    uint32_t i;
    module_ble_event_t *event;
    if (!inst || !inst->started) return;
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
        inst->direct_reconnect_pending = 0;
        start_scan(inst);
    }
    poll_input_fallback(inst);
    poll_keepalive(inst);
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

static void worker_command_failed(hidpad_instance_t *inst, const char *error)
{
    inst->last_error = error;
    mark_status_dirty(inst);
}

static void execute_worker_command(hidpad_instance_t *inst, worker_command_t command,
                                   const char *address)
{
    uint8_t i;
    int32_t err;
    if (!inst || command == WORKER_COMMAND_NONE) return;
    switch (command) {
    case WORKER_COMMAND_RESCAN:
        if (!inst->started || inst->state.connecting) {
            worker_command_failed(inst, "rescan rejected while driver is busy");
            break;
        }
        inst->manual_scan = 0;
        inst->direct_reconnect_pending = 0;
        inst->force_scan_once = 1;
        inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
        if (inst->scan_active) (void)inst->host->ble.gap_scan_stop(inst->session);
        if (inst->state.connected && inst->conn_handle != 0xffff) {
            (void)inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
        } else {
            inst->state.connecting = 0;
            schedule_rescan(inst, 0);
        }
        break;
    case WORKER_COMMAND_SCAN:
        if (!inst->started || inst->state.connecting) {
            worker_command_failed(inst, "scan rejected while driver is busy");
            break;
        }
        if (inst->scan_active) (void)inst->host->ble.gap_scan_stop(inst->session);
        inst->scan_active = 0;
        inst->manual_scan = 1;
        inst->direct_reconnect_pending = 0;
        inst->force_scan_once = 1;
        inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
        inst->scan_result_count = 0;
        zero_bytes(inst->cold->scan_results, sizeof(inst->cold->scan_results));
        if (!start_scan(inst)) {
            inst->manual_scan = 0;
            worker_command_failed(inst, "scan start failed");
        }
        break;
    case WORKER_COMMAND_CONNECT:
        if (!inst->started || inst->state.connected || inst->state.connecting) {
            worker_command_failed(inst, "connect rejected while driver is busy");
            break;
        }
        for (i = 0; i < inst->scan_result_count; ++i) {
            if (text_equal(inst->cold->scan_results[i].address, address)) {
                copy_text(inst->cold->preferred_address, sizeof(inst->cold->preferred_address),
                          address, strlen(address));
                if (!connect_device(inst, &inst->cold->scan_results[i])) {
                    worker_command_failed(inst, "connect failed");
                }
                return;
            }
        }
        worker_command_failed(inst, "selected device is no longer available");
        break;
    case WORKER_COMMAND_DISCONNECT:
        if (!inst->state.connected || inst->conn_handle == 0xffff ||
            inst->host->ble.gap_disconnect(inst->session, inst->conn_handle) != MODULE_OK) {
            worker_command_failed(inst, "disconnect failed");
        }
        break;
    case WORKER_COMMAND_PAIR:
        if (!inst->state.connected || inst->conn_handle == 0xffff ||
            inst->host->ble.gap_pair(inst->session, inst->conn_handle, 1) != MODULE_OK) {
            worker_command_failed(inst, "pair failed");
        }
        break;
    case WORKER_COMMAND_FORGET:
        if (!inst->started || !inst->host->ble.gap_forget_device) {
            worker_command_failed(inst, "forget controller is unsupported");
            break;
        }
        if (!inst->state.address[0]) {
            if (!inst->host->ble.gap_clear_bonds) {
                worker_command_failed(inst, "controller address is missing");
                break;
            }
            if (inst->scan_active) {
                (void)inst->host->ble.gap_scan_stop(inst->session);
                inst->scan_active = 0;
            }
            err = inst->host->ble.gap_clear_bonds(inst->session);
            if (err != MODULE_OK && err != MODULE_ERR_NOT_FOUND) {
                worker_command_failed(inst, "clear controller bonds failed");
                break;
            }
            inst->last_error = NULL;
            inst->state.disconnect_reason = 0;
            schedule_rescan(inst, 0);
            break;
        }
        if (inst->state.connected && inst->conn_handle != 0xffff) {
            inst->forget_pending = 1;
            if (inst->host->ble.gap_disconnect(inst->session, inst->conn_handle) != MODULE_OK) {
                inst->forget_pending = 0;
                worker_command_failed(inst, "disconnect before forgetting bond failed");
            }
            break;
        }
        if (inst->scan_active) {
            (void)inst->host->ble.gap_scan_stop(inst->session);
            inst->scan_active = 0;
        }
        err = inst->host->ble.gap_forget_device(
            inst->session, inst->peer_addr_type, inst->state.address);
        if (err == MODULE_ERR_NOT_FOUND && inst->host->ble.gap_clear_bonds) {
            err = inst->host->ble.gap_clear_bonds(inst->session);
            if (err == MODULE_ERR_NOT_FOUND) err = MODULE_OK;
        }
        if (err != MODULE_OK) {
            worker_command_failed(inst, "forget controller bond failed");
            break;
        }
        inst->last_error = NULL;
        inst->state.disconnect_reason = 0;
        schedule_rescan(inst, 0);
        break;
    default:
        break;
    }
}

static int32_t queue_worker_command(hidpad_instance_t *inst, worker_command_t command,
                                    const char *address)
{
    if (!inst || !inst->worker_running) return MODULE_ERR_BAD_STATE;
    if (!instance_lock(inst, 1000)) return MODULE_ERR_BUSY;
    if (inst->worker_command != WORKER_COMMAND_NONE) {
        instance_unlock(inst);
        return MODULE_ERR_BUSY;
    }
    inst->worker_command = command;
    copy_text(inst->worker_command_address, sizeof(inst->worker_command_address),
              address ? address : "", address ? strlen(address) : 0);
    instance_unlock(inst);
    (void)inst->host->sync.give(inst->worker_wake);
    return MODULE_OK;
}

static void worker_main(void *arg)
{
    hidpad_instance_t *inst = (hidpad_instance_t *)arg;
    uint32_t wait_ms = 10;
    int should_post = 0;
    if (!inst) return;

    if (instance_lock(inst, 1000)) {
        if (driver_start(inst) != MODULE_OK) mark_status_dirty(inst);
        should_post = inst->state_dirty != 0;
        instance_unlock(inst);
    }
    if (should_post) post_lua_event(inst);

    while (!inst->worker_stop) {
        if (instance_lock(inst, 1000)) {
            worker_command_t command = inst->worker_command;
            char address[18];
            copy_text(address, sizeof(address), inst->worker_command_address,
                      strlen(inst->worker_command_address));
            inst->worker_command = WORKER_COMMAND_NONE;
            inst->worker_command_address[0] = 0;
            execute_worker_command(inst, command, address);
            driver_poll(inst);
            should_post = inst->state_dirty != 0;
            wait_ms = inst->phase == PHASE_READY ? 10u :
                      (inst->phase == PHASE_SCANNING ? 20u : 50u);
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
    inst->worker_running = 0;
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
    if (inst->worker_running) return MODULE_OK;
    destroy_worker_sync(inst);
    err = inst->host->sync.create_mutex(&inst->worker_mutex);
    if (err != MODULE_OK) goto failed;
    err = inst->host->sync.create_counting(1, 0, &inst->worker_wake);
    if (err != MODULE_OK) goto failed;
    err = inst->host->sync.create_counting(1, 0, &inst->worker_stopped);
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
    destroy_worker_sync(inst);
    return err;
}

static void stop_worker(hidpad_instance_t *inst)
{
    if (!inst || !inst->worker_running) return;
    inst->worker_stop = 1;
    (void)inst->host->sync.give(inst->worker_wake);
    (void)inst->host->sync.take(inst->worker_stopped, MODULE_WAIT_FOREVER);
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
    config->mtu = 185;
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
    inst->conn_handle = 0xffff;
    inst->state_dirty = 1;
    inst->status_dirty = 1;
    inst->last_error = NULL;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->keepalive_count = 0;
    inst->direct_reconnect_pending = 0;
    inst->force_scan_once = 0;
    clear_controls(inst);
    reset_gatt(inst);
    /* Always rediscover before connecting. This keeps GATT initialization in
     * the same, reliable order for controllers such as Q34/Q36. */
    start_scan(inst);
    return MODULE_OK;
}

static void driver_stop(hidpad_instance_t *inst)
{
    if (!inst || !inst->started) return;
    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
    if (inst->state.connected && inst->conn_handle != 0xffff) {
        inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
    }
    inst->host->ble.close(inst->session);
    inst->session = 0;
    inst->started = 0;
    inst->scan_active = 0;
    inst->direct_reconnect_pending = 0;
    inst->force_scan_once = 0;
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
}

static void fill_state(lua_State *L, hidpad_instance_t *inst, int table_index)
{
    const hidpad_host_api_t *host = inst->host;
    fill_input_state(L, inst, table_index);
    set_boolean_at(L, host, table_index, "started", inst->started);
    set_boolean_at(L, host, table_index, "connected", inst->state.connected);
    set_boolean_at(L, host, table_index, "connecting", inst->state.connecting);
    set_boolean_at(L, host, table_index, "encrypted", inst->state.encrypted);
    set_integer_at(L, host, table_index, "disconnect_reason", inst->state.disconnect_reason);
    set_boolean_at(L, host, table_index, "manual_scan", inst->manual_scan);
    set_integer_at(L, host, table_index, "scan_count", inst->scan_result_count);
    set_integer_at(L, host, table_index, "keepalive_count", inst->keepalive_count);
    set_boolean_at(L, host, table_index, "keepalive_supported",
                   inst->control_point_handle != 0);
    set_string_at(L, host, table_index, "phase", phase_text(inst->phase));
    set_string_at(L, host, table_index, "profile", profile_text(inst->profile));
    set_string_at(L, host, table_index, "address", inst->state.address);
    set_string_at(L, host, table_index, "name", inst->state.name);
    set_integer_at(L, host, table_index, "addr_type", inst->peer_addr_type);
    set_string_at(L, host, table_index, "last_error", inst->last_error);
}

static void push_state(lua_State *L, hidpad_instance_t *inst)
{
    inst->host->lua.createtable(L, 0, 27);
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
    if (s_host.lua.gettop(L) >= 1 && s_host.lua.isnumber(L, 1)) {
        int64_t scan_ms = s_host.lua.tointeger(L, 1);
        if (scan_ms >= 1000 && scan_ms <= 60000) inst->scan_ms = (uint32_t)scan_ms;
    }
    if (runtime_event_mode_supported(inst)) {
        int32_t err = start_worker(inst);
        if (err != MODULE_OK) return push_error(L, &s_host, "hidpad worker start failed");
    } else if (driver_start(inst) != MODULE_OK) {
        return push_error(L, &s_host, inst->last_error);
    }
    s_host.lua.pushboolean(L, 1);
    return 1;
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

static int l_rescan(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (runtime_event_mode_supported(inst)) {
        if (queue_worker_command(inst, WORKER_COMMAND_RESCAN, NULL) != MODULE_OK) {
            return push_error(L, &s_host, "hidpad command queue is busy");
        }
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (!inst || !inst->started) return push_error(L, &s_host, "hidpad is not started");
    if (inst->state.connecting) return push_error(L, &s_host, "connection is still in progress");
    inst->manual_scan = 0;
    inst->direct_reconnect_pending = 0;
    inst->force_scan_once = 1;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
    if (inst->state.connected && inst->conn_handle != 0xffff) {
        inst->host->ble.gap_disconnect(inst->session, inst->conn_handle);
    } else {
        inst->state.connecting = 0;
        schedule_rescan(inst, 0);
    }
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_scan(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (runtime_event_mode_supported(inst)) {
        if (queue_worker_command(inst, WORKER_COMMAND_SCAN, NULL) != MODULE_OK) {
            return push_error(L, &s_host, "hidpad command queue is busy");
        }
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (!inst || !inst->started) return push_error(L, &s_host, "hidpad is not started");
    if (inst->state.connecting) return push_error(L, &s_host, "connection is still in progress");
    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
    inst->scan_active = 0;
    inst->manual_scan = 1;
    inst->direct_reconnect_pending = 0;
    inst->force_scan_once = 1;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->scan_result_count = 0;
    zero_bytes(inst->cold->scan_results, sizeof(inst->cold->scan_results));
    if (!start_scan(inst)) {
        inst->manual_scan = 0;
        return push_error(L, &s_host, "scan start failed");
    }
    s_host.lua.pushboolean(L, 1);
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
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    const char *address;
    uint8_t i;
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (s_host.lua.gettop(L) < 1 || !s_host.lua.isstring(L, 1)) {
        return push_error(L, &s_host, "device address missing");
    }
    address = s_host.lua.tostring(L, 1);
    if (runtime_event_mode_supported(inst)) {
        if (queue_worker_command(inst, WORKER_COMMAND_CONNECT, address) != MODULE_OK) {
            return push_error(L, &s_host, "hidpad command queue is busy");
        }
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (!inst->started) return push_error(L, &s_host, "hidpad is not started");
    if (inst->state.connected || inst->state.connecting) {
        return push_error(L, &s_host, "gamepad is already connected or connecting");
    }
    for (i = 0; i < inst->scan_result_count; ++i) {
        if (text_equal(inst->cold->scan_results[i].address, address)) {
            copy_text(inst->cold->preferred_address, sizeof(inst->cold->preferred_address),
                      address, strlen(address));
            if (!connect_device(inst, &inst->cold->scan_results[i])) {
                return push_error(L, &s_host, "connect failed");
            }
            s_host.lua.pushboolean(L, 1);
            return 1;
        }
    }
    return push_error(L, &s_host, "selected device is no longer available");
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
    if (!address[0]) inst->direct_reconnect_pending = 0;
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

static int l_disconnect(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (runtime_event_mode_supported(inst)) {
        if (queue_worker_command(inst, WORKER_COMMAND_DISCONNECT, NULL) != MODULE_OK) {
            return push_error(L, &s_host, "hidpad command queue is busy");
        }
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (!inst || !inst->state.connected || inst->conn_handle == 0xffff) {
        return push_error(L, &s_host, "gamepad is not connected");
    }
    if (inst->host->ble.gap_disconnect(inst->session, inst->conn_handle) != MODULE_OK) {
        return push_error(L, &s_host, "disconnect failed");
    }
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_pair(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (runtime_event_mode_supported(inst)) {
        if (queue_worker_command(inst, WORKER_COMMAND_PAIR, NULL) != MODULE_OK) {
            return push_error(L, &s_host, "hidpad command queue is busy");
        }
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (!inst || !inst->state.connected || inst->conn_handle == 0xffff) {
        return push_error(L, &s_host, "gamepad is not connected");
    }
    if (inst->host->ble.gap_pair(inst->session, inst->conn_handle, 1) != MODULE_OK) {
        return push_error(L, &s_host, "pair failed");
    }
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_forget(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    int32_t err;
    if (runtime_event_mode_supported(inst)) {
        if (queue_worker_command(inst, WORKER_COMMAND_FORGET, NULL) != MODULE_OK) {
            return push_error(L, &s_host, "hidpad command queue is busy");
        }
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (!inst || !inst->started) return push_error(L, &s_host, "hidpad is not started");
    if (!inst->host->ble.gap_forget_device) {
        return push_error(L, &s_host, "firmware does not support forgetting bonds");
    }
    if (!inst->state.address[0]) {
        if (!inst->host->ble.gap_clear_bonds) {
            return push_error(L, &s_host, "controller address is missing");
        }
        if (inst->scan_active) {
            inst->host->ble.gap_scan_stop(inst->session);
            inst->scan_active = 0;
        }
        err = inst->host->ble.gap_clear_bonds(inst->session);
        if (err != MODULE_OK && err != MODULE_ERR_NOT_FOUND) {
            return push_error(L, &s_host, "clear controller bonds failed");
        }
        inst->last_error = NULL;
        inst->state.disconnect_reason = 0;
        schedule_rescan(inst, 0);
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (inst->state.connected && inst->conn_handle != 0xffff) {
        inst->forget_pending = 1;
        if (inst->host->ble.gap_disconnect(inst->session, inst->conn_handle) != MODULE_OK) {
            inst->forget_pending = 0;
            return push_error(L, &s_host, "disconnect before forgetting bond failed");
        }
        s_host.lua.pushboolean(L, 1);
        return 1;
    }
    if (inst->scan_active) {
        inst->host->ble.gap_scan_stop(inst->session);
        inst->scan_active = 0;
    }
    err = inst->host->ble.gap_forget_device(
        inst->session, inst->peer_addr_type, inst->state.address);
    if (err == MODULE_ERR_NOT_FOUND && inst->host->ble.gap_clear_bonds) {
        err = inst->host->ble.gap_clear_bonds(inst->session);
        if (err == MODULE_ERR_NOT_FOUND) err = MODULE_OK;
    }
    if (err != MODULE_OK) return push_error(L, &s_host, "forget controller bond failed");
    inst->last_error = NULL;
    inst->state.disconnect_reason = 0;
    schedule_rescan(inst, 0);
    s_host.lua.pushboolean(L, 1);
    return 1;
}

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
    set_function(L, "rescan", l_rescan, inst);
    set_function(L, "scan", l_scan, inst);
    set_function(L, "scan_count", l_scan_count, inst);
    set_function(L, "scan_device", l_scan_device, inst);
    set_function(L, "connect", l_connect, inst);
    set_function(L, "set_preferred", l_set_preferred, inst);
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
