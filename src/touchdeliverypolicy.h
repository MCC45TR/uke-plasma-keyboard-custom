// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QtGlobal>

namespace UkeKeyboard {
inline void configureTouchDelivery()
{
    // Qt Quick normally holds touch updates until the next rendered frame.
    // A virtual touchpad must forward input even while its surface is idle.
    // This is process-local; no compositor or display settings are changed.
    qputenv("QML_NO_TOUCH_COMPRESSION", QByteArrayLiteral("1"));
}
}
