#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIDPAD_MAX_REPORT_FIELDS 96
#define HIDPAD_MAX_REPORT_LAYOUTS 12

typedef enum hidpad_profile_t {
    HIDPAD_PROFILE_GENERIC = 0,
    HIDPAD_PROFILE_Q36 = 1,
} hidpad_profile_t;

typedef struct hidpad_decoded_report_t {
    uint32_t buttons;
    uint32_t raw_buttons;
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;
    uint16_t lt;
    uint16_t rt;
    uint8_t report_id;
} hidpad_decoded_report_t;

typedef struct hidpad_report_field_t {
    uint8_t report_id;
    uint16_t offset_bits;
    uint8_t size_bits;
    uint16_t usage_page;
    uint16_t usage;
    uint16_t usage_min;
    uint16_t usage_max;
    int32_t logical_min;
    int32_t logical_max;
    uint8_t variable;
} hidpad_report_field_t;

typedef struct hidpad_report_layout_t {
    uint8_t report_id;
    uint16_t bits;
    uint16_t payload_bytes;
    uint16_t total_bytes;
} hidpad_report_layout_t;

typedef struct hidpad_report_parser_t {
    hidpad_report_field_t fields[HIDPAD_MAX_REPORT_FIELDS];
    hidpad_report_layout_t layouts[HIDPAD_MAX_REPORT_LAYOUTS];
    uint16_t field_count;
    uint8_t layout_count;
    uint8_t has_report_id;
} hidpad_report_parser_t;

/** 清空已解析的 HID Report Map。 */
void hidpad_parser_clear(hidpad_report_parser_t *parser);

/** 解析 HID Report Map，只保留手柄输入需要的固定字段。 */
int hidpad_parser_parse(hidpad_report_parser_t *parser, const uint8_t *data, size_t len);

/** 解码一帧 HID Input Report；report_id 为 0 时按报文自行推断。 */
int hidpad_parser_decode(const hidpad_report_parser_t *parser,
                         uint8_t report_id,
                         const uint8_t *data,
                         size_t len,
                         hidpad_profile_t profile,
                         hidpad_decoded_report_t *out);

#ifdef __cplusplus
}
#endif
