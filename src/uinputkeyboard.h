/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <QObject>

/** Compatibility facade for the inherited shortcut call sites.
 * Events use the selected input method's authenticated native KWin device.
 * There is no kernel input-device file or timer-driven event queue.
 */
class UinputKeyboard : public QObject
{
    Q_OBJECT
public:
    explicit UinputKeyboard(QObject *parent = nullptr);
    ~UinputKeyboard() override;
    UinputKeyboard(const UinputKeyboard &) = delete;
    UinputKeyboard &operator=(const UinputKeyboard &) = delete;
    bool isValid() const;
    void pressKey(int keyCode);
    void releaseKey(int keyCode);
};
