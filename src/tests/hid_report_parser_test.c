#include "hid_report_parser.h"

#include <stdint.h>
#include <stdio.h>

#define BTN_A (1u << 4)
#define BTN_B (1u << 5)
#define BTN_X (1u << 6)
#define BTN_Y (1u << 7)
#define BTN_LB (1u << 8)
#define BTN_RB (1u << 9)
#define BTN_VIEW (1u << 12)
#define BTN_MENU (1u << 13)
#define BTN_UP (1u << 0)
#define BTN_LEFT (1u << 2)

static int expect(int condition, const char *message)
{
    if (condition) return 1;
    fprintf(stderr, "FAIL: %s\n", message);
    return 0;
}

static int test_unsigned_logical_max_and_report_id(void)
{
    static const uint8_t report_map[] = {
        0x05, 0x01,       /* Usage Page (Generic Desktop) */
        0x09, 0x05,       /* Usage (Game Pad) */
        0xa1, 0x01,       /* Collection (Application) */
        0x85, 0x01,       /* Report ID 1 */
        0x09, 0x30, 0x09, 0x31, /* X, Y */
        0x15, 0x00, 0x25, 0xff, /* Logical 0..255 */
        0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x0c, /* Buttons 1..12 */
        0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x0c, 0x81, 0x02,
        0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
        0xc0
    };
    static const uint8_t characteristic_payload[] = {0x00, 0xff, 0x05, 0x00};
    static const uint8_t embedded_id_payload[] = {0x01, 0x00, 0xff, 0x05, 0x00};
    hidpad_report_parser_t parser;
    hidpad_decoded_report_t decoded;
    int ok = 1;

    ok &= expect(hidpad_parser_parse(&parser, report_map, sizeof(report_map)),
                 "standard gamepad report map parses");
    ok &= expect(hidpad_parser_decode(&parser, 1, characteristic_payload,
                                      sizeof(characteristic_payload),
                                      HIDPAD_PROFILE_GENERIC, &decoded),
                 "Report Reference ID decodes payload without embedded ID");
    ok &= expect(decoded.lx == -32767 && decoded.ly == -32767,
                 "0..255 axes normalize across their full range");
    ok &= expect((decoded.buttons & (BTN_A | BTN_X)) == (BTN_A | BTN_X),
                 "common Android buttons 1 and 3 map to A and X");
    ok &= expect(hidpad_parser_decode(&parser, 0, embedded_id_payload,
                                      sizeof(embedded_id_payload),
                                      HIDPAD_PROFILE_GENERIC, &decoded),
                 "embedded Report ID is inferred from packet length");
    return ok;
}

static int test_output_usage_does_not_leak_into_input(void)
{
    static const uint8_t report_map[] = {
        0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
        0x05, 0x09, 0x09, 0x01,
        0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x01, 0x91, 0x02, /* Output (Button 1) */
        0x05, 0x01, 0x09, 0x30,
        0x15, 0x00, 0x25, 0xff,
        0x75, 0x08, 0x95, 0x01, 0x81, 0x02, /* Input (X) */
        0xc0
    };
    static const uint8_t payload[] = {0x80};
    hidpad_report_parser_t parser;
    hidpad_decoded_report_t decoded;
    int ok = 1;

    ok &= expect(hidpad_parser_parse(&parser, report_map, sizeof(report_map)),
                 "map with output followed by input parses");
    ok &= expect(hidpad_parser_decode(&parser, 0, payload, sizeof(payload),
                                      HIDPAD_PROFILE_GENERIC, &decoded),
                 "input after output decodes");
    ok &= expect((decoded.valid_mask & HIDPAD_VALID_LX) != 0,
                 "output Usage does not contaminate following X input");
    return ok;
}

static int test_digitizer_axes_are_not_gamepad_sticks(void)
{
    static const uint8_t report_map[] = {
        0x05, 0x0d, 0x09, 0x04, 0xa1, 0x01, /* Digitizer Touch Screen */
        0x09, 0x22, 0xa1, 0x02,             /* Finger */
        0x09, 0x42, 0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x01, 0x81, 0x02,
        0x75, 0x07, 0x95, 0x01, 0x81, 0x03,
        0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
        0x16, 0x00, 0x00, 0x26, 0xff, 0x0f,
        0x75, 0x10, 0x95, 0x02, 0x81, 0x02,
        0xc0, 0xc0
    };
    hidpad_report_parser_t parser;
    return expect(!hidpad_parser_parse(&parser, report_map, sizeof(report_map)),
                  "digitizer X/Y fields are not accepted as gamepad sticks");
}

static int test_explicit_report_reference_with_optional_embedded_id(void)
{
    static const uint8_t report_map[] = {
        0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
        0x85, 0x03,
        0x75, 0x08, 0x95, 0x01, 0x81, 0x01, /* one constant byte */
        0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
        0x75, 0x08, 0x95, 0x01, 0x81, 0x42, /* hat */
        0x05, 0x09, 0x19, 0x01, 0x29, 0x08,
        0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
        0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
        0x15, 0x00, 0x25, 0xff,
        0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
        0xc0
    };
    static const uint8_t payload[] = {0x00, 0x08, 0x01, 0x80, 0x80};
    static const uint8_t embedded_id_payload[] = {0x03, 0x00, 0x08, 0x01, 0x80, 0x80};
    hidpad_report_parser_t parser;
    hidpad_decoded_report_t decoded;
    int ok = 1;

    ok &= expect(hidpad_parser_parse(&parser, report_map, sizeof(report_map)),
                 "Report ID 3 gamepad map parses");
    ok &= expect(hidpad_parser_decode(&parser, 3, payload, sizeof(payload),
                                      HIDPAD_PROFILE_GENERIC, &decoded),
                 "Report Reference payload without embedded ID decodes");
    ok &= expect((decoded.buttons & BTN_A) != 0 && decoded.lx == 128,
                 "non-embedded Report ID keeps field alignment");
    ok &= expect(hidpad_parser_decode(&parser, 3, embedded_id_payload,
                                      sizeof(embedded_id_payload),
                                      HIDPAD_PROFILE_GENERIC, &decoded),
                 "Report Reference payload with embedded ID decodes");
    ok &= expect((decoded.buttons & BTN_A) != 0 && decoded.lx == 128,
                 "embedded Report ID is skipped without shifting controls");
    return ok;
}

static int test_gamepad_fields_after_first_att_chunk(void)
{
    static const uint8_t vendor_feature[] = {
        0x06, 0x00, 0xff, /* Usage Page (Vendor 0xff00) */
        0x75, 0x08, 0x95, 0x01,
        0x09, 0x01, 0xb1, 0x02 /* one-byte Feature */
    };
    static const uint8_t gamepad_tail[] = {
        0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
        0x85, 0x07,
        0x09, 0x30, 0x09, 0x31,
        0x15, 0x00, 0x25, 0xff,
        0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x08,
        0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
        0xc0
    };
    uint8_t report_map[512];
    static const uint8_t payload[] = {0x80, 0x80, 0x01};
    hidpad_report_parser_t parser;
    hidpad_decoded_report_t decoded;
    size_t len = 0;
    size_t i;
    size_t j;
    int ok = 1;

    for (i = 0; i < 23; ++i) {
        for (j = 0; j < sizeof(vendor_feature); ++j) report_map[len++] = vendor_feature[j];
    }
    for (i = 0; i < sizeof(gamepad_tail); ++i) report_map[len++] = gamepad_tail[i];

    ok &= expect(!hidpad_parser_parse(&parser, report_map, 244),
                 "first 244-byte ATT chunk has no gamepad fields");
    ok &= expect(hidpad_parser_parse(&parser, report_map, len),
                 "accumulated long Report Map parses fields after byte 244");
    ok &= expect(hidpad_parser_decode(&parser, 7, payload, sizeof(payload),
                                      HIDPAD_PROFILE_GENERIC, &decoded),
                 "input described after the first ATT chunk decodes");
    ok &= expect((decoded.buttons & BTN_A) != 0,
                 "long Report Map preserves common button usages");
    return ok;
}

static int test_btp_kp20d_standard_report(void)
{
    static const uint8_t report_map[] = {
        0x05,0x01,0x09,0x05,0xa1,0x01,0x85,0x03,0x75,0x01,0x95,0x08,0x81,0x01,
        0x05,0x01,0x75,0x08,0x95,0x01,0x15,0x00,0x25,0x07,0x35,0x00,0x46,0x3b,
        0x01,0x65,0x14,0x09,0x39,0x81,0x42,0x65,0x00,0x05,0x09,0x25,0x01,0x19,
        0x01,0x29,0x0f,0x95,0x0f,0x75,0x01,0x81,0x02,0x05,0x0c,0x09,0x69,0x75,
        0x01,0x95,0x01,0x81,0x02,0x05,0x01,0x75,0x08,0x95,0x02,0x15,0x00,0x26,
        0xff,0x00,0x35,0x00,0x46,0xff,0x00,0xa1,0x00,0x09,0x30,0x09,0x31,0x81,
        0x02,0xc0,0xa1,0x00,0x09,0x32,0x09,0x35,0x81,0x02,0xc0,0x05,0x02,0x09,
        0xc5,0x09,0xc4,0x75,0x08,0x95,0x02,0x15,0x00,0x25,0xff,0x35,0x00,0x45,
        0xff,0x81,0x02,0xc0
    };
    static const uint8_t payload[] = {
        0x00, 0x0f, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80, 0xff, 0x40
    };
    static const uint8_t labeled_buttons[] = {
        0x00, 0x0f, 0xd8, 0x0c, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00
    };
    hidpad_report_parser_t parser;
    hidpad_decoded_report_t decoded;
    int ok = 1;

    ok &= expect(hidpad_parser_parse(&parser, report_map, sizeof(report_map)),
                 "BTP-KP20D 116-byte standard Game Pad map parses");
    ok &= expect(hidpad_parser_decode(&parser, 3, payload, sizeof(payload),
                                      HIDPAD_PROFILE_Q36, &decoded),
                 "BTP-KP20D Report Reference payload decodes without embedded ID");
    ok &= expect((decoded.buttons & BTN_A) != 0,
                 "BTP-KP20D standard Button 1 maps to A");
    ok &= expect(decoded.lx == 128 && decoded.ly == -128 &&
                 decoded.rx == 128 && decoded.ry == -128,
                 "BTP-KP20D four standard axes are centered");
    ok &= expect(decoded.lt == 65535 && decoded.rt == 16448,
                 "Simulation Brake/Accelerator map to LT/RT");
    ok &= expect(hidpad_parser_decode(&parser, 3, labeled_buttons,
                                      sizeof(labeled_buttons),
                                      HIDPAD_PROFILE_Q36, &decoded),
                 "BTP-KP20D labeled buttons decode with Q36-style usages");
    ok &= expect((decoded.buttons & (BTN_X | BTN_Y | BTN_LB | BTN_RB |
                                     BTN_VIEW | BTN_MENU)) ==
                 (BTN_X | BTN_Y | BTN_LB | BTN_RB | BTN_VIEW | BTN_MENU),
                 "BTP-KP20D Usage 4/5/7/8/11/12 maps to its physical labels");
    return ok;
}

static int test_independent_dpad_usages(void)
{
    static const uint8_t report_map[] = {
        0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
        0x09, 0x90, 0x09, 0x91, 0x09, 0x92, 0x09, 0x93,
        0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x04, 0x81, 0x02,
        0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
        0xc0
    };
    static const uint8_t payload[] = {0x09};
    hidpad_report_parser_t parser;
    hidpad_decoded_report_t decoded;
    int ok = 1;

    ok &= expect(hidpad_parser_parse(&parser, report_map, sizeof(report_map)),
                 "independent D-pad Usage map parses");
    ok &= expect(hidpad_parser_decode(&parser, 0, payload, sizeof(payload),
                                      HIDPAD_PROFILE_GENERIC, &decoded),
                 "independent D-pad Usage payload decodes");
    ok &= expect((decoded.buttons & (BTN_UP | BTN_LEFT)) == (BTN_UP | BTN_LEFT),
                 "D-pad Up and Left usages map to directions");
    return ok;
}

int main(void)
{
    int ok = test_unsigned_logical_max_and_report_id();
    ok &= test_output_usage_does_not_leak_into_input();
    ok &= test_digitizer_axes_are_not_gamepad_sticks();
    ok &= test_explicit_report_reference_with_optional_embedded_id();
    ok &= test_gamepad_fields_after_first_att_chunk();
    ok &= test_btp_kp20d_standard_report();
    ok &= test_independent_dpad_usages();
    if (!ok) return 1;
    puts("hid_report_parser tests passed");
    return 0;
}
