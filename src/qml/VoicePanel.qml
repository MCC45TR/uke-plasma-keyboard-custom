// SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

pragma ComponentBehavior: Bound

import QtQuick

import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard.lib as PlasmaKeyboard

/**
 * The voice input mode: while it is on, the keys are replaced by one big
 * microphone button in the middle of the keyboard area, the way the phone
 * keyboards show it.
 *
 * The first tap starts the recording, the second one stops it and recognises
 * what was said. The recognised text is inserted by the keyboard, which also
 * leaves this mode. The row under the button keeps the keys that are needed
 * while dictating: the space bar, backspace and enter.
 *
 * A gamepad drives the page the same way it drives the ordinary keyboard: the
 * directions move the highlight between the seven controls (the microphone, the
 * four keys of the row under it and the two corner buttons), A activates the
 * highlighted one, and every control carries the badge of the button that
 * reaches it.
 */
Item {
    id: root

    /*! Whether the microphone is recording right now. */
    property bool recording: false

    /*! Whether a recorded phrase is being recognised. */
    property bool busy: false

    /*! Peak level of the recording, 0..1, for the pulsing ring. */
    property real level: 0

    /*! Message shown under the button, for example when the model is missing. */
    property string message: ""

    /*! Whether the gamepad navigation is on: the highlight is shown on the
        element the gamepad selected. */
    property bool highlighted: false

    /*! Index of the element the gamepad selected, see the focus* properties. */
    property int focusIndex: 0

    /*! Name of the language of the active layout, shown on the space bar the
        same way the ordinary keyboard shows it. */
    property string languageName: ""

    /*! Height of the keys in the row under the microphone. */
    readonly property real keyHeight: Math.min(height * 0.17, width * 0.11)

    // The controls the gamepad walks, in the order of the focus index: the
    // microphone, then the row under it (language, space, enter, backspace),
    // then the corner buttons (hide, back).
    readonly property int microphoneIndex: 0
    readonly property int actionRowFirstIndex: 1
    readonly property int actionRowCount: 4
    readonly property int hideIndex: actionRowFirstIndex + actionRowCount
    readonly property int closeIndex: hideIndex + 1

    signal toggleRequested()
    signal closeRequested()
    signal hideRequested()
    signal languageRequested()
    signal spaceRequested()
    signal enterRequested()
    signal backspaceRequested()

    /*! First element of the group the gamepad walks inside. */
    function groupFirstIndex(group) {
        if (group === 0) {
            return microphoneIndex;
        }
        return group === 1 ? actionRowFirstIndex : hideIndex;
    }

    /*! Number of elements in that group. */
    function groupCount(group) {
        if (group === 0) {
            return 1;
        }
        return group === 1 ? actionRowCount : 2;
    }

    /*! Group of the given element: 0 — the microphone, 1 — the row under it,
        2 — the corner buttons. */
    function groupOf(index) {
        if (index <= microphoneIndex) {
            return 0;
        }
        return index < hideIndex ? 1 : 2;
    }

    /*! Whether the gamepad highlight is on the element with that index. */
    function focused(index) {
        return root.highlighted && root.focusIndex === index;
    }

    /*! Moves the highlight with a gamepad direction. Sideways it walks the
        group the highlight is in, up and down it walks the groups. */
    function navigate(key) {
        const group = root.groupOf(root.focusIndex);
        const first = root.groupFirstIndex(group);
        const count = root.groupCount(group);

        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            const offset = root.focusIndex - first;
            const step = key === Qt.Key_Right ? 1 : count - 1;
            root.focusIndex = first + (offset + step) % count;
            return;
        }

        if (key === Qt.Key_Up || key === Qt.Key_Down) {
            const nextGroup = (group + (key === Qt.Key_Down ? 1 : 2)) % 3;
            root.focusIndex = root.groupFirstIndex(nextGroup);
        }
    }

    /*! Activates the element the highlight is on, like A on the ordinary
        keyboard types the highlighted key. */
    function activate() {
        switch (root.focusIndex) {
        case root.microphoneIndex:
            root.toggleRequested();
            break;
        case root.actionRowFirstIndex:
            root.languageRequested();
            break;
        case root.actionRowFirstIndex + 1:
            root.spaceRequested();
            break;
        case root.actionRowFirstIndex + 2:
            root.enterRequested();
            break;
        case root.actionRowFirstIndex + 3:
            root.backspaceRequested();
            break;
        case root.hideIndex:
            root.hideRequested();
            break;
        case root.closeIndex:
            root.closeRequested();
            break;
        }
    }

    Rectangle {
        anchors.fill: parent
        color: PlasmaKeyboard.Theme.current.keyboardBackgroundColor
    }

    // Swallow the taps that land next to the controls: the keys below must not
    // react while the voice mode covers them.
    MouseArea {
        anchors.fill: parent
    }

    // Corner buttons: hide the keyboard and go back to the ordinary keyboard.
    // The arrow is deliberately large: leaving the voice mode has to be obvious.
    Row {
        id: cornerButtons
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin * 2
        spacing: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin * 2

        readonly property real buttonSize: Math.min(root.width, root.height) * 0.17

        Item {
            id: hideButton
            width: cornerButtons.buttonSize
            height: width

            PlasmaKeyboard.BreezeKeyPanel {
                anchors.fill: parent
                Item {
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        implicitWidth: parent.width * 0.45
                        implicitHeight: parent.height * 0.45
                        color: PlasmaKeyboard.BreezeConstants.keyTextColor
                        source: PlasmaKeyboard.BreezeConstants.breezeIcon("input-keyboard-virtual-hide-symbolic")
                    }
                }
            }

            // Start has no letter on the controller, so the badge shows the
            // menu glyph the button carries on handhelds.
            GamepadBadge {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                glyph: "≡"
                badgeColor: "#455a64"
            }

            Rectangle {
                anchors.fill: parent
                radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                color: "transparent"
                border.width: root.focused(root.hideIndex) ? 3 : 0
                border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.focusIndex = root.hideIndex;
                    root.hideRequested();
                }
            }
        }

        Item {
            id: closeButton
            width: cornerButtons.buttonSize
            height: width

            PlasmaKeyboard.BreezeKeyPanel {
                anchors.fill: parent
                Item {
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        implicitWidth: parent.width * 0.45
                        implicitHeight: parent.height * 0.45
                        color: PlasmaKeyboard.BreezeConstants.keyTextColor
                        source: PlasmaKeyboard.BreezeConstants.icon("go-previous-symbolic")
                    }
                }
            }

            GamepadBadge {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                glyph: "B"
                badgeColor: "#c62828"
            }

            Rectangle {
                anchors.fill: parent
                radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                color: "transparent"
                border.width: root.focused(root.closeIndex) ? 3 : 0
                border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.focusIndex = root.closeIndex;
                    root.closeRequested();
                }
            }
        }
    }

    // The big microphone button.
    Item {
        id: microphone
        readonly property real diameter: Math.min(parent.width * 0.5, parent.height * 0.5)
        width: diameter
        height: diameter
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -parent.height * 0.12

        // Pulsing ring that follows the voice level while recording.
        Rectangle {
            id: ring
            anchors.centerIn: parent
            width: parent.width * (1.0 + 0.18 * Math.min(1, root.level * 4))
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(2, width * 0.03)
            border.color: PlasmaKeyboard.BreezeConstants.primaryDarkColor
            opacity: root.recording ? 0.55 : 0
        }

        Rectangle {
            id: button
            anchors.fill: parent
            radius: width / 2
            color: root.recording ? PlasmaKeyboard.BreezeConstants.primaryDarkColor : PlasmaKeyboard.Theme.current.normalKeyBackgroundColor
            border.width: root.focused(root.microphoneIndex) ? 3 : 0
            border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor

            Kirigami.Icon {
                anchors.centerIn: parent
                implicitWidth: parent.width * 0.4
                implicitHeight: parent.height * 0.4
                color: PlasmaKeyboard.Theme.current.keyTextColor
                source: PlasmaKeyboard.BreezeConstants.breezeIcon("audio-input-microphone-symbolic")
                opacity: root.busy ? 0.4 : 1
            }
        }

        GamepadBadge {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: parent.width * 0.14
            glyph: "A"
            badgeColor: "#2e7d32"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                root.focusIndex = root.microphoneIndex;
                root.toggleRequested();
            }
        }
    }

    Text {
        id: statusText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: microphone.bottom
        anchors.topMargin: Kirigami.Units.smallSpacing
        width: parent.width * 0.8
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: PlasmaKeyboard.Theme.current.keyTextColor
        font.family: PlasmaKeyboard.BreezeConstants.fontFamily
        font.pixelSize: Math.max(Kirigami.Units.gridUnit, parent.height * 0.05)
        text: {
            if (root.message.length > 0) {
                return root.message;
            }
            if (root.busy) {
                return i18nd("uke-plasma-keyboard-custom", "Recognising…");
            }
            if (root.recording) {
                return i18nd("uke-plasma-keyboard-custom", "Listening… tap to stop");
            }
            return i18nd("uke-plasma-keyboard-custom", "Tap to speak");
        }
    }

    // The keys that are still needed while dictating, centred under the
    // microphone: space, backspace and enter.
    Row {
        id: actionRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.height * 0.05
        spacing: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin * 2
        height: root.keyHeight

        Repeater {
            model: [
                {
                    "kind": "language",
                    "icon": "globe-symbolic",
                    "glyph": "RB",
                    "badgeColor": "#455a64",
                    "weight": 1.5
                },
                {
                    "kind": "space",
                    "icon": "",
                    "glyph": "Y",
                    "badgeColor": "#f9a825",
                    "weight": 5
                },
                {
                    "kind": "enter",
                    "icon": "keyboard-enter-symbolic",
                    "glyph": "RT",
                    "badgeColor": "#455a64",
                    "weight": 1.5
                },
                {
                    "kind": "backspace",
                    "icon": "edit-clear-symbolic",
                    "glyph": "X",
                    "badgeColor": "#1565c0",
                    "weight": 1.5
                }
            ]

            delegate: Item {
                required property var modelData
                required property int index
                width: root.keyHeight * modelData.weight
                height: root.keyHeight

                Kirigami.ShadowedRectangle {
                    anchors.fill: parent
                    anchors.margins: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin
                    radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                    color: keyHandler.pressed ? PlasmaKeyboard.Theme.current.keyColorFor("normal", "pressed") : PlasmaKeyboard.Theme.current.normalKeyBackgroundColor
                    border.width: root.focused(root.actionRowFirstIndex + index) ? 3 : 0
                    border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: Math.round(parent.height * 0.55)
                        height: width
                        visible: modelData.icon.length > 0
                        source: PlasmaKeyboard.BreezeConstants.icon(modelData.icon)
                        color: PlasmaKeyboard.Theme.current.keyTextColor
                    }

                    // The space bar carries the active layout, like the space
                    // bar of the ordinary keyboard does: smaller and dimmed, so
                    // it stays a hint rather than a label.
                    Text {
                        anchors.centerIn: parent
                        visible: modelData.kind === "space" && root.languageName.length > 0
                        text: root.languageName
                        color: PlasmaKeyboard.Theme.current.keyTextColor
                        opacity: 0.5
                        font.family: PlasmaKeyboard.BreezeConstants.fontFamily
                        font.pixelSize: Math.max(Kirigami.Units.gridUnit * 0.75, parent.height * 0.3)
                    }
                }

                GamepadBadge {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin + 4
                    glyph: modelData.glyph
                    badgeColor: modelData.badgeColor
                }

                MouseArea {
                    id: keyHandler
                    anchors.fill: parent
                    onClicked: {
                        root.focusIndex = root.actionRowFirstIndex + index;
                        if (modelData.kind === "language") {
                            root.languageRequested();
                        } else if (modelData.kind === "space") {
                            root.spaceRequested();
                        } else if (modelData.kind === "enter") {
                            root.enterRequested();
                        } else {
                            root.backspaceRequested();
                        }
                    }
                }
            }
        }
    }
}
