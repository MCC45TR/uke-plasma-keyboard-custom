// SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components

/**
 * A momentary Ctrl/Alt/Meta modifier key.
 *
 * InputListenerItem follows physical touch ownership so a second finger can
 * type a chord without releasing the modifier. Lifting the finger releases it.
 */
Key {
    id: root

    /** Which modifier this key controls: "ctrl", "alt" or "meta". */
    property string modifier: "ctrl"

    /** Historical property name; true only while the modifier is held. */
    readonly property bool latched: {
        if (root.modifier === "alt") return Modifiers.alt;
        if (root.modifier === "meta") return Modifiers.meta;
        return Modifiers.ctrl;
    }

    key: {
        if (root.modifier === "alt") return Qt.Key_Alt;
        if (root.modifier === "meta") return Qt.Key_Meta;
        return Qt.Key_Control;
    }
    functionKey: true
    noModifier: true
}
