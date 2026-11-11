// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard.lib as Keyboard
Flow {
    id: root
    objectName: "ukeToolbar"
    property var target
    property real perMm: Keyboard.Laptop.pixelsPerMm
    property var popupAreaItem: null
    property rect panelBounds: Qt.rect(0,0,0,0)
    readonly property real windowHeight: Window.window?Window.window.height:root.height
    readonly property real windowWidth: Window.window?Window.window.width:root.width
    function popupTop(height){
        if(popupAreaItem)return popupAreaItem.mapToItem(root,0,0).y;
        const origin=root.mapToItem(null,0,0).y;
        if(panelBounds.y>=height+perMm)return panelBounds.y-height-perMm-origin;
        const bottom=panelBounds.y+panelBounds.height;
        if(windowHeight-bottom>=height+perMm)return bottom+perMm-origin;
        return Math.max(0,Math.min(windowHeight-height,panelBounds.y-height))-origin;
    }
    function popupLeft(width){
        if(popupAreaItem)return popupAreaItem.mapToItem(root,0,0).x+(popupAreaItem.width-width)/2;
        const origin=root.mapToItem(null,0,0).x;
        return Math.max(-origin,Math.min(0,windowWidth-width-origin));
    }
    readonly property var localEditor: emojiPopup.opened ? search : null
    spacing: perMm
    function openTool(name) {
        if(name==="clipboard"){emojiPopup.close();if(clipboardPopup.opened)clipboardPopup.close();else clipboardPopup.open();}
        else if(name==="emoji"){clipboardPopup.close();if(emojiPopup.opened)emojiPopup.close();else emojiPopup.open();}
        else{clipboardPopup.close();emojiPopup.close();Keyboard.Laptop.tool(name);}
    }
    function fits(popup){const p=root.mapToItem(null,popup.x,popup.y);return p.x>=-0.1&&p.y>=-0.1&&p.x+popup.width<=windowWidth+0.1&&p.y+popup.height<=windowHeight+0.1;}
    readonly property bool popupsFit: fits(emojiPopup)&&fits(clipboardPopup)
    Repeater {
        model: Keyboard.Laptop.options.toolbar
        delegate: RowLayout {
            required property string modelData
            readonly property var tools: modelData==="volume"?[{name:"volume-down",icon:"audio-volume-low"},{name:"mute",icon:"audio-volume-muted"},{name:"volume-up",icon:"audio-volume-high"}]:modelData==="brightness"?[{name:"brightness-down",icon:"brightness-low"},{name:"brightness-up",icon:"brightness-high"}]:modelData==="media"?[{name:"previous",icon:"media-skip-backward"},{name:"play",icon:"media-playback-start"},{name:"next",icon:"media-skip-forward"}]:[{name:modelData,icon:({clipboard:"edit-paste",emoji:"face-smile",language:"input-keyboard",settings:"configure"})[modelData]}]
            width: tools.length*9*root.perMm
            height: 9*root.perMm
            spacing: 0
            Repeater {
                model: parent.tools
                delegate: QQC2.ToolButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 9*root.perMm
                    icon.name: modelData.icon
                    icon.width: Math.round(4*root.perMm)
                    icon.height: Math.round(4*root.perMm)
                    focusPolicy: Qt.NoFocus
                    Accessible.name: Keyboard.Laptop.toolLabel(modelData.name)
                    QQC2.ToolTip.text: Keyboard.Laptop.toolLabel(modelData.name)
                    QQC2.ToolTip.visible: hovered
                    enabled: (modelData.name!=="clipboard"&&modelData.name!=="emoji"&&modelData.name!=="settings") || (root.target&&root.target.auxiliaryUiAllowed)
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
        closePolicy: QQC2.Popup.CloseOnEscape
        onClosed: {if(root.target)root.target.forceActiveFocus();}
        y: root.popupTop(height)
        x: root.popupLeft(width)
        width: Math.min(root.width,100*root.perMm,root.popupAreaItem?root.popupAreaItem.width:root.windowWidth)
        height: Math.min(70*root.perMm,550,root.popupAreaItem?root.popupAreaItem.height:root.windowHeight)
        focus: false
        contentItem: QQC2.ScrollView {
            clip: true
            ColumnLayout {
                width: clipboardPopup.availableWidth
                RowLayout {
                    Layout.fillWidth:true
                    QQC2.Label { Layout.fillWidth:true;text: i18nd("uke-plasma-keyboard-custom", "Clipboard"); font.bold: true; }
                    QQC2.ToolButton {icon.name:"window-close";Accessible.name:i18nd("uke-plasma-keyboard-custom","Close");onClicked:clipboardPopup.close();}
                }
                Repeater {
                    model: clipboardPopup.opened&&root.target?root.target.clipboardHistory.pinnedTexts:[]
                    delegate: RowLayout {
                        required property string modelData
                        required property int index
                        QQC2.Button { Layout.fillWidth:true;text:modelData.replace(/\s+/g," ").slice(0,120);focusPolicy:Qt.NoFocus;onClicked:{root.target.commitText(modelData);clipboardPopup.close();} }
                        QQC2.ToolButton {icon.name:"window-unpin";Accessible.name:i18nd("uke-plasma-keyboard-custom", "Unpin");focusPolicy:Qt.NoFocus;onClicked:root.target.clipboardHistory.unpin(index);}
                    }
                }
                Repeater {
                    model: clipboardPopup.opened&&root.target?root.target.clipboardHistory:null
                    delegate: RowLayout {
                        required property string text
                        required property int index
                        QQC2.Button { Layout.fillWidth:true;text:parent.text.replace(/\s+/g," ").slice(0,120);focusPolicy:Qt.NoFocus;onClicked:{root.target.pasteClipboardEntry(index);clipboardPopup.close();} }
                        QQC2.ToolButton {icon.name:"window-pin";Accessible.name:i18nd("uke-plasma-keyboard-custom", "Pin");focusPolicy:Qt.NoFocus;onClicked:root.target.clipboardHistory.pin(index);}
                    }
                }
                QQC2.Label {text:i18nd("uke-plasma-keyboard-custom", "Clipboard follows Plasma/Klipper settings. Password fields are excluded.");wrapMode:Text.Wrap;Layout.fillWidth:true;}
            }
        }
    }
    QQC2.Popup {
        id: emojiPopup
        closePolicy: QQC2.Popup.CloseOnEscape
        onOpened: search.forceActiveFocus()
        onClosed: {if(root.target)root.target.forceActiveFocus();}
        y: root.popupTop(height)
        x: root.popupLeft(width)
        width: Math.min(root.width,110*root.perMm,root.popupAreaItem?root.popupAreaItem.width:root.windowWidth)
        height: Math.min(80*root.perMm,600,root.popupAreaItem?root.popupAreaItem.height:root.windowHeight)
        focus: false
        contentItem: ColumnLayout {
            RowLayout {
                Layout.fillWidth:true
                QQC2.Label {Layout.fillWidth:true;text:i18nd("uke-plasma-keyboard-custom","Emoji");font.bold:true;}
                QQC2.ToolButton {icon.name:"window-close";Accessible.name:i18nd("uke-plasma-keyboard-custom","Close");onClicked:emojiPopup.close();}
            }
            QQC2.TextField {
                id: search
                Layout.fillWidth: true
                placeholderText: i18nd("uke-plasma-keyboard-custom", "Search emoji")
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
