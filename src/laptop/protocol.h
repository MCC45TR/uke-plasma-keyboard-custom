// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifndef UKE_KWIN_PLUGIN_ID
#define UKE_KWIN_PLUGIN_ID "uke_keyboard_backend"
#endif
namespace UkeKeyboard {
inline constexpr auto pluginId = UKE_KWIN_PLUGIN_ID;
inline constexpr auto service = "org.uke.PlasmaKeyboard.Backend";
inline constexpr auto path = "/org/uke/PlasmaKeyboard/Backend";
inline constexpr auto interface = "org.uke.PlasmaKeyboard.Backend1";
inline constexpr int protocolVersion = 1;
}
