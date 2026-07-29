#include "hid_report_parser.h"

#define USAGE_PAGE_GENERIC_DESKTOP 0x01u
#define USAGE_PAGE_SIMULATION 0x02u
#define USAGE_PAGE_BUTTON 0x09u
#define USAGE_PAGE_CONSUMER 0x0cu

#define USAGE_JOYSTICK 0x04u
#define USAGE_GAME_PAD 0x05u
#define USAGE_MULTI_AXIS_CONTROLLER 0x08u
#define USAGE_X 0x30u
#define USAGE_Y 0x31u
#define USAGE_Z 0x32u
#define USAGE_RX 0x33u
#define USAGE_RY 0x34u
#define USAGE_RZ 0x35u
#define USAGE_HAT 0x39u
#define USAGE_DPAD_UP 0x90u
#define USAGE_DPAD_DOWN 0x91u
#define USAGE_DPAD_RIGHT 0x92u
#define USAGE_DPAD_LEFT 0x93u
#define USAGE_ACCELERATOR 0xc4u
#define USAGE_BRAKE 0xc5u

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
#define BTN_MEDIA (1u << 16)
#define BTN_VOLUME_UP (1u << 17)
#define BTN_VOLUME_DOWN (1u << 18)
#define BTN_VOLUME_MUTE (1u << 19)

typedef struct global_state_t {
    uint16_t usage_page;
    int32_t logical_min;
    int32_t logical_max;
    uint8_t report_size;
    uint8_t report_count;
    uint8_t report_id;
} global_state_t;

typedef struct local_state_t {
    uint32_t usages[24];
    uint8_t usage_count;
    uint32_t usage_min;
    uint32_t usage_max;
    uint8_t has_usage_min;
    uint8_t has_usage_max;
} local_state_t;

/* Single BLE owner calls the parser serially; static workspace avoids Lua task stack use. */
typedef struct parser_workspace_t {
    global_state_t global;
    local_state_t local;
    global_state_t stack[4];
    uint16_t offsets[HIDPAD_MAX_REPORT_LAYOUTS];
    uint8_t offset_ids[HIDPAD_MAX_REPORT_LAYOUTS];
    uint8_t gamepad_collection[8];
} parser_workspace_t;

static parser_workspace_t s_parse_work;

/*
 * ESP32-S31's RISC-V libgcc is not built as PIC, so pulling __divdi3 into a
 * shared module makes the linker emit unsupported absolute relocations.
 * Keep the two report-normalization divisions module-local and PIC-safe.
 */
static int64_t divide_s64(int64_t numerator, int64_t denominator)
{
    uint64_t dividend;
    uint64_t divisor;
    uint64_t quotient = 0;
    uint64_t remainder = 0;
    uint8_t negative;
    uint8_t bit;

    if (denominator == 0) {
        return 0;
    }

    negative = (uint8_t)((numerator < 0) != (denominator < 0));
    dividend = numerator < 0
        ? (uint64_t)(-(numerator + 1)) + 1u
        : (uint64_t)numerator;
    divisor = denominator < 0
        ? (uint64_t)(-(denominator + 1)) + 1u
        : (uint64_t)denominator;

    for (bit = 0; bit < 64u; ++bit) {
        remainder = (remainder << 1u) | (dividend >> 63u);
        dividend <<= 1u;
        quotient <<= 1u;
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= 1u;
        }
    }

    return negative ? (int64_t)(~quotient + 1u) : (int64_t)quotient;
}

static void zero_bytes(void *ptr, size_t len)
{
    size_t i;
    uint8_t *bytes = (uint8_t *)ptr;
    if (!bytes) {
        return;
    }
    for (i = 0; i < len; ++i) {
        bytes[i] = 0;
    }
}

static int32_t sign_extend(uint32_t value, uint8_t item_size)
{
    uint8_t bits;
    uint32_t sign;
    uint32_t full;
    if (item_size == 0) {
        return 0;
    }
    if (item_size >= 4) {
        return (int32_t)value;
    }
    bits = (uint8_t)(item_size * 8u);
    sign = 1u << (bits - 1u);
    full = 1u << bits;
    return (value & sign) ? (int32_t)(value - full) : (int32_t)value;
}

static void split_usage(uint32_t raw, uint16_t default_page, uint16_t *page, uint16_t *usage)
{
    if (raw > 0xffffu) {
        *page = (uint16_t)(raw >> 16);
        *usage = (uint16_t)raw;
    } else {
        *page = default_page;
        *usage = (uint16_t)raw;
    }
}

static int read_bits(const uint8_t *data, size_t len, uint16_t offset,
                     uint8_t size, int is_signed, int32_t *out)
{
    size_t byte_offset;
    uint8_t shift;
    uint8_t byte_count;
    uint8_t index;
    uint32_t value = 0;
    uint32_t mask;
    if (!data || !out || size == 0 || size > 31 || (size_t)offset + size > len * 8u) {
        return 0;
    }
    byte_offset = offset / 8u;
    shift = (uint8_t)(offset % 8u);
    byte_count = (uint8_t)((shift + size + 7u) / 8u);
    if (byte_count <= 4u) {
        for (index = 0; index < byte_count; ++index) {
            value |= (uint32_t)data[byte_offset + index] << (index * 8u);
        }
        value >>= shift;
    } else {
        uint8_t bit;
        for (bit = 0; bit < size; ++bit) {
            size_t source = (size_t)offset + bit;
            if ((data[source / 8u] & (1u << (source % 8u))) != 0) value |= 1u << bit;
        }
    }
    mask = (1u << size) - 1u;
    value &= mask;
    if (is_signed && (value & (1u << (size - 1u))) != 0) {
        value |= ~mask;
    }
    *out = (int32_t)value;
    return 1;
}

static int16_t normalize_axis(int32_t raw, int32_t min_value, int32_t max_value, int invert)
{
    int64_t numerator;
    int64_t denominator;
    int64_t value;
    if (max_value == min_value) {
        return 0;
    }
    numerator = (int64_t)raw * 2 - (int64_t)min_value - (int64_t)max_value;
    denominator = (int64_t)max_value - (int64_t)min_value;
    value = divide_s64(numerator * 32767, denominator);
    if (invert) {
        value = -value;
    }
    if (value > 32767) value = 32767;
    if (value < -32767) value = -32767;
    return (int16_t)value;
}

static uint16_t normalize_trigger(int32_t raw, int32_t min_value, int32_t max_value)
{
    int64_t value;
    if (max_value == min_value) {
        return 0;
    }
    value = divide_s64(((int64_t)raw - min_value) * 65535,
                       (int64_t)max_value - min_value);
    if (value < 0) value = 0;
    if (value > 65535) value = 65535;
    return (uint16_t)value;
}

static void apply_button(hidpad_decoded_report_t *out, uint16_t usage,
                         int pressed, hidpad_profile_t profile)
{
    if (!pressed || !out) {
        return;
    }
    if (usage >= 1 && usage <= 32) {
        out->raw_buttons |= 1u << (usage - 1u);
    }
    if (profile == HIDPAD_PROFILE_Q36) {
        switch (usage) {
        case 1: out->buttons |= BTN_A; break;
        case 2: out->buttons |= BTN_B; break;
        case 4: out->buttons |= BTN_X; break;
        case 5: out->buttons |= BTN_Y; break;
        case 7: out->buttons |= BTN_LB; break;
        case 8: out->buttons |= BTN_RB; break;
        case 9: out->valid_mask |= HIDPAD_VALID_LT; out->lt = 65535; break;
        case 10: out->valid_mask |= HIDPAD_VALID_RT; out->rt = 65535; break;
        case 11: out->buttons |= BTN_VIEW; break;
        case 12: out->buttons |= BTN_MENU; break;
        case 13: out->buttons |= BTN_HOME; break;
        case 14: out->buttons |= BTN_SHARE; break;
        default: break;
        }
        return;
    }
    switch (usage) {
    case 1: out->buttons |= BTN_A; break;
    case 2: out->buttons |= BTN_B; break;
    case 3: out->buttons |= BTN_X; break;
    case 4: out->buttons |= BTN_Y; break;
    case 5: out->buttons |= BTN_LB; break;
    case 6: out->buttons |= BTN_RB; break;
    case 9: out->buttons |= BTN_VIEW; break;
    case 10: out->buttons |= BTN_MENU; break;
    case 11: out->buttons |= BTN_LS; break;
    case 12: out->buttons |= BTN_RS; break;
    case 13: out->buttons |= BTN_HOME; break;
    case 14: out->buttons |= BTN_SHARE; break;
    default: break;
    }
}

static void apply_consumer(hidpad_decoded_report_t *out, uint16_t usage, int pressed)
{
    if (!pressed || !out) return;
    switch (usage) {
    case 0x0040: out->consumer_buttons |= BTN_MENU; break;
    case 0x00cd: out->consumer_buttons |= BTN_MEDIA; break;
    case 0x00e2: out->consumer_buttons |= BTN_VOLUME_MUTE; break;
    case 0x00e9: out->consumer_buttons |= BTN_VOLUME_UP; break;
    case 0x00ea: out->consumer_buttons |= BTN_VOLUME_DOWN; break;
    case 0x0223: out->consumer_buttons |= BTN_HOME; break;
    case 0x0224: out->consumer_buttons |= BTN_VIEW; break;
    default: break;
    }
}

static int field_is_relevant(uint16_t usage_page, uint16_t usage)
{
    if (usage_page == USAGE_PAGE_BUTTON || usage_page == USAGE_PAGE_CONSUMER) return 1;
    if (usage_page == USAGE_PAGE_SIMULATION) {
        return usage == USAGE_ACCELERATOR || usage == USAGE_BRAKE;
    }
    if (usage_page != USAGE_PAGE_GENERIC_DESKTOP) return 0;
    return usage == USAGE_X || usage == USAGE_Y || usage == USAGE_Z ||
           usage == USAGE_RX || usage == USAGE_RY || usage == USAGE_RZ ||
           usage == USAGE_HAT || usage == USAGE_DPAD_UP ||
           usage == USAGE_DPAD_DOWN || usage == USAGE_DPAD_RIGHT ||
           usage == USAGE_DPAD_LEFT;
}

static int local_is_gamepad_application(const global_state_t *global,
                                        const local_state_t *local)
{
    uint16_t usage_page;
    uint16_t usage;
    if (!global || !local || local->usage_count == 0) return 0;
    split_usage(local->usages[0], global->usage_page, &usage_page, &usage);
    return usage_page == USAGE_PAGE_GENERIC_DESKTOP &&
           (usage == USAGE_JOYSTICK || usage == USAGE_GAME_PAD ||
            usage == USAGE_MULTI_AXIS_CONTROLLER);
}

static void apply_hat(hidpad_decoded_report_t *out, int32_t raw,
                      int32_t logical_min, int32_t logical_max)
{
    int32_t dir = -1;
    /* Preserve the old Q36 parser: the Report Map logical range decides
     * whether the hat is 1..8 or 0..7. */
    if (logical_min == 1 && logical_max >= 8 && raw >= 1 && raw <= 8) {
        dir = raw - 1;
    } else if (raw >= 0 && raw <= 7) {
        dir = raw;
    }
    switch (dir) {
    case 0: out->buttons |= BTN_UP; break;
    case 1: out->buttons |= BTN_UP | BTN_RIGHT; break;
    case 2: out->buttons |= BTN_RIGHT; break;
    case 3: out->buttons |= BTN_DOWN | BTN_RIGHT; break;
    case 4: out->buttons |= BTN_DOWN; break;
    case 5: out->buttons |= BTN_DOWN | BTN_LEFT; break;
    case 6: out->buttons |= BTN_LEFT; break;
    case 7: out->buttons |= BTN_UP | BTN_LEFT; break;
    default: break;
    }
}

int hidpad_q36_decode_android(uint8_t report_id,
                              const uint8_t *data,
                              size_t len,
                              hidpad_decoded_report_t *out)
{
    size_t offset = 0;
    uint8_t buttons;
    uint8_t system;
    if (!data || !out) return 0;
    if (len == 11) {
        offset = 1;
        if (report_id == 0) report_id = data[0];
    } else if (len != 10) {
        return 0;
    }

    zero_bytes(out, sizeof(*out));
    out->report_id = report_id;
    out->valid_mask = HIDPAD_VALID_GAME_BUTTONS |
                      HIDPAD_VALID_LX | HIDPAD_VALID_LY |
                      HIDPAD_VALID_RX | HIDPAD_VALID_RY |
                      HIDPAD_VALID_LT | HIDPAD_VALID_RT;
    out->lx = normalize_axis(data[offset], 0, 255, 0);
    out->ly = normalize_axis(data[offset + 1], 0, 255, 1);
    out->rx = normalize_axis(data[offset + 2], 0, 255, 0);
    out->ry = normalize_axis(data[offset + 3], 0, 255, 1);
    out->lt = (uint16_t)((uint16_t)data[offset + 7] * 257u);
    out->rt = (uint16_t)((uint16_t)data[offset + 8] * 257u);
    apply_hat(out, data[offset + 4], 0, 7);

    buttons = data[offset + 5];
    system = data[offset + 6];
    out->raw_buttons = (uint32_t)buttons | ((uint32_t)system << 8);
    if (buttons & (1u << 0)) out->buttons |= BTN_A;
    if (buttons & (1u << 1)) out->buttons |= BTN_B;
    if (buttons & (1u << 3)) out->buttons |= BTN_X;
    if (buttons & (1u << 4)) out->buttons |= BTN_Y;
    if (buttons & (1u << 6)) out->buttons |= BTN_LB;
    if (buttons & (1u << 7)) out->buttons |= BTN_RB;
    if (system & (1u << 2)) out->buttons |= BTN_VIEW;
    if (system & (1u << 3)) out->buttons |= BTN_MENU;
    if (system & (1u << 4)) out->buttons |= BTN_HOME;
    if (system & (1u << 5)) out->buttons |= BTN_SHARE;
    return 1;
}

static void set_layout(hidpad_report_parser_t *parser, uint8_t report_id, uint16_t bits)
{
    uint8_t i;
    hidpad_report_layout_t *layout;
    for (i = 0; i < parser->layout_count; ++i) {
        if (parser->layouts[i].report_id == report_id) {
            layout = &parser->layouts[i];
            layout->bits = bits;
            layout->payload_bytes = (uint16_t)((bits + 7u) / 8u);
            layout->total_bytes = (uint16_t)(layout->payload_bytes +
                ((parser->has_report_id && report_id != 0) ? 1u : 0u));
            return;
        }
    }
    if (parser->layout_count >= HIDPAD_MAX_REPORT_LAYOUTS) return;
    layout = &parser->layouts[parser->layout_count++];
    layout->report_id = report_id;
    layout->bits = bits;
    layout->payload_bytes = (uint16_t)((bits + 7u) / 8u);
    layout->total_bytes = (uint16_t)(layout->payload_bytes +
        ((parser->has_report_id && report_id != 0) ? 1u : 0u));
}

void hidpad_parser_clear(hidpad_report_parser_t *parser)
{
    if (parser) zero_bytes(parser, sizeof(*parser));
}

int hidpad_parser_parse(hidpad_report_parser_t *parser, const uint8_t *data, size_t len)
{
    global_state_t *global = &s_parse_work.global;
    local_state_t *local = &s_parse_work.local;
    global_state_t *stack = s_parse_work.stack;
    uint8_t stack_depth = 0;
    uint16_t *offsets = s_parse_work.offsets;
    uint8_t *offset_ids = s_parse_work.offset_ids;
    uint8_t offset_count = 0;
    uint8_t collection_depth = 0;
    size_t index = 0;
    uint8_t i;
    if (!parser || !data || len == 0) return 0;
    hidpad_parser_clear(parser);
    zero_bytes(&s_parse_work, sizeof(s_parse_work));
    global->logical_max = 1;

    while (index < len) {
        uint8_t prefix = data[index++];
        uint8_t size_code;
        uint8_t item_size;
        uint8_t item_type;
        uint8_t item_tag;
        uint32_t unsigned_value = 0;
        int32_t signed_value;
        uint8_t b;
        if (prefix == 0xfe) {
            uint8_t long_size;
            if (index + 1 >= len) break;
            long_size = data[index];
            index += 2u + long_size;
            continue;
        }
        size_code = prefix & 3u;
        item_size = size_code == 3u ? 4u : size_code;
        item_type = (prefix >> 2u) & 3u;
        item_tag = (prefix >> 4u) & 15u;
        if (index + item_size > len) break;
        for (b = 0; b < item_size; ++b) {
            unsigned_value |= (uint32_t)data[index + b] << (8u * b);
        }
        signed_value = sign_extend(unsigned_value, item_size);
        index += item_size;

        if (item_type == 1u) {
            switch (item_tag) {
            case 0: global->usage_page = (uint16_t)unsigned_value; break;
            case 1: global->logical_min = signed_value; break;
            /* HID logical maxima are unsigned when Logical Minimum is
             * non-negative.  Treating 0xff as -1 breaks the very common
             * 0..255 axis range used by Android BLE gamepads. */
            case 2: global->logical_max = global->logical_min < 0 ?
                                          signed_value : (int32_t)unsigned_value; break;
            case 7: global->report_size = (uint8_t)unsigned_value; break;
            case 8: global->report_id = (uint8_t)unsigned_value; parser->has_report_id = 1; break;
            case 9: global->report_count = (uint8_t)unsigned_value; break;
            case 10: if (stack_depth < 4) stack[stack_depth++] = *global; break;
            case 11: if (stack_depth > 0) *global = stack[--stack_depth]; break;
            default: break;
            }
        } else if (item_type == 2u) {
            if (item_tag == 0u && local->usage_count < 24) {
                local->usages[local->usage_count++] = unsigned_value;
            } else if (item_tag == 1u) {
                local->usage_min = unsigned_value;
                local->has_usage_min = 1;
            } else if (item_tag == 2u) {
                local->usage_max = unsigned_value;
                local->has_usage_max = 1;
            }
        } else if (item_type == 0u) {
            if (item_tag == 8u) {
                uint16_t *offset = NULL;
                uint16_t start_offset;
                uint16_t bit_len;
                uint8_t field_index;
                int is_constant = (unsigned_value & 1u) != 0;
                int is_variable = (unsigned_value & 2u) != 0;
                for (i = 0; i < offset_count; ++i) {
                    if (offset_ids[i] == global->report_id) offset = &offsets[i];
                }
                if (!offset && offset_count < HIDPAD_MAX_REPORT_LAYOUTS) {
                    offset_ids[offset_count] = global->report_id;
                    offset = &offsets[offset_count++];
                }
                start_offset = offset ? *offset : 0;
                bit_len = (uint16_t)global->report_size * global->report_count;
                int in_gamepad_collection = collection_depth > 0 &&
                    s_parse_work.gamepad_collection[collection_depth - 1u] != 0;
                if (in_gamepad_collection && !is_constant && global->report_size > 0 &&
                    global->report_count > 0) {
                    for (field_index = 0; field_index < global->report_count; ++field_index) {
                        uint32_t raw_usage = 0;
                        uint16_t usage_page;
                        uint16_t usage;
                        hidpad_report_field_t *field;
                        if (field_index < local->usage_count) raw_usage = local->usages[field_index];
                        else if (local->has_usage_min) raw_usage = local->usage_min + field_index;
                        split_usage(raw_usage, global->usage_page, &usage_page, &usage);
                        if (!field_is_relevant(usage_page, usage)) continue;
                        if (parser->field_count >= HIDPAD_MAX_REPORT_FIELDS) break;
                        field = &parser->fields[parser->field_count++];
                        field->report_id = global->report_id;
                        field->offset_bits = (uint16_t)(start_offset + field_index * global->report_size);
                        field->size_bits = global->report_size;
                        field->logical_min = global->logical_min;
                        field->logical_max = global->logical_max;
                        field->variable = is_variable ? 1 : 0;
                        field->usage_page = usage_page;
                        field->usage = usage;
                    }
                }
                if (offset) *offset = (uint16_t)(start_offset + bit_len);
            } else if (item_tag == 10u) {
                int in_gamepad_collection = collection_depth > 0 &&
                    s_parse_work.gamepad_collection[collection_depth - 1u] != 0;
                /* Collection type 1 is Application. Only Generic Desktop
                 * Joystick/Game Pad/Multi-axis applications describe game
                 * controls; Digitizer X/Y values must never become sticks. */
                if ((unsigned_value & 0xffu) == 1u) {
                    in_gamepad_collection = local_is_gamepad_application(global, local);
                }
                if (collection_depth < sizeof(s_parse_work.gamepad_collection)) {
                    s_parse_work.gamepad_collection[collection_depth++] =
                        in_gamepad_collection ? 1u : 0u;
                }
            } else if (item_tag == 12u) {
                if (collection_depth > 0) collection_depth--;
            }
            /* HID local items apply to exactly one following Main item. Clear
             * them after Input, Output, Feature and Collection items alike so
             * an output report's Usage list cannot leak into a later input
             * report (common on full HOGP controllers such as APEX 5). */
            zero_bytes(local, sizeof(*local));
        }
    }
    for (i = 0; i < offset_count; ++i) set_layout(parser, offset_ids[i], offsets[i]);
    for (i = 0; i < parser->field_count; ++i) {
        uint16_t j;
        uint16_t lowest = i;
        for (j = (uint16_t)(i + 1u); j < parser->field_count; ++j) {
            if (parser->fields[j].report_id < parser->fields[lowest].report_id) lowest = j;
        }
        if (lowest != i) {
            hidpad_report_field_t swap = parser->fields[i];
            parser->fields[i] = parser->fields[lowest];
            parser->fields[lowest] = swap;
        }
    }
    for (i = 0; i < parser->layout_count; ++i) {
        hidpad_report_layout_t *layout = &parser->layouts[i];
        uint16_t field_index;
        layout->first_field = parser->field_count;
        layout->field_count = 0;
        layout->has_rx = 0;
        layout->has_ry = 0;
        for (field_index = 0; field_index < parser->field_count; ++field_index) {
            hidpad_report_field_t *field = &parser->fields[field_index];
            if (field->report_id != layout->report_id) continue;
            if (layout->field_count == 0) layout->first_field = field_index;
            layout->field_count++;
            if (field->usage_page == USAGE_PAGE_GENERIC_DESKTOP && field->usage == USAGE_RX) layout->has_rx = 1;
            if (field->usage_page == USAGE_PAGE_GENERIC_DESKTOP && field->usage == USAGE_RY) layout->has_ry = 1;
        }
    }
    return parser->field_count > 0;
}

static const hidpad_report_layout_t *find_layout(const hidpad_report_parser_t *parser,
                                                  uint8_t report_id)
{
    uint8_t i;
    for (i = 0; i < parser->layout_count; ++i) {
        if (parser->layouts[i].report_id == report_id) return &parser->layouts[i];
    }
    return NULL;
}

static void infer_report(const hidpad_report_parser_t *parser, const uint8_t *data,
                         size_t len, uint8_t *report_id, uint16_t *base_bits)
{
    uint8_t i;
    *report_id = 0;
    *base_bits = 0;
    if (!parser->has_report_id || !data || len == 0) return;
    for (i = 0; i < parser->layout_count; ++i) {
        if (parser->layouts[i].report_id == data[0] && parser->layouts[i].total_bytes == len) {
            *report_id = data[0];
            *base_bits = 8;
            return;
        }
    }
    for (i = 0; i < parser->layout_count; ++i) {
        if (parser->layouts[i].report_id != 0 && parser->layouts[i].payload_bytes == len) {
            *report_id = parser->layouts[i].report_id;
            return;
        }
    }
}

static void infer_report_q36_legacy(const hidpad_report_parser_t *parser,
                                    const uint8_t *data, size_t len,
                                    uint8_t *report_id, uint16_t *base_bits)
{
    uint8_t i;
    uint8_t best_id = 0;
    int best_score = -1;
    *report_id = 0;
    *base_bits = 0;
    if (!parser->has_report_id || !data || len == 0) return;
    for (i = 0; i < parser->layout_count; ++i) {
        if (parser->layouts[i].report_id != 0 &&
            parser->layouts[i].report_id == data[0]) {
            *report_id = data[0];
            *base_bits = 8;
            return;
        }
    }
    for (i = 0; i < parser->layout_count; ++i) {
        const hidpad_report_layout_t *layout = &parser->layouts[i];
        int score = -1;
        if (layout->report_id == 0) continue;
        if (len == layout->payload_bytes) score = 100;
        else if (len == layout->total_bytes) score = 90;
        else if (len > layout->payload_bytes && len <= (size_t)layout->total_bytes + 1u) score = 60;
        else if (len >= layout->payload_bytes) score = 20;
        if (score > best_score) {
            best_score = score;
            best_id = layout->report_id;
        }
    }
    *report_id = best_id;
}

int hidpad_parser_decode(const hidpad_report_parser_t *parser,
                         uint8_t report_id,
                         const uint8_t *data,
                         size_t len,
                         hidpad_profile_t profile,
                         hidpad_decoded_report_t *out)
{
    uint16_t i;
    uint8_t selected_id = report_id;
    uint16_t base_bits = 0;
    uint16_t first_field = 0;
    uint16_t field_end;
    int matched = 0;
    int has_rx = 0;
    int has_ry = 0;
    const hidpad_report_layout_t *layout;
    if (!parser || !data || len == 0 || !out) return 0;
    zero_bytes(out, sizeof(*out));
    if (selected_id == 0) {
        if (profile == HIDPAD_PROFILE_Q36) {
            infer_report_q36_legacy(parser, data, len, &selected_id, &base_bits);
        } else {
            infer_report(parser, data, len, &selected_id, &base_bits);
        }
    }
    out->report_id = selected_id;
    layout = find_layout(parser, selected_id);
    /* HOGP normally identifies the report through its characteristic and
     * omits the Report ID byte. Some otherwise standard controllers include
     * it anyway. Accept both forms by checking the descriptor-derived length
     * and the leading ID, even when Report Reference already selected it. */
    if (selected_id != 0 && parser->has_report_id && layout &&
        len == layout->total_bytes && data[0] == selected_id) {
        base_bits = 8;
    }
    if (layout && layout->field_count > 0) {
        first_field = layout->first_field;
        field_end = (uint16_t)(layout->first_field + layout->field_count);
        has_rx = layout->has_rx;
        has_ry = layout->has_ry;
    } else {
        field_end = parser->field_count;
    }
    for (i = first_field; i < field_end; ++i) {
        const hidpad_report_field_t *field = &parser->fields[i];
        int32_t raw;
        if (field->report_id != 0 && field->report_id != selected_id) continue;
        if (!read_bits(data, len, (uint16_t)(base_bits + field->offset_bits),
                       field->size_bits, field->logical_min < 0, &raw)) continue;
        matched = 1;
        if (field->usage_page == USAGE_PAGE_BUTTON) {
            out->valid_mask |= HIDPAD_VALID_GAME_BUTTONS;
            apply_button(out, field->variable ? field->usage : (uint16_t)raw,
                         raw != 0, profile);
        } else if (field->usage_page == USAGE_PAGE_CONSUMER) {
            out->valid_mask |= HIDPAD_VALID_CONSUMER_BUTTONS;
            apply_consumer(out, field->variable ? field->usage : (uint16_t)raw, raw != 0);
        } else if (field->usage_page == USAGE_PAGE_SIMULATION) {
            /* Match Linux hid-input: Accelerator is ABS_GAS and Brake is
             * ABS_BRAKE. Gamepads conventionally expose them as RT and LT. */
            if (field->usage == USAGE_ACCELERATOR) {
                out->valid_mask |= HIDPAD_VALID_RT;
                out->rt = normalize_trigger(raw, field->logical_min, field->logical_max);
            } else if (field->usage == USAGE_BRAKE) {
                out->valid_mask |= HIDPAD_VALID_LT;
                out->lt = normalize_trigger(raw, field->logical_min, field->logical_max);
            }
        } else if (field->usage_page == USAGE_PAGE_GENERIC_DESKTOP) {
            switch (field->usage) {
            case USAGE_X: out->valid_mask |= HIDPAD_VALID_LX; out->lx = normalize_axis(raw, field->logical_min, field->logical_max, 0); break;
            case USAGE_Y: out->valid_mask |= HIDPAD_VALID_LY; out->ly = normalize_axis(raw, field->logical_min, field->logical_max, 1); break;
            case USAGE_Z:
                if (has_rx) { out->valid_mask |= HIDPAD_VALID_LT; out->lt = normalize_trigger(raw, field->logical_min, field->logical_max); }
                else { out->valid_mask |= HIDPAD_VALID_RX; out->rx = normalize_axis(raw, field->logical_min, field->logical_max, 0); }
                break;
            case USAGE_RX: out->valid_mask |= HIDPAD_VALID_RX; out->rx = normalize_axis(raw, field->logical_min, field->logical_max, 0); break;
            case USAGE_RY: out->valid_mask |= HIDPAD_VALID_RY; out->ry = normalize_axis(raw, field->logical_min, field->logical_max, 1); break;
            case USAGE_RZ:
                if (has_ry) { out->valid_mask |= HIDPAD_VALID_RT; out->rt = normalize_trigger(raw, field->logical_min, field->logical_max); }
                else { out->valid_mask |= HIDPAD_VALID_RY; out->ry = normalize_axis(raw, field->logical_min, field->logical_max, 1); }
                break;
            case USAGE_HAT: out->valid_mask |= HIDPAD_VALID_GAME_BUTTONS; apply_hat(out, raw, field->logical_min, field->logical_max); break;
            case USAGE_DPAD_UP: out->valid_mask |= HIDPAD_VALID_GAME_BUTTONS; if (raw) out->buttons |= BTN_UP; break;
            case USAGE_DPAD_DOWN: out->valid_mask |= HIDPAD_VALID_GAME_BUTTONS; if (raw) out->buttons |= BTN_DOWN; break;
            case USAGE_DPAD_RIGHT: out->valid_mask |= HIDPAD_VALID_GAME_BUTTONS; if (raw) out->buttons |= BTN_RIGHT; break;
            case USAGE_DPAD_LEFT: out->valid_mask |= HIDPAD_VALID_GAME_BUTTONS; if (raw) out->buttons |= BTN_LEFT; break;
            default: break;
            }
        }
    }
    return matched;
}
