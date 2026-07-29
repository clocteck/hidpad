# BTP-KP20D Lua BLE probe

This service isolates the controller and firmware BLE transport from
`hidpad.so`. It connects to `40:e4:02:16:a4:65`, pairs, and subscribes the known
Report ID 3 CCCD at handle 27. It then discovers only the BTP `0x7310` vendor
service, subscribes `0x7311`/`0x7313`, reads `0x7311`, and sends the official
one-shot KP20D sequence (`21 00 00`, response-triggered `11 00 20`, `15`, and
`55`) through `0x7312`. HID discovery, Protocol Mode writes, input polling,
`hidpad.so`, and connection-parameter updates remain absent.

The service acquires a 320x240 LVGL service canvas and shows a fixed, compact
diagnostic screen:

- connection phase, including a distinct `STALLED` state;
- currently pressed D-pad and common buttons;
- raw stick and trigger bytes;
- notification count, latest notification gap, and raw Report ID 3 payload.

L3 and R3 are intentionally ignored. Short button presses are held on screen
for 700 ms, while rendering is coalesced by a 150 ms timer, so BLE callbacks
only update state and are never blocked by LVGL drawing.

The controller preserves its bonded CCCD value across reconnects. To test
whether a repeated `01 00` is failing to re-arm its Report ID 3 stream, this
probe explicitly writes `00 00` and waits for the response before writing
`01 00` on every connection.

Runtime state is available at:

```text
GET /btp-probe/api/state
```

Version 0.6 returns to the one-shot vendor session. After the first ID3 stream
has been silent for two seconds, it performs one in-place `00 00` -> `01 00`
CCCD re-arm without disconnecting. This distinguishes an expired notification
subscription from a dead BLE link or a reconnect-only controller state.
