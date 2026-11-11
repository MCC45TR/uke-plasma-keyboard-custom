// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard
import org.uke.plasma.keyboard.lib as Keyboard
Rectangle {
    id: deck
    property var inputTarget
    property var recognizer
    property bool useNative: true
    property real perMm: Keyboard.Laptop.pixelsPerMm
    property var predictions: []
    signal swipeAccepted(var words)
    signal predictionChosen(string text)
    function keyGeometry(code) { return pc.keyGeometry(code); }
    readonly property bool toolbarFits: tools.childrenRect.width<=tools.width+0.1
    color: Kirigami.Theme.backgroundColor
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: deck.perMm
        spacing: deck.perMm*0.8
        KeyboardTools { id: tools; Layout.fillWidth:true; target:deck.inputTarget; perMm:deck.perMm; }
        Rectangle {
            visible: Keyboard.Laptop.options.calibrationRuler
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 50*deck.perMm
            Layout.preferredHeight: 6*deck.perMm
            color: Kirigami.Theme.alternateBackgroundColor
            border.color: Kirigami.Theme.textColor
            Text {anchors.centerIn:parent;text:"50 mm";font.pixelSize:3*deck.perMm;color:Kirigami.Theme.textColor;}
        }
        RowLayout {
            Layout.fillWidth: true
            visible: deck.inputTarget&&deck.inputTarget.auxiliaryUiAllowed
            Repeater {
                model: deck.predictions
                delegate: QQC2.Button {
                    required property var modelData
                    Layout.fillWidth:true
                    Layout.preferredHeight: 8*deck.perMm
                    text:modelData.text
                    focusPolicy:Qt.NoFocus
                    onClicked:deck.predictionChosen(modelData.text)
                }
            }
        }
        PcKeyboard {
            id: pc
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(implicitHeight,deck.height*0.58)
            visible: deck.useNative
            target:deck.inputTarget
            recognizer:deck.recognizer
            localEditor:tools.localEditor
            auxiliaryAllowed:deck.inputTarget?deck.inputTarget.auxiliaryUiAllowed:false
            perMm:deck.perMm
            onSwipeAccepted: words=>deck.swipeAccepted(words)
        }
        QQC2.Label {
            visible:!deck.useNative
            Layout.fillWidth:true
            Layout.preferredHeight: deck.height*0.5
            text:i18nd("uke-plasma-keyboard-custom", "The language input panel is displayed above the touchpad.")
            horizontalAlignment:Text.AlignHCenter
            opacity:0
        }
        Touchpad {
            Layout.fillHeight: true
            Layout.minimumHeight: 15*deck.perMm
            Layout.preferredHeight: Keyboard.Laptop.options.touchpadHeightMm*deck.perMm
            Layout.preferredWidth: Keyboard.Laptop.options.touchpadFullWidth?parent.width:parent.width*Keyboard.Laptop.options.touchpadWidthPercent/100
            Layout.alignment:Qt.AlignHCenter
            perMm:deck.perMm
        }
        RowLayout {
            Layout.fillWidth:true
            QQC2.Button {text:i18nd("uke-plasma-keyboard-custom", "Normal keyboard");icon.name:"input-keyboard";focusPolicy:Qt.NoFocus;onClicked:Keyboard.Laptop.leave();}
            QQC2.Label {Layout.fillWidth:true;text:Keyboard.Laptop.reason;wrapMode:Text.Wrap;visible:text.length>0;}
            QQC2.Button {text:i18nd("uke-plasma-keyboard-custom", "Layout");icon.name:"input-keyboard";focusPolicy:Qt.NoFocus;onClicked:Keyboard.Laptop.nextLayout();}
            QQC2.ToolButton {icon.name:"configure";Accessible.name:i18nd("uke-plasma-keyboard-custom", "Keyboard settings");focusPolicy:Qt.NoFocus;onClicked:Keyboard.Laptop.openPreferences();}
        }
    }
}
