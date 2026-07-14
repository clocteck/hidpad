#include "module_abi.h"
#include "hid_report_parser.h"

#include <stddef.h>
#include <stdint.h>

#define HIDPAD_VERSION "0.4.2"
#define HIDPAD_EXPORT __attribute__((visibility("default")))
#define HIDPAD_MAX_REPORTS 12
#define HIDPAD_MAX_SCAN_RESULTS 8
#define HIDPAD_EVENT_BUDGET 64
#define HIDPAD_REPORT_CACHE_SIZE 32
#define HIDPAD_KEEPALIVE_MS 15000u
#define HIDPAD_KEEPALIVE_RETRY_MS 3000u
#define HIDPAD_RESCAN_MIN_MS 1000u
#define HIDPAD_RESCAN_MAX_MS 30000u
#define HIDPAD_CONN_INTERVAL_MIN 7u
#define HIDPAD_CONN_INTERVAL_MAX 24u
#define HIDPAD_CONN_LATENCY 0u
#define HIDPAD_CONN_SUPERVISION_TIMEOUT 500u

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
    uint8_t raw_report[244];
    uint16_t raw_report_len;
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

typedef struct hidpad_instance_t {
    const module_host_api_v2 *host;
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
    uint8_t pair_requested;
    uint8_t peer_addr_type;
    uint8_t forget_pending;
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
    char preferred_address[18];
    discovered_device_t scan_results[HIDPAD_MAX_SCAN_RESULTS];
    driver_state_t state;
    /* Reused heap work buffers: internal RAM is preferred for the hot input path. */
    module_ble_event_t event_work;
    module_ble_config_t config_work;
    module_ble_scan_config_t scan_work;
    hidpad_decoded_report_t decoded_work;
    advertisement_t advertisement_work;
    const char *last_error;
} hidpad_instance_t;

static module_host_api_v2 s_host;

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
    inst->state.raw_report_len = 0;
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

static void copy_raw_report(driver_state_t *state, const uint8_t *data, size_t len)
{
    size_t i;
    if (!state) return;
    if (len > sizeof(state->raw_report)) len = sizeof(state->raw_report);
    for (i = 0; i < len; ++i) state->raw_report[i] = data[i];
    state->raw_report_len = (uint16_t)len;
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
                          const hidpad_decoded_report_t *decoded,
                          const uint8_t *raw,
                          size_t raw_len)
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
        copy_raw_report(&inst->state, raw, raw_len);
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
    copy_raw_report(&inst->state, raw, raw_len);
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
        apply_decoded(inst, report, decoded, data, len);
        remember_report(report, data, len, 1);
        return 1;
    }
    if (inst->profile == DEVICE_PROFILE_Q36 &&
        hidpad_q36_decode_android(report_id, data, len, decoded)) {
        apply_decoded(inst, report, decoded, data, len);
        remember_report(report, data, len, 1);
        return 1;
    }
    profile = inst->profile == DEVICE_PROFILE_Q36 ? HIDPAD_PROFILE_Q36 : HIDPAD_PROFILE_GENERIC;
    if (profile == HIDPAD_PROFILE_Q36 && !inst->parser.has_report_id) report_id = 0;
    decoded_ok = hidpad_parser_decode(&inst->parser, report_id, data, len, profile, decoded);
    if (!decoded_ok) {
        copy_raw_report(&inst->state, data, len);
        inst->state.report_id = report_id;
        remember_report(report, data, len, 0);
        return 0;
    }
    apply_decoded(inst, report, decoded, data, len);
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
    if (text_contains(adv->name, "xbox") ||
        (adv->appearance == 0x03c4 && adv->company == 0x0006)) {
        *profile = DEVICE_PROFILE_XBOX;
        return 240;
    }
    if (adv->has_hid || text_contains(adv->name, "q36") ||
        text_contains(adv->name, "shanwan")) {
        *profile = DEVICE_PROFILE_Q36;
    } else {
        *profile = DEVICE_PROFILE_HID;
    }
    if (text_contains(adv->name, "q36") || text_contains(adv->name, "shanwan")) score += 100;
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
    scan = &inst->scan_work;
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
        if (text_equal(inst->scan_results[i].address, event->address)) {
            device = &inst->scan_results[i];
            break;
        }
    }
    if (!device && inst->scan_result_count < HIDPAD_MAX_SCAN_RESULTS) {
        device = &inst->scan_results[inst->scan_result_count++];
        zero_bytes(device, sizeof(*device));
    }
    if (!device && inst->scan_result_count == HIDPAD_MAX_SCAN_RESULTS) {
        uint8_t weakest = 0;
        for (i = 1; i < HIDPAD_MAX_SCAN_RESULTS; ++i) {
            if (inst->scan_results[i].score < inst->scan_results[weakest].score) weakest = i;
        }
        if ((inst->preferred_address[0] && text_equal(inst->preferred_address, event->address)) ||
            score > inst->scan_results[weakest].score) {
            device = &inst->scan_results[weakest];
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

static int connect_device(hidpad_instance_t *inst, const discovered_device_t *device)
{
    int32_t err;
    if (!inst || !device) return 0;
    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
    inst->scan_active = 0;
    inst->profile = (device_profile_t)device->profile;
    inst->peer_addr_type = device->addr_type;
    copy_text(inst->state.address, sizeof(inst->state.address),
              device->address, strlen(device->address));
    copy_text(inst->state.name, sizeof(inst->state.name), device->name, strlen(device->name));
    inst->state.connecting = 1;
    inst->phase = PHASE_CONNECTING;
    mark_status_dirty(inst);
    err = inst->host->ble.gap_connect(inst->session, device->addr_type,
                                     device->address, 15000);
    if (err != MODULE_OK) {
        inst->state.connecting = 0;
        inst->phase = inst->manual_scan ? PHASE_SELECT_DEVICE : PHASE_WAIT_RESCAN;
        mark_status_dirty(inst);
        return 0;
    }
    inst->manual_scan = 0;
    return 1;
}

static void handle_scan_result(hidpad_instance_t *inst, const module_ble_event_t *event)
{
    advertisement_t *adv;
    discovered_device_t *device;
    device_profile_t profile = DEVICE_PROFILE_HID;
    int score;
    uint8_t i;
    if (!inst->scan_active || inst->state.connected || inst->state.connecting) return;
    if (!inst->manual_scan && inst->preferred_address[0] &&
        !text_equal(inst->preferred_address, event->address)) return;
    for (i = 0; i < inst->scan_result_count; ++i) {
        if (!text_equal(inst->scan_results[i].address, event->address)) continue;
        inst->scan_results[i].rssi = event->rssi;
        if (!inst->manual_scan && !connect_device(inst, &inst->scan_results[i])) {
            schedule_rescan_with_backoff(inst);
        }
        return;
    }
    adv = &inst->advertisement_work;
    parse_advertisement(event->data, event->data_len, adv);
    score = score_advertisement(adv, &profile);
    if (score < 40) return;
    device = remember_device(inst, event, adv, profile, score);
    if (!device || inst->manual_scan) return;
    if (inst->preferred_address[0] &&
        !text_equal(inst->preferred_address, device->address)) return;
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
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->pair_requested = 1;
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
    int32_t forget_err = MODULE_OK;
    inst->state.disconnect_reason = event ? event->status : 0;
    if (pairing) inst->last_error = pairing_disconnect_error(inst->state.disconnect_reason);
    inst->conn_handle = 0xffff;
    inst->state.connected = 0;
    inst->state.connecting = 0;
    inst->state.encrypted = 0;
    inst->pair_requested = 0;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
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
    schedule_rescan(inst, inst->manual_scan ? 0 : 1200);
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
        if (!inst->state.connected && !inst->state.connecting) {
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
            /* Q36 for Android exposes a valid keyboard-like map that does not
             * contain fields understood by the gamepad-only parser. Its fixed
             * 10-byte input report is decoded by hidpad_q36_decode_android(). */
            if (!inst->report_map_valid && event->data_len > 0 &&
                (text_contains(inst->state.name, "q36") ||
                 text_contains(inst->state.name, "shanwan"))) {
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
        start_scan(inst);
    }
    poll_input_fallback(inst);
    poll_keepalive(inst);
}

static int driver_start(hidpad_instance_t *inst)
{
    module_ble_config_t *config;
    int32_t err;
    if (!inst) return MODULE_ERR_INVALID_ARG;
    if (inst->started) return MODULE_OK;
    config = &inst->config_work;
    zero_bytes(config, sizeof(*config));
    config->size = sizeof(*config);
    config->mtu = 185;
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
    clear_controls(inst);
    reset_gatt(inst);
    /* start_scan schedules a retry on transient host errors. */
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
    inst->state.connected = 0;
    inst->state.connecting = 0;
    inst->phase = PHASE_STOPPED;
    clear_controls(inst);
    mark_status_dirty(inst);
}

static void set_integer_field(lua_State *L, const module_host_api_v2 *host,
                              const char *name, int64_t value)
{
    host->lua.pushinteger(L, value);
    host->lua.setfield(L, -2, name);
}

static void set_boolean_field(lua_State *L, const module_host_api_v2 *host,
                              const char *name, int value)
{
    host->lua.pushboolean(L, value);
    host->lua.setfield(L, -2, name);
}

static void set_string_field(lua_State *L, const module_host_api_v2 *host,
                             const char *name, const char *value)
{
    host->lua.pushstring(L, value ? value : "");
    host->lua.setfield(L, -2, name);
}

static void push_state(lua_State *L, hidpad_instance_t *inst)
{
    const module_host_api_v2 *host = inst->host;
    host->lua.createtable(L, 0, 27);
    set_integer_field(L, host, "seq", inst->state.seq);
    set_integer_field(L, host, "timestamp_ms", inst->state.timestamp_ms);
    set_integer_field(L, host, "buttons", inst->state.buttons);
    set_integer_field(L, host, "raw_buttons", inst->state.raw_buttons);
    set_integer_field(L, host, "lx", inst->state.lx);
    set_integer_field(L, host, "ly", inst->state.ly);
    set_integer_field(L, host, "rx", inst->state.rx);
    set_integer_field(L, host, "ry", inst->state.ry);
    set_integer_field(L, host, "lt", inst->state.lt);
    set_integer_field(L, host, "rt", inst->state.rt);
    set_integer_field(L, host, "report_id", inst->state.report_id);
    set_boolean_field(L, host, "started", inst->started);
    set_boolean_field(L, host, "connected", inst->state.connected);
    set_boolean_field(L, host, "connecting", inst->state.connecting);
    set_boolean_field(L, host, "encrypted", inst->state.encrypted);
    set_integer_field(L, host, "disconnect_reason", inst->state.disconnect_reason);
    set_boolean_field(L, host, "manual_scan", inst->manual_scan);
    set_integer_field(L, host, "scan_count", inst->scan_result_count);
    set_integer_field(L, host, "keepalive_count", inst->keepalive_count);
    set_boolean_field(L, host, "keepalive_supported", inst->control_point_handle != 0);
    set_string_field(L, host, "phase", phase_text(inst->phase));
    set_string_field(L, host, "profile", profile_text(inst->profile));
    set_string_field(L, host, "address", inst->state.address);
    set_string_field(L, host, "name", inst->state.name);
    set_string_field(L, host, "last_error", inst->last_error);
    host->lua.pushlstring(L, (const char *)inst->state.raw_report, inst->state.raw_report_len);
    host->lua.setfield(L, -2, "raw_report");
}

static void push_input_state(lua_State *L, hidpad_instance_t *inst)
{
    const module_host_api_v2 *host = inst->host;
    host->lua.createtable(L, 0, 11);
    set_integer_field(L, host, "seq", inst->state.seq);
    set_integer_field(L, host, "timestamp_ms", inst->state.timestamp_ms);
    set_integer_field(L, host, "buttons", inst->state.buttons);
    set_integer_field(L, host, "raw_buttons", inst->state.raw_buttons);
    set_integer_field(L, host, "lx", inst->state.lx);
    set_integer_field(L, host, "ly", inst->state.ly);
    set_integer_field(L, host, "rx", inst->state.rx);
    set_integer_field(L, host, "ry", inst->state.ry);
    set_integer_field(L, host, "lt", inst->state.lt);
    set_integer_field(L, host, "rt", inst->state.rt);
    set_integer_field(L, host, "report_id", inst->state.report_id);
}

static hidpad_instance_t *lua_instance(lua_State *L, const module_host_api_v2 *host)
{
    int index = host->lua.upvalue_index(1);
    return (hidpad_instance_t *)host->lua.touserdata(L, index);
}

static int push_error(lua_State *L, const module_host_api_v2 *host, const char *error)
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
    if (driver_start(inst) != MODULE_OK) return push_error(L, &s_host, inst->last_error);
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_poll(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    driver_poll(inst);
    if (!inst->state_dirty) {
        s_host.lua.pushnil(L);
        return 1;
    }
    inst->state_dirty = 0;
    if (inst->status_dirty) {
        inst->status_dirty = 0;
        push_state(L, inst);
    } else {
        push_input_state(L, inst);
    }
    return 1;
}

static int l_state(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    push_state(L, inst);
    return 1;
}

static int l_rescan(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst || !inst->started) return push_error(L, &s_host, "hidpad is not started");
    if (inst->state.connecting) return push_error(L, &s_host, "connection is still in progress");
    inst->manual_scan = 0;
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
    if (!inst || !inst->started) return push_error(L, &s_host, "hidpad is not started");
    if (inst->state.connecting) return push_error(L, &s_host, "connection is still in progress");
    if (inst->scan_active) inst->host->ble.gap_scan_stop(inst->session);
    inst->scan_active = 0;
    inst->manual_scan = 1;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->scan_result_count = 0;
    zero_bytes(inst->scan_results, sizeof(inst->scan_results));
    if (inst->state.connected && inst->conn_handle != 0xffff) {
        if (inst->host->ble.gap_disconnect(inst->session, inst->conn_handle) != MODULE_OK) {
            inst->manual_scan = 0;
            return push_error(L, &s_host, "disconnect before scan failed");
        }
    } else {
        schedule_rescan(inst, 0);
    }
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_scan_count(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    s_host.lua.pushinteger(L, inst->scan_result_count);
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
    if (requested < 1 || requested > inst->scan_result_count) {
        s_host.lua.pushnil(L);
        return 1;
    }
    device = &inst->scan_results[(uint8_t)requested - 1u];
    s_host.lua.createtable(L, 0, 6);
    set_string_field(L, &s_host, "address", device->address);
    set_string_field(L, &s_host, "name", device->name);
    set_string_field(L, &s_host, "profile", profile_text((device_profile_t)device->profile));
    set_integer_field(L, &s_host, "rssi", device->rssi);
    set_integer_field(L, &s_host, "addr_type", device->addr_type);
    return 1;
}

static int l_connect(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    const char *address;
    uint8_t i;
    if (!inst || !inst->started) return push_error(L, &s_host, "hidpad is not started");
    if (inst->state.connected || inst->state.connecting) {
        return push_error(L, &s_host, "gamepad is already connected or connecting");
    }
    if (s_host.lua.gettop(L) < 1 || !s_host.lua.isstring(L, 1)) {
        return push_error(L, &s_host, "device address missing");
    }
    address = s_host.lua.tostring(L, 1);
    for (i = 0; i < inst->scan_result_count; ++i) {
        if (text_equal(inst->scan_results[i].address, address)) {
            copy_text(inst->preferred_address, sizeof(inst->preferred_address),
                      address, strlen(address));
            if (!connect_device(inst, &inst->scan_results[i])) {
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
    if (!inst) return push_error(L, &s_host, "hidpad instance missing");
    if (s_host.lua.gettop(L) >= 1 && s_host.lua.isstring(L, 1)) {
        address = s_host.lua.tostring(L, 1);
    }
    copy_text(inst->preferred_address, sizeof(inst->preferred_address), address, strlen(address));
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_disconnect(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
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
    if (!inst || !inst->state.connected || inst->conn_handle == 0xffff) {
        return push_error(L, &s_host, "gamepad is not connected");
    }
    if (inst->host->ble.gap_pair(inst->session, inst->conn_handle, 1) != MODULE_OK) {
        return push_error(L, &s_host, "pair failed");
    }
    inst->pair_requested = 1;
    s_host.lua.pushboolean(L, 1);
    return 1;
}

static int l_forget(lua_State *L)
{
    hidpad_instance_t *inst = lua_instance(L, &s_host);
    int32_t err;
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
    driver_stop(inst);
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
    err = module_sdk_resolve_host_v2(resolve, resolve_ctx, &s_host);
    if (err != MODULE_OK) return err;
    if (!s_host.ble.open || !s_host.ble.event_poll || !s_host.heap.calloc || !s_host.heap.free ||
        !s_host.lua.createtable || !s_host.lua.pushlstring) return MODULE_ERR_UNSUPPORTED;
    inst = (hidpad_instance_t *)s_host.heap.calloc(1, sizeof(*inst),
                                                   MODULE_HEAP_INTERNAL | MODULE_HEAP_8BIT);
    if (!inst) inst = (hidpad_instance_t *)s_host.heap.calloc(1, sizeof(*inst),
                                                              MODULE_HEAP_PSRAM | MODULE_HEAP_8BIT);
    if (!inst) inst = (hidpad_instance_t *)s_host.heap.calloc(1, sizeof(*inst), MODULE_HEAP_DEFAULT);
    if (!inst) return MODULE_ERR_NO_MEMORY;
    inst->host = &s_host;
    inst->owner_token = info->owner_token;
    inst->scan_ms = 8000;
    inst->rescan_backoff_ms = HIDPAD_RESCAN_MIN_MS;
    inst->conn_handle = 0xffff;
    inst->phase = PHASE_STOPPED;
    inst->profile = DEVICE_PROFILE_HID;
    *out_instance = inst;
    return MODULE_OK;
}

HIDPAD_EXPORT int32_t module_luaopen_v1(void *instance, lua_State *L)
{
    hidpad_instance_t *inst = (hidpad_instance_t *)instance;
    if (!inst || !L) return MODULE_ERR_INVALID_ARG;
    s_host.lua.createtable(L, 0, 32);
    set_string_field(L, &s_host, "VERSION", HIDPAD_VERSION);
    set_function(L, "start", l_start, inst);
    set_function(L, "poll", l_poll, inst);
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
    driver_stop(inst);
    if (inst->host && inst->host->heap.free) inst->host->heap.free(inst);
}
