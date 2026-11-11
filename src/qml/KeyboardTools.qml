// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard.lib as Keyboard
RowLayout {
    id: root
    property var target
    property real perMm: Keyboard.Laptop.pixelsPerMm
    readonly property var localEditor: emojiPopup.opened ? search : null
    implicitHeight: 9*perMm
    spacing: perMm
    function openTool(name) {
        if(name==="clipboard")clipboardPopup.open();
        else if(name==="emoji")emojiPopup.open();
        else Keyboard.Laptop.tool(name);
    }
    Repeater {
        model: Keyboard.Laptop.options.toolbar
        delegate: RowLayout {
            required property string modelData
            readonly property var tools: modelData==="volume"?[{name:"volume-down",icon:"audio-volume-low"},{name:"mute",icon:"audio-volume-muted"},{name:"volume-up",icon:"audio-volume-high"}]:modelData==="brightness"?[{name:"brightness-down",icon:"brightness-low"},{name:"brightness-up",icon:"brightness-high"}]:modelData==="media"?[{name:"previous",icon:"media-skip-backward"},{name:"play",icon:"media-playback-start"},{name:"next",icon:"media-skip-forward"}]:[{name:modelData,icon:({clipboard:"edit-paste",emoji:"face-smile",language:"input-keyboard",settings:"configure"})[modelData]}]
            Layout.fillWidth: true
            spacing: 0
            Repeater {
                model: parent.tools
                delegate: QQC2.ToolButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 9*root.perMm
                    icon.name: modelData.icon
                    focusPolicy: Qt.NoFocus
                    Accessible.name: modelData.name
                    QQC2.ToolTip.text: modelData.name
                    QQC2.ToolTip.visible: hovered
                    enabled: (modelData.name!=="clipboard"&&modelData.name!=="emoji") || (root.target&&root.target.auxiliaryUiAllowed)
                    onClicked: root.openTool(modelData.name)
                }
            }
        }
    }
    Connections {
        target: root.target
        function onPredictionTargetChanged(){if(!root.target.auxiliaryUiAllowed){clipboardPopup.close();emojiPopup.close();}}
    }
    QQC2.Popup {
        id: clipboardPopup
        y: parent.height
        width: Math.min(root.width,100*root.perMm)
        height: Math.min(70*root.perMm,550)
        focus: false
        contentItem: QQC2.ScrollView {
            clip: true
            ColumnLayout {
                width: clipboardPopup.availableWidth
                QQC2.Label { text: qsTr("Clipboard"); font.bold: true; }
                Repeater {
                    model: clipboardPopup.opened&&root.target?root.target.clipboardHistory.pinnedTexts:[]
                    delegate: RowLayout {
                        required property string modelData
                        required property int index
                        QQC2.Button { Layout.fillWidth:true;text:modelData.replace(/\s+/g," ").slice(0,120);focusPolicy:Qt.NoFocus;onClicked:{root.target.commitText(modelData);clipboardPopup.close();} }
                        QQC2.ToolButton {icon.name:"window-unpin";Accessible.name:qsTr("Unpin");focusPolicy:Qt.NoFocus;onClicked:root.target.clipboardHistory.unpin(index);}
                    }
                }
                Repeater {
                    model: clipboardPopup.opened&&root.target?root.target.clipboardHistory:null
                    delegate: RowLayout {
                        required property string text
                        required property int index
                        QQC2.Button { Layout.fillWidth:true;text:parent.text.replace(/\s+/g," ").slice(0,120);focusPolicy:Qt.NoFocus;onClicked:{root.target.pasteClipboardEntry(index);clipboardPopup.close();} }
                        QQC2.ToolButton {icon.name:"window-pin";Accessible.name:qsTr("Pin");focusPolicy:Qt.NoFocus;onClicked:root.target.clipboardHistory.pin(index);}
                    }
                }
                QQC2.Label {text:qsTr("Clipboard follows Plasma/Klipper settings. Password fields are excluded.");wrapMode:Text.Wrap;Layout.fillWidth:true;}
            }
        }
    }
    QQC2.Popup {
        id: emojiPopup
        y: parent.height
        width: Math.min(root.width,110*root.perMm)
        height: Math.min(80*root.perMm,600)
        focus: false
        contentItem: ColumnLayout {
            QQC2.TextField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search emoji")
                inputMethodHints: Qt.ImhNoPredictiveText
                selectByMouse: true
            }
            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                GridLayout {
                    width: emojiPopup.availableWidth
                    columns: Math.max(3,Math.floor(width/(11*root.perMm)))
                    Repeater {
                        model: emojiPopup.opened?Keyboard.Laptop.emojiMatches(search.text):[]
                        delegate: QQC2.Button {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 10*root.perMm
                            text: modelData.emoji
                            font.pixelSize: 5*root.perMm
                            focusPolicy: Qt.NoFocus
                            Accessible.name: modelData.name
                            onClicked: {if(root.target.auxiliaryUiAllowed)root.target.commitText(modelData.emoji);emojiPopup.close();}
                        }
                    }
                }
            }
        }
    }
}
