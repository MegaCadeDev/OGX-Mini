#include <cstring>

#include "host/usbh.h"

#include "USBHost/HostDriver/XInput/tuh_xinput/tuh_xinput.h"
#include "USBHost/HostDriver/XInput/XboxOne.h"

void XboxOneHost::initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len)
{
    tuh_xinput::receive_report(address, instance);
}

void XboxOneHost::process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len)
{
    const XboxOne::InReport* in_report = reinterpret_cast<const XboxOne::InReport*>(report);
    if (std::memcmp(&prev_in_report_ + 4, in_report + 4, 14) == 0)
    {
        tuh_xinput::receive_report(address, instance);
        return;
    }

    Gamepad::PadIn gp_in;

    const uint16_t b = in_report->buttons;
    if (b & XboxOne::GamepadButtons::DPAD_UP)    gp_in.dpad |= gamepad.MAP_DPAD_UP;
    if (b & XboxOne::GamepadButtons::DPAD_DOWN)  gp_in.dpad |= gamepad.MAP_DPAD_DOWN;
    if (b & XboxOne::GamepadButtons::DPAD_LEFT)  gp_in.dpad |= gamepad.MAP_DPAD_LEFT;
    if (b & XboxOne::GamepadButtons::DPAD_RIGHT) gp_in.dpad |= gamepad.MAP_DPAD_RIGHT;

    if (b & XboxOne::GamepadButtons::LEFT_THUMB)  gp_in.buttons |= gamepad.MAP_BUTTON_L3;
    if (b & XboxOne::GamepadButtons::RIGHT_THUMB) gp_in.buttons |= gamepad.MAP_BUTTON_R3;
    if (b & XboxOne::GamepadButtons::LEFT_SHOULDER)  gp_in.buttons |= gamepad.MAP_BUTTON_LB;
    if (b & XboxOne::GamepadButtons::RIGHT_SHOULDER) gp_in.buttons |= gamepad.MAP_BUTTON_RB;
    if (b & XboxOne::GamepadButtons::VIEW)  gp_in.buttons |= gamepad.MAP_BUTTON_BACK;
    if (b & XboxOne::GamepadButtons::MENU) gp_in.buttons |= gamepad.MAP_BUTTON_START;
    if (b & XboxOne::GamepadButtons::SYNC)  gp_in.buttons |= gamepad.MAP_BUTTON_MISC;
    if (in_report->guide_pressed) gp_in.buttons |= gamepad.MAP_BUTTON_SYS;
    if (b & XboxOne::GamepadButtons::A)     gp_in.buttons |= gamepad.MAP_BUTTON_A;
    if (b & XboxOne::GamepadButtons::B)     gp_in.buttons |= gamepad.MAP_BUTTON_B;
    if (b & XboxOne::GamepadButtons::X)     gp_in.buttons |= gamepad.MAP_BUTTON_X;
    if (b & XboxOne::GamepadButtons::Y)     gp_in.buttons |= gamepad.MAP_BUTTON_Y;

    gp_in.trigger_l = gamepad.scale_trigger_l(static_cast<uint8_t>(in_report->trigger_l >> 2));
    gp_in.trigger_r = gamepad.scale_trigger_r(static_cast<uint8_t>(in_report->trigger_r >> 2));

    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) = gamepad.scale_joystick_l(in_report->joystick_lx, in_report->joystick_ly, true);
    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) = gamepad.scale_joystick_r(in_report->joystick_rx, in_report->joystick_ry, true);

    gamepad.set_pad_in(gp_in);

    tuh_xinput::receive_report(address, instance);
    std::memcpy(&prev_in_report_, in_report, 18);
}

bool XboxOneHost::send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    Gamepad::PadOut gp_out = gamepad.get_pad_out();
    return tuh_xinput::set_rumble(address, instance, gp_out.rumble_l, gp_out.rumble_r, false);
}