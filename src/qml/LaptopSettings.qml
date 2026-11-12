// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard.lib as Keyboard
Kirigami.ApplicationWindow {
    id: settings
    property bool ukePreferencesWindow: true
    function moveToolbarTool(tool,step,restoreFocus) {
        const items=Keyboard.Laptop.options.toolbar.slice();
        const from=items.indexOf(tool),to=from+step;
        if(from<0||to<0||to>=items.length)return;
        [items[from],items[to]]=[items[to],items[from]];
        Keyboard.Laptop.setOption("toolbar",items);
        if(restoreFocus)Qt.callLater(function(){
            for(let i=0;i<toolbarRepeater.count;++i){
                const row=toolbarRepeater.itemAt(i);
                if(row&&row.modelData===tool){row.restoreMoveFocus(step);return;}
            }
        });
    }
    width: 860
    height: 780
    minimumWidth: 400
    minimumHeight: 400
    visible: true
    title: i18nd("uke-plasma-keyboard-custom", "Uke keyboard — laptop and physical layout")
    pageStack.initialPage: Kirigami.ScrollablePage {
        title: i18nd("uke-plasma-keyboard-custom", "Keyboard and laptop mode")
        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.largeSpacing
            Kirigami.InlineMessage {Layout.fillWidth:true;visible:Keyboard.Laptop.reason.length>0;text:Keyboard.Laptop.reason;type:Kirigami.MessageType.Information;}
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:i18nd("uke-plasma-keyboard-custom", "Static laptop mode requires an enabled touchscreen, an extended external display and a face-up tablet within 45 degrees of horizontal. After tilting upright, lower it below 40 degrees to reopen. The movable panel opens by button only.");}
            QQC2.Button {text:Keyboard.Laptop.active?i18nd("uke-plasma-keyboard-custom", "Return to normal keyboard"):Keyboard.Laptop.entryPending?i18nd("uke-plasma-keyboard-custom", "Cancel laptop mode"):i18nd("uke-plasma-keyboard-custom", "Enter laptop mode");enabled:Keyboard.Laptop.active||Keyboard.Laptop.entryPending||Keyboard.Laptop.canRequest;onClicked:Keyboard.Laptop.toggle();}
            Kirigami.FormLayout {
                Layout.fillWidth:true
                QQC2.ComboBox {
                    Kirigami.FormData.label:i18nd("uke-plasma-keyboard-custom","Laptop surface:")
                    model:[i18nd("uke-plasma-keyboard-custom","Static — full tablet"),i18nd("uke-plasma-keyboard-custom","Dynamic — movable panel")]
                    currentIndex:Keyboard.Laptop.options.presentation==="dynamic"?1:0
                    onActivated:Keyboard.Laptop.setOption("presentation",currentIndex===1?"dynamic":"static")
                }
                QQC2.CheckBox {
                    visible:Keyboard.Laptop.options.presentation==="static"
                    text:i18nd("uke-plasma-keyboard-custom","Open static mode automatically when its conditions are met")
                    checked:Keyboard.Laptop.options.staticAutoShow
                    onToggled:Keyboard.Laptop.setOption("staticAutoShow",checked)
                }
                QQC2.Slider {visible:Keyboard.Laptop.options.presentation==="dynamic";Kirigami.FormData.label:i18nd("uke-plasma-keyboard-custom","Dynamic panel width:");from:40;to:95;stepSize:1;value:Keyboard.Laptop.options.dynamicWidthPercent;onMoved:Keyboard.Laptop.setOption("dynamicWidthPercent",Math.round(value));}
                QQC2.Slider {visible:Keyboard.Laptop.options.presentation==="dynamic";Kirigami.FormData.label:i18nd("uke-plasma-keyboard-custom","Dynamic panel height:");from:40;to:95;stepSize:1;value:Keyboard.Laptop.options.dynamicHeightPercent;onMoved:Keyboard.Laptop.setOption("dynamicHeightPercent",Math.round(value));}
                QQC2.ComboBox {
                    enabled:Keyboard.Laptop.options.presentation==="static"
                    Kirigami.FormData.label: i18nd("uke-plasma-keyboard-custom", "Static refresh profile:")
                    model:[i18nd("uke-plasma-keyboard-custom", "Keep current settings"),i18nd("uke-plasma-keyboard-custom", "Fixed 30 Hz"),i18nd("uke-plasma-keyboard-custom", "Fixed 60 Hz"),i18nd("uke-plasma-keyboard-custom", "Adaptive 30–60 Hz (qualification required)")]
                    currentIndex:["keep","30","60","adaptive"].indexOf(Keyboard.Laptop.options.refreshProfile)
                    onActivated:Keyboard.Laptop.setOption("refreshProfile",["keep","30","60","adaptive"][currentIndex])
                }
                Repeater {
                    model:[{key:"keyWidthMm",label:i18nd("uke-plasma-keyboard-custom", "Target key width (mm):"),min:70,max:240},{key:"keyHeightMm",label:i18nd("uke-plasma-keyboard-custom", "Target key height (mm):"),min:70,max:240},{key:"gapMm",label:i18nd("uke-plasma-keyboard-custom", "Key gap (mm):"),min:0,max:40},{key:"touchpadHeightMm",label:i18nd("uke-plasma-keyboard-custom", "Touchpad height (mm):"),min:200,max:1500},{key:"physicalWidthMm",label:i18nd("uke-plasma-keyboard-custom", "Physical panel width (mm, 0 = automatic):"),min:0,max:10000},{key:"physicalHeightMm",label:i18nd("uke-plasma-keyboard-custom", "Physical panel height (mm, 0 = automatic):"),min:0,max:10000}]
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
                QQC2.Slider {Kirigami.FormData.label:i18nd("uke-plasma-keyboard-custom", "Touchpad width:");from:30;to:100;stepSize:1;value:Keyboard.Laptop.options.touchpadWidthPercent;onMoved:Keyboard.Laptop.setOption("touchpadWidthPercent",Math.round(value));}
                QQC2.Slider {Kirigami.FormData.label:i18nd("uke-plasma-keyboard-custom", "Pointer speed:");from:0.2;to:4;value:Keyboard.Laptop.options.pointerSpeed;onMoved:Keyboard.Laptop.setOption("pointerSpeed",value);}
            }
            QQC2.CheckBox {text:i18nd("uke-plasma-keyboard-custom", "Natural pointer acceleration");checked:Keyboard.Laptop.options.pointerAcceleration;onToggled:Keyboard.Laptop.setOption("pointerAcceleration",checked);}
            QQC2.CheckBox {text:i18nd("uke-plasma-keyboard-custom", "Subtle pixel shifting in static mode");checked:Keyboard.Laptop.options.pixelShift;onToggled:Keyboard.Laptop.setOption("pixelShift",checked);}
            QQC2.CheckBox {text:i18nd("uke-plasma-keyboard-custom", "Show a 50 mm calibration ruler on the tablet in laptop mode");checked:Keyboard.Laptop.options.calibrationRuler;onToggled:Keyboard.Laptop.setOption("calibrationRuler",checked);}
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:i18nd("uke-plasma-keyboard-custom", "Measure the ruler on the tablet panel. If its length is wrong, enter the visible panel width and height in millimetres. Keys adapt to rotation and fit smaller screens.");}
            Repeater {
                model:[{key:"functionRow",label:i18nd("uke-plasma-keyboard-custom", "F1–F12 row")},{key:"modifierRow",label:i18nd("uke-plasma-keyboard-custom", "Ctrl / Alt / Meta region")},{key:"arrowRow",label:i18nd("uke-plasma-keyboard-custom", "Arrow keys")},{key:"navigationRow",label:i18nd("uke-plasma-keyboard-custom", "Home / End / Page Up / Page Down / Insert / Delete")},{key:"systemRow",label:i18nd("uke-plasma-keyboard-custom", "Print Screen / Scroll Lock / Pause")},{key:"nativePcLayout",label:i18nd("uke-plasma-keyboard-custom", "Use the PC layout (language engines remain available)")},{key:"normalPcLayout",label:i18nd("uke-plasma-keyboard-custom", "Also use the PC layout in normal mode")},{key:"touchpadFullWidth",label:i18nd("uke-plasma-keyboard-custom", "Use the entire bottom area as touchpad")},{key:"leftButton",label:i18nd("uke-plasma-keyboard-custom", "Show left-click button")},{key:"middleButton",label:i18nd("uke-plasma-keyboard-custom", "Show middle-click button")},{key:"rightButton",label:i18nd("uke-plasma-keyboard-custom", "Show right-click button")},{key:"naturalScroll",label:i18nd("uke-plasma-keyboard-custom", "Natural scrolling")},{key:"palmGuard",label:i18nd("uke-plasma-keyboard-custom", "Short accidental-touch protection while typing")},{key:"spaceGesture",label:i18nd("uke-plasma-keyboard-custom", "Slide on Space to move the text cursor")},{key:"deleteGesture",label:i18nd("uke-plasma-keyboard-custom", "Slide left on Backspace to delete words")},{key:"keyPreview",label:i18nd("uke-plasma-keyboard-custom", "Show character preview bubbles")},{key:"syncLayout",label:i18nd("uke-plasma-keyboard-custom", "Synchronize input layout with Plasma (never the interface language)")}]
                delegate: QQC2.CheckBox {
                    required property var modelData
                    text:modelData.label
                    checked:Keyboard.Laptop.options[modelData.key]
                    onToggled:Keyboard.Laptop.setOption(modelData.key,checked)
                }
            }
            Kirigami.Heading {text:i18nd("uke-plasma-keyboard-custom", "Languages and dictionaries");level:2;}
            QQC2.ComboBox {Layout.fillWidth:true;model:Keyboard.Laptop.layouts;textRole:"label";onActivated:Keyboard.Laptop.selectLayout(currentIndex);}
            RowLayout {
                Layout.fillWidth:true
                QQC2.ComboBox {id:addLanguage;Layout.fillWidth:true;model:Keyboard.Laptop.allLayouts();textRole:"label";onActivated:addVariant.currentIndex=0;}
                QQC2.ComboBox {id:addVariant;Layout.fillWidth:true;model:[{code:"",label:i18nd("uke-plasma-keyboard-custom", "Default variant")}].concat(addLanguage.currentIndex>=0?Keyboard.Laptop.variants(addLanguage.model[addLanguage.currentIndex].code):[]);textRole:"label";}
                QQC2.Button {text:i18nd("uke-plasma-keyboard-custom", "Add");onClicked:Keyboard.Laptop.addLayout(addLanguage.model[addLanguage.currentIndex].code,addVariant.model[addVariant.currentIndex].code);}
            }
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:Keyboard.Laptop.dictionaryAvailable?i18nd("uke-plasma-keyboard-custom", "An installed dictionary is available. Suggestions require choosing a candidate; personal learning is disabled."):i18nd("uke-plasma-keyboard-custom", "Optional Fedora package: %1. Install the dictionary for this language to enable suggestions. Personal learning is disabled.").arg(Keyboard.Laptop.dictionaryPackage);}
            Kirigami.Heading {text:i18nd("uke-plasma-keyboard-custom", "Top toolbar");level:2;}
            Repeater {
                id: toolbarRepeater
                model: {
                    const available=["volume","brightness","media","clipboard","emoji","language","settings"];
                    const selected=Keyboard.Laptop.options.toolbar.slice();
                    return selected.concat(available.filter(tool=>!selected.includes(tool)));
                }
                delegate: RowLayout {
                    id: toolRow
                    objectName: "toolbarSettings_"+modelData
                    Layout.fillWidth: true
                    required property string modelData
                    readonly property int position:Keyboard.Laptop.options.toolbar.indexOf(modelData)
                    readonly property string label:Keyboard.Laptop.toolLabel(modelData)
                    function restoreMoveFocus(step){
                        const button=step<0?earlier:later;
                        if(button.enabled)button.forceActiveFocus();else toolEnabled.forceActiveFocus();
                    }
                    QQC2.CheckBox {id:toolEnabled;Layout.fillWidth:true;text:toolRow.label;checked:toolRow.position>=0;onToggled:{const items=Keyboard.Laptop.options.toolbar.slice();const n=items.indexOf(modelData);if(checked&&n<0)items.push(modelData);else if(!checked&&n>=0)items.splice(n,1);Keyboard.Laptop.setOption("toolbar",items);}}
                    QQC2.ToolButton {
                        id: earlier
                        objectName: "moveEarlier_"+modelData
                        icon.name: "go-up"
                        enabled: toolRow.position>0
                        Accessible.name: i18nd("uke-plasma-keyboard-custom", "Move %1 earlier").arg(toolRow.label)
                        QQC2.ToolTip.text: Accessible.name
                        QQC2.ToolTip.visible: hovered
                        onClicked: settings.moveToolbarTool(toolRow.modelData,-1,activeFocus)
                    }
                    QQC2.ToolButton {
                        id: later
                        objectName: "moveLater_"+modelData
                        icon.name: "go-down"
                        enabled: toolRow.position>=0&&toolRow.position<Keyboard.Laptop.options.toolbar.length-1
                        Accessible.name: i18nd("uke-plasma-keyboard-custom", "Move %1 later").arg(toolRow.label)
                        QQC2.ToolTip.text: Accessible.name
                        QQC2.ToolTip.visible: hovered
                        onClicked: settings.moveToolbarTool(toolRow.modelData,1,activeFocus)
                    }
                }
            }
            QQC2.Button {text:i18nd("uke-plasma-keyboard-custom", "Typing, themes, sound and voice settings");onClicked:Qt.openUrlExternally("systemsettings:kcm_uke_plasma_keyboard_custom");}
            QQC2.Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:i18nd("uke-plasma-keyboard-custom", "Dynamic mode opens from the tray by default. Drag its handle to move it. It leaves pointer movement, window placement and refresh settings under desktop control.");}
        }
    }
}
