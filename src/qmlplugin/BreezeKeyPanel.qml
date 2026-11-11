// SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Styles

import org.uke.plasma.keyboard
import org.kde.kirigami as Kirigami

KeyPanel {
    id: root

    default property alias contentItem: visualContainer.contentItem

    property alias background: visualContainer.background

    property real padding: BreezeConstants.keyBackgroundMargin

    property real radius: BreezeConstants.buttonRadius

    readonly property string category: Theme.categoryOf(control)
    readonly property var outline: Theme.current.keyOutlineFor(category)
    readonly property real shadowStrength: Theme.current.keyShadowFor(category)

    property color color: {
        if (control && control.latched) {
            return Theme.current.categoryActiveColorFor(category);
        } else if (control && control.keyType === QtVirtualKeyboard.KeyType.ShiftKey
                   && (InputContext.shiftActive || InputContext.capsLockActive)) {
            // Show the shift key as latched while shift/caps is active.
            return Theme.current.categoryActiveColorFor(category);
        } else if (control && control.pressed) {
            return Theme.current.keyColorFor(category, "pressed");
        } else if (control && control.highlighted) {
            return Theme.current.keyColorFor(category, "highlighted");
        }
        return Theme.current.keyColorFor(category, "normal");
    }

    soundEffect: PlasmaKeyboardSettings.soundEnabled ? Qt.resolvedUrl('qrc:///sounds/keyboard_tick2_quiet.wav') : ''

    QQC2.Control {
        id: visualContainer
        anchors.fill: parent
        anchors.margins: root.padding

        background: Kirigami.ShadowedRectangle {
            color: root.color
            radius: root.radius

            border.width: root.outline.width
            border.color: root.outline.color

            // Shadow
            shadow.color: Qt.rgba(0, 0, 0, 0.2 * root.shadowStrength)
            shadow.size: 3 * root.shadowStrength
            shadow.yOffset: 1 * root.shadowStrength
        }
    }

    // Implement key vibration
    Connections {
        target: root.control
        function onPressedChanged() {
            if (root.control.pressed && PlasmaKeyboardSettings.vibrationEnabled) {
                Vibration.vibrate(PlasmaKeyboardSettings.vibrationMs, PlasmaKeyboardSettings.vibrationStrength);
            }
        }
    }
}
