// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard.lib as Keyboard
Kirigami.ApplicationWindow {
    id: settings
    property bool ukePreferencesWindow: true
    width: 860
    height: 780
    minimumWidth: 400
    minimumHeight: 400
    visible: true
    title: qsTr("Uke keyboard — laptop and physical layout")
    pageStack.initialPage: Kirigami.ScrollablePage {
        title: qsTr("Keyboard and laptop mode")
        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.largeSpacing
            Kirigami.InlineMessage {Layout.fillWidth:true;visible:Keyboard.Laptop.reason.length>0;text:Keyboard.Laptop.reason;type:Kirigami.MessageType.Information;}
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("Static laptop mode requires an enabled touchscreen and an extended external display. Display power, layout, scale and rotation are never changed.");}
            QQC2.Button {text:Keyboard.Laptop.active?qsTr("Return to normal keyboard"):qsTr("Enter static laptop mode");enabled:Keyboard.Laptop.active||Keyboard.Laptop.available;onClicked:Keyboard.Laptop.toggle();}
            Kirigami.FormLayout {
                Layout.fillWidth:true
                QQC2.ComboBox {
                    Kirigami.FormData.label: qsTr("Static refresh profile:")
                    model:[qsTr("Keep current settings"),qsTr("Fixed 30 Hz"),qsTr("Fixed 60 Hz"),qsTr("Adaptive 30–60 Hz (qualification required)")]
                    currentIndex:["keep","30","60","adaptive"].indexOf(Keyboard.Laptop.options.refreshProfile)
                    onActivated:Keyboard.Laptop.setOption("refreshProfile",["keep","30","60","adaptive"][currentIndex])
                }
                Repeater {
                    model:[{key:"keyWidthMm",label:qsTr("Target key width (mm):"),min:70,max:240},{key:"keyHeightMm",label:qsTr("Target key height (mm):"),min:70,max:240},{key:"gapMm",label:qsTr("Key gap (mm):"),min:0,max:40},{key:"touchpadHeightMm",label:qsTr("Touchpad height (mm):"),min:200,max:1500},{key:"physicalWidthMm",label:qsTr("Physical panel width (mm, 0 = automatic):"),min:0,max:10000},{key:"physicalHeightMm",label:qsTr("Physical panel height (mm, 0 = automatic):"),min:0,max:10000}]
                    delegate: QQC2.SpinBox {
                        required property var modelData
                        Kirigami.FormData.label:modelData.label
                        from:modelData.min;to:modelData.max;stepSize:1;editable:true
                        value:Math.round(Keyboard.Laptop.options[modelData.key]*10)
                        textFromValue:v=>(v/10).toFixed(1)
                        valueFromText:t=>Math.round(parseFloat(t)*10)
                        onValueModified:Keyboard.Laptop.setOption(modelData.key,value/10)
                    }
                }
                QQC2.Slider {Kirigami.FormData.label:qsTr("Touchpad width:");from:30;to:100;stepSize:1;value:Keyboard.Laptop.options.touchpadWidthPercent;onMoved:Keyboard.Laptop.setOption("touchpadWidthPercent",Math.round(value));}
                QQC2.Slider {Kirigami.FormData.label:qsTr("Pointer speed:");from:0.2;to:4;value:Keyboard.Laptop.options.pointerSpeed;onMoved:Keyboard.Laptop.setOption("pointerSpeed",value);}
            }
            Rectangle {
                Layout.preferredWidth:50*Keyboard.Laptop.pixelsPerMm
                Layout.preferredHeight:8*Keyboard.Laptop.pixelsPerMm
                border.color:Kirigami.Theme.textColor
                color:Kirigami.Theme.alternateBackgroundColor
                QQC2.Label {anchors.centerIn:parent;text:qsTr("50 mm calibration ruler");}
            }
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("Measure the ruler on the tablet panel. If its length is wrong, enter the visible panel width and height in millimetres. Keys adapt to rotation and fit smaller screens.");}
            Repeater {
                model:[{key:"functionRow",label:qsTr("F1–F12 row")},{key:"modifierRow",label:qsTr("Ctrl / Alt / Meta region")},{key:"arrowRow",label:qsTr("Arrow keys")},{key:"navigationRow",label:qsTr("Home / End / Page Up / Page Down / Insert / Delete")},{key:"systemRow",label:qsTr("Print Screen / Scroll Lock / Pause")},{key:"nativePcLayout",label:qsTr("Use dynamic PC layout (language engines remain available)")},{key:"touchpadFullWidth",label:qsTr("Use the entire bottom area as touchpad")},{key:"leftButton",label:qsTr("Show left-click button")},{key:"middleButton",label:qsTr("Show middle-click button")},{key:"rightButton",label:qsTr("Show right-click button")},{key:"naturalScroll",label:qsTr("Natural scrolling")},{key:"palmGuard",label:qsTr("Short accidental-touch protection while typing")},{key:"spaceGesture",label:qsTr("Slide on Space to move the text cursor")},{key:"deleteGesture",label:qsTr("Slide left on Backspace to delete words")},{key:"keyPreview",label:qsTr("Show character preview bubbles")},{key:"syncLayout",label:qsTr("Synchronize input layout with Plasma (never the interface language)")}]
                delegate: QQC2.CheckBox {
                    required property var modelData
                    text:modelData.label
                    checked:Keyboard.Laptop.options[modelData.key]
                    onToggled:Keyboard.Laptop.setOption(modelData.key,checked)
                }
            }
            Kirigami.Heading {text:qsTr("Languages and dictionaries");level:2;}
            QQC2.ComboBox {Layout.fillWidth:true;model:Keyboard.Laptop.layouts;textRole:"label";onActivated:Keyboard.Laptop.selectLayout(currentIndex);}
            RowLayout {
                Layout.fillWidth:true
                QQC2.ComboBox {id:addLanguage;Layout.fillWidth:true;model:Keyboard.Laptop.allLayouts();textRole:"label";onActivated:addVariant.currentIndex=0;}
                QQC2.ComboBox {id:addVariant;Layout.fillWidth:true;model:[{code:"",label:qsTr("Default variant")}].concat(addLanguage.currentIndex>=0?Keyboard.Laptop.variants(addLanguage.model[addLanguage.currentIndex].code):[]);textRole:"label";}
                QQC2.Button {text:qsTr("Add");onClicked:Keyboard.Laptop.addLayout(addLanguage.model[addLanguage.currentIndex].code,addVariant.model[addVariant.currentIndex].code);}
            }
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:Keyboard.Laptop.dictionaryAvailable?qsTr("An installed dictionary is available. Suggestions require choosing a candidate; personal learning is disabled."):qsTr("Optional Fedora package: %1. Install the dictionary for this language to enable suggestions. Personal learning is disabled.").arg(Keyboard.Laptop.dictionaryPackage);}
            Kirigami.Heading {text:qsTr("Top toolbar");level:2;}
            Repeater {
                model:["volume","brightness","media","clipboard","emoji","language","settings"]
                delegate: RowLayout {
                    required property string modelData
                    readonly property int position:Keyboard.Laptop.options.toolbar.indexOf(modelData)
                    QQC2.CheckBox {text:modelData;checked:parent.position>=0;onToggled:{const items=Keyboard.Laptop.options.toolbar.slice();const n=items.indexOf(modelData);if(checked&&n<0)items.push(modelData);else if(!checked&&n>=0)items.splice(n,1);Keyboard.Laptop.setOption("toolbar",items);}}
                    QQC2.ToolButton {text:"↑";enabled:parent.position>0;onClicked:{const items=Keyboard.Laptop.options.toolbar.slice();const n=items.indexOf(modelData);[items[n-1],items[n]]=[items[n],items[n-1]];Keyboard.Laptop.setOption("toolbar",items);}}
                    QQC2.ToolButton {text:"↓";enabled:parent.position>=0&&parent.position<Keyboard.Laptop.options.toolbar.length-1;onClicked:{const items=Keyboard.Laptop.options.toolbar.slice();const n=items.indexOf(modelData);[items[n+1],items[n]]=[items[n],items[n+1]];Keyboard.Laptop.setOption("toolbar",items);}}
                }
            }
            QQC2.Button {text:qsTr("Typing, themes, sound and voice settings");onClicked:Qt.openUrlExternally("systemsettings:kcm_uke_plasma_keyboard_custom");}
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("Dynamic laptop mode is planned for a later release. It is not enabled by these settings.");}
        }
    }
}
