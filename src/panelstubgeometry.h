/*
    SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <QRect>
#include <QRegion>
#include <QSize>

// KWin reports the bounding rectangle as input-panel geometry but hit-tests
// the exact region. Never use an empty mask (Qt means unrestricted input), nor
// a filled transparent rectangle (it would swallow touches intended for keys).
inline QRegion panelStubInputRegion(const QRect &panel, const QSize &surfaceSize, bool knownLocked, bool docked)
{
    const QRegion fallback(0, 0, 1, 1);
    if (!knownLocked || !docked || !panel.isValid() || !QRect(QPoint(), surfaceSize).contains(panel)) {
        return fallback;
    }
    // QRect::bottomRight is inclusive: both logical pixels remain inside the
    // non-key border. At worst these two tiny corner areas consume a touch.
    return QRegion(QRect(panel.topLeft(), QSize(1, 1))) | QRegion(QRect(panel.bottomRight(), QSize(1, 1)));
}
