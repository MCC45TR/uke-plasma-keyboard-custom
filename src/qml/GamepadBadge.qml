// SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick

import org.uke.plasma.keyboard.lib as PlasmaKeyboard

/**
 * Small round glyph shown on the buttons a gamepad can reach, the way the key
 * panels of the ordinary keyboard show the controller button that activates
 * them. Visible only while a gamepad is available.
 */
Rectangle {
    id: root

    /*! Text of the badge: "A", "B", "X", "Y", "RB", "RT", "Start", ... */
    property string glyph: ""

    /*! Colour of the badge: every controller button keeps its own, so the
        prompts are recognisable by colour as well as by letter. */
    property color badgeColor: "#37474f"

    width: 20
    height: 20
    radius: width / 2
    color: badgeColor
    border.color: "white"
    border.width: 1
    visible: PlasmaKeyboard.Modifiers.gamepadAvailable
    z: 2

    Text {
        anchors.centerIn: parent
        text: root.glyph
        color: "white"
        font.bold: true
        font.pixelSize: root.glyph.length > 1 ? 9 : 12
    }
}
