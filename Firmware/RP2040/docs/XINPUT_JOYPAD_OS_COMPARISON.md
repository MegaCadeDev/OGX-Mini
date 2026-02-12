# XInput (Xbox 360) Report & Descriptor Comparison: OGX-Mini vs joypad-os

This document compares the XInput device implementation in this project (OGX-Mini) with [joypad-os](https://github.com/joypad-ai/joypad-os) so we can align behavior and add **real Xbox 360 console** support.

---

## 1. Input report (20 bytes) — byte-by-byte

Both projects use the **same 20-byte input report layout**. Field names differ; byte positions and semantics match.

| Byte | OGX-Mini (`XInput::InReport`) | joypad-os (`xinput_in_report_t`) | Notes |
|------|------------------------------|-----------------------------------|--------|
| 0 | `report_id` | `report_id` | Always `0x00` |
| 1 | `report_size` | `report_size` | Always `0x14` (20) |
| 2 | `buttons[0]` | `buttons0` | D-pad (0–3), Start (4), Back (5), L3 (6), R3 (7) |
| 3 | `buttons[1]` | `buttons1` | LB (0), RB (1), Guide (2), — (3), A (4), B (5), X (6), Y (7) |
| 4 | `trigger_l` | `trigger_l` | 0–255 |
| 5 | `trigger_r` | `trigger_r` | 0–255 |
| 6–7 | `joystick_lx` | `stick_lx` | int16 LE (-32768..32767) |
| 8–9 | `joystick_ly` | `stick_ly` | int16 LE (Y inverted: positive = up) |
| 10–11 | `joystick_rx` | `stick_rx` | int16 LE |
| 12–13 | `joystick_ry` | `stick_ry` | int16 LE (Y inverted) |
| 14–19 | `reserved[6]` | `reserved[6]` | Zero padding |

**Conclusion:** No change needed for the **input report format**. OGX-Mini’s mapping in `XInput.cpp` already matches joypad-os’s `xinput_mode.c` (same button bits and axis handling, including Y inversion).

---

## 2. Button bit definitions — identical

| Button | Byte | Bit | OGX-Mini | joypad-os |
|--------|------|-----|----------|-----------|
| D-pad U/D/L/R | 2 | 0–3 | `Buttons0::DPAD_*` | `XINPUT_BTN_DPAD_*` |
| Start | 2 | 4 | `Buttons0::START` | `XINPUT_BTN_START` |
| Back | 2 | 5 | `Buttons0::BACK` | `XINPUT_BTN_BACK` |
| L3 / R3 | 2 | 6–7 | `Buttons0::L3/R3` | `XINPUT_BTN_L3/R3` |
| LB / RB | 3 | 0–1 | `Buttons1::LB/RB` | `XINPUT_BTN_LB/RB` |
| Guide | 3 | 2 | `Buttons1::HOME` | `XINPUT_BTN_GUIDE` |
| A / B / X / Y | 3 | 4–7 | `Buttons1::A/B/X/Y` | `XINPUT_BTN_A/B/X/Y` |

No code changes required for button mapping.

---

## 3. Output report (8 bytes) — identical

| Byte | Content | OGX-Mini | joypad-os |
|------|---------|----------|-----------|
| 0 | report_id | RUMBLE=0x00, LED=0x01 | Same |
| 1 | report_size | 0x08 | 0x08 |
| 2 | led | LED pattern | led |
| 3 | rumble_l | rumble_l | rumble_l |
| 4 | rumble_r | rumble_r | rumble_r |
| 5–7 | reserved | 0 | reserved[3] |

No change needed for output report layout.

---

## 4. Device descriptor — same VID/PID, minor bmAttributes

| Field | OGX-Mini | joypad-os |
|-------|----------|-----------|
| idVendor | 0x045E | XINPUT_VID 0x045E |
| idProduct | 0x028E | XINPUT_PID 0x028E |
| bcdDevice | 0x0114 (2.14) | XINPUT_BCD_DEVICE 0x0114 |
| bmAttributes | 0x80 (no remote wakeup) | 0xA0 (remote wakeup) |

Optional: set bmAttributes to `0xA0` in `DESC_DEVICE[]` if you want remote wakeup to match joypad-os and some 360 behavior.

---

## 5. Configuration descriptor — main difference (PC vs 360 console)

### OGX-Mini (current): PC-only, 1 interface

- **wTotalLength:** 48 (0x30)
- **bNumInterfaces:** 1
- **Interface 0:** Gamepad (0xFF / 0x5D / 0x01), 2 endpoints
  - EP 0x81 IN (interrupt, 32 bytes, **bInterval 1**)
  - EP 0x01 OUT (interrupt, 32 bytes, bInterval 8)
- Followed by a 16-byte vendor/HID-style descriptor (0x21 with bNumDescriptors, etc.).

This is enough for **PC (XInput)** and many games. It is **not** enough for a **real Xbox 360 console**, which expects a multi-interface device and XSM3 security.

### joypad-os: PC + Xbox 360 console, 4 interfaces

- **wTotalLength:** 153
- **bNumInterfaces:** 4

| Iface | Class / SubClass / Protocol | Purpose | Endpoints |
|-------|-----------------------------|---------|-----------|
| 0 | 0xFF / 0x5D / 0x01 | Gamepad | EP 0x81 IN (4 ms), EP 0x02 OUT (8 ms) |
| 1 | 0xFF / 0x5D / 0x03 | Audio (stub) | 4 EPs (not opened by driver) |
| 2 | 0xFF / 0x5D / 0x02 | Plugin (stub) | 1 EP (not opened) |
| 3 | 0xFF / **0xFD** / **0x13** | **Security (XSM3)** | 0 EPs (control only) |

- Gamepad uses a **17-byte vendor descriptor** (0x21) after the interface, not the longer HID-style block.
- **Interface 3** is the security interface; the console uses **vendor control requests** on the device to run the XSM3 challenge/response. Without this interface and the XSM3 handler, the 360 console will not accept the device.

---

## 6. What to add for real Xbox 360 console support

To have OGX-Mini work on a **real Xbox 360** (like joypad-os), you need the following.

### 6.1 Four-interface configuration descriptor

- Replace the single-interface configuration in `Descriptors/XInput.h` with a **153-byte** configuration that includes:
  - Interface 0: Gamepad (0xFF/0x5D/0x01) + 17-byte vendor descriptor + EP 0x81 IN (bInterval **4**), EP 0x02 OUT (8).
  - Interface 1: Audio (0xFF/0x5D/0x03), stub only (descriptors present, no need to open EPs).
  - Interface 2: Plugin (0xFF/0x5D/0x02), stub only.
  - Interface 3: Security (0xFF/0xFD/0x13), no endpoints, security descriptor (0x41) only.

You can copy the exact bytes from joypad-os’s `xinput_descriptors.h` (`xinput_config_descriptor[]`) and adapt to OGX-Mini’s naming (e.g. `DESC_CONFIGURATION[]` / `XINPUT_CONFIG_TOTAL_LEN`).

### 6.2 XSM3 authentication

- The 360 console sends **vendor control** requests (bRequest):
  - **0x81** GET_SERIAL — device returns 29-byte ID (from libxsm3).
  - **0x82** INIT_AUTH — console sends 34-byte challenge; device must respond with 46-byte response.
  - **0x83** RESPOND — device returns challenge response (46 or 22 bytes).
  - **0x84** KEEPALIVE — device responds with 0 bytes.
  - **0x86** STATE — device returns 2-byte state (e.g. 2 = response ready).
  - **0x87** VERIFY — console sends 22-byte verify; device responds with 22-byte response.

- joypad-os uses **[libxsm3](https://github.com/InvoxiPlayGames/libxsm3)** (LGPL-2.1) to compute the challenge/response. You would need to:
  - Add libxsm3 (or a port) to the OGX-Mini firmware.
  - In the USB device stack, route **vendor control** transfers to an XInput/XSM3 handler (joypad-os does this in `tud_xinput_vendor_control_xfer_cb`).
  - Implement the same state machine: on 0x82 store the challenge and call libxsm3 init; on 0x83 return the response; on 0x87 run verify and return the verify response; etc.

### 6.3 Class driver `open()` for multiple interfaces

- joypad-os’s `tud_xinput.c` `xinput_open()` is called for **each** interface. It:
  - **Opens** only Interface 0 (gamepad): claims the interface and opens EP IN and EP OUT.
  - **Skips** Interfaces 1 and 2 (audio, plugin): advances the descriptor pointer without opening endpoints.
  - **Records** Interface 3 (security) index (e.g. `_sec_itf_num`) for XSM3; no endpoints.

OGX-Mini’s `tud_xinput` currently assumes a single interface. It needs to be extended so that when the device presents the 4-interface configuration, the driver parses and skips the stub interfaces and only opens the gamepad endpoints, and recognizes the security interface (for routing vendor requests in usbd, not necessarily by interface number in the driver).

### 6.4 Optional: IN endpoint interval

- joypad-os uses **bInterval 4** (4 ms) for the gamepad IN endpoint; OGX-Mini uses **1** (1 ms). For 360 compatibility, changing the gamepad IN endpoint to **4** in the new configuration descriptor is a good idea.

---

## 7. Summary table

| Item | OGX-Mini | joypad-os | Action for 360 |
|------|----------|-----------|----------------|
| Input report (20 bytes) | Same layout | Same | None |
| Button bits | Same | Same | None |
| Output report (8 bytes) | Same | Same | None |
| VID/PID | 045E:028E | 045E:028E | None |
| Config: num interfaces | 1 | 4 | Add 4-interface, 153-byte config |
| Config: gamepad EP IN interval | 1 | 4 | Use 4 in new config |
| Vendor control (XSM3) | Not implemented | 0x81/82/83/84/86/87 | Add libxsm3 + vendor handler |
| Class driver multi-interface | Single iface | Opens if 0, skips 1–2, records 3 | Extend open() for 4-iface config |

Implementing the 4-interface descriptor and XSM3 (with libxsm3) will align OGX-Mini with joypad-os’s XInput mode for **real Xbox 360 console** output while keeping the existing report code unchanged.
