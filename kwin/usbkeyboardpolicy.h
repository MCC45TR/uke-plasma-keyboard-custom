// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <core/inputdevice.h>
#include <QVariant>
#include <linux/input.h>

namespace UkeKeyboard {
inline bool isPhysicalUsbKeyboard(const KWin::InputDevice *device)
{
    if (!device || !device->isKeyboard() || device->busType() != BUS_USB) return false;
    // A real USB receiver also counts. Exclude uinput/UHID and the plugin's
    // own device; neither is evidence of a physically attached USB keyboard.
    const auto path = device->sysPath();
    if (!path.startsWith(QLatin1String("/sys/devices/")) ||
        path.startsWith(QLatin1String("/sys/devices/virtual/"))) return false;
    // Libinput's generic keyboard capability includes media buttons on mice.
    // Use the original KWin full-keyboard classification, independent of layout.
    return device->property("alphaNumericKeyboard").toBool();
}
}
