// SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components

import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard
import org.uke.plasma.keyboard.lib as PlasmaKeyboard

/**
 * Key that switches the keyboard between the docked panel at the bottom of the
 * screen and the floating panel that can be moved around and whose position is
 * remembered.
 *
 * The key panel shows the floating mode as latched while it is on.
 */
BaseKey {
    id: root

    functionKey: true
    highlighted: true
    keyPanelDelegate: keyboard.style && keyboard.style.floatingKeyPanel ? keyboard.style.floatingKeyPanel : fallbackPanel

    //! Whether the floating mode is on; the key panel uses it for its colour.
    readonly property bool latched: PlasmaKeyboardSettings.floatingKeyboard

    onClicked: {
        PlasmaKeyboardSettings.floatingKeyboard = !PlasmaKeyboardSettings.floatingKeyboard;
        PlasmaKeyboardSettings.save();
    }

    //! The style in use may not know the floating key panel (for example the
    //! style installed system-wide while a local build is being tested), so the
    //! key draws itself in that case.
    Component {
        id: fallbackPanel

        PlasmaKeyboard.BreezeKeyPanel {
            Item {
                Kirigami.Icon {
                    anchors.centerIn: parent
                    implicitHeight: 96 * PlasmaKeyboard.BreezeConstants.keyIconScale
                    color: PlasmaKeyboard.BreezeConstants.keyTextColor
                    source: PlasmaKeyboard.BreezeConstants.icon("object-move-symbolic")
                }
            }
        }
    }
}
