// SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Window

import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard
import org.uke.plasma.keyboard.lib as PlasmaKeyboard

/**
 * Microphone key that sits to the right of the space bar and opens the voice
 * input mode of the keyboard.
 *
 * The key is only there when the voice input is enabled in the settings: with
 * it disabled the key is invisible, so the layout looks exactly like before and
 * the key takes no room at all.
 */
BaseKey {
    id: root

    functionKey: true
    highlighted: true
    visible: PlasmaKeyboardSettings.sttEnabled && root.Window.window !== null && root.Window.window.auxiliaryUiAllowed === true
    keyPanelDelegate: fallbackPanel

    onClicked: {
        const window = root.Window.window;
        if (window && typeof window.openVoiceMode === "function") {
            window.openVoiceMode();
        }
    }

    //! The style in use does not know a microphone key panel, so the key draws
    //! itself.
    Component {
        id: fallbackPanel

        PlasmaKeyboard.BreezeKeyPanel {
            Item {
                Kirigami.Icon {
                    anchors.centerIn: parent
                    implicitHeight: 96 * PlasmaKeyboard.BreezeConstants.keyIconScale
                    color: PlasmaKeyboard.BreezeConstants.keyTextColor
                    source: PlasmaKeyboard.BreezeConstants.breezeIcon("audio-input-microphone-symbolic")
                }
            }
        }
    }
}
