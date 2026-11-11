// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard
import org.uke.plasma.keyboard.lib as Keyboard

Item {
    id: keyboard
    property var target
    property var localEditor: null
    property var recognizer
    property bool auxiliaryAllowed: false
    property real perMm: Keyboard.Laptop.pixelsPerMm
    property real gap: Keyboard.Laptop.options.gapMm * perMm
    property real keyHeight: Math.min(Keyboard.Laptop.options.keyHeightMm * perMm, (height - gap * Math.max(0, rowRepeater.count - 1)) / Math.max(1, rowRepeater.count))
    implicitHeight: Keyboard.Laptop.rows.length * Keyboard.Laptop.options.keyHeightMm * perMm + Math.max(0,Keyboard.Laptop.rows.length-1)*gap
    signal swipeAccepted(var words)
    function feedback() {
        if(click.item)click.item.play();
        if(PlasmaKeyboardSettings.vibrationEnabled)Vibration.vibrate(PlasmaKeyboardSettings.vibrationMs,PlasmaKeyboardSettings.vibrationStrength);
    }
    Loader {
        id: click
        active: keyboard.visible && PlasmaKeyboardSettings.soundEnabled
        sourceComponent: SoundEffect { source: "qrc:/sounds/keyboard_tick2_quiet.wav"; volume: 1; }
    }
    function emitKey(code,pressed,caption) {
        if (localEditor && ![29,42,54,56,97,100,125,126].includes(code)) {
            if (!pressed) return;
            if(code===14){if(localEditor.selectionStart!==localEditor.selectionEnd)localEditor.remove(localEditor.selectionStart,localEditor.selectionEnd);else if(localEditor.cursorPosition>0)localEditor.remove(localEditor.cursorPosition-1,localEditor.cursorPosition);}
            else if(code===105)localEditor.cursorPosition=Math.max(0,localEditor.cursorPosition-1);
            else if(code===106)localEditor.cursorPosition=Math.min(localEditor.text.length,localEditor.cursorPosition+1);
            else if(code===57)localEditor.insert(localEditor.cursorPosition," ");
            else if(caption.length===1)localEditor.insert(localEditor.cursorPosition,caption);
            return;
        }
        Keyboard.Laptop.key(code,pressed);
    }
    function tapKey(code,caption){emitKey(code,true,caption);emitKey(code,false,caption);}
    function keyMap() {
        const result = [];
        for (let r=0;r<rowRepeater.count;r++) {
            const row=rowRepeater.itemAt(r);
            for(let k=0;k<row.keys.count;k++) {
                const key=row.keys.itemAt(k);
                const text=key.caption;
                if(text.length===1 && text.toLowerCase()!==text.toUpperCase()) {
                    const p=key.mapToItem(keyboard,0,0);
                    result.push({letter:text.toLowerCase(),x:p.x,y:p.y,w:key.width,h:key.height});
                }
            }
        }
        return result;
    }
    Column {
        anchors.fill: parent
        spacing: keyboard.gap
        Repeater {
            id: rowRepeater
            model: Keyboard.Laptop.rows
            delegate: Row {
                id: row
                required property var modelData
                property alias keys: keysRepeater
                readonly property real units: modelData.reduce((sum,k)=>sum+k.units,0)
                width: Math.min(keyboard.width, units*Keyboard.Laptop.options.keyWidthMm*keyboard.perMm + Math.max(0,modelData.length-1)*keyboard.gap)
                x: (keyboard.width-width)/2
                height: keyboard.keyHeight
                spacing: keyboard.gap
                Repeater {
                    id: keysRepeater
                    model: row.modelData
                    delegate: Kirigami.ShadowedRectangle {
                        id: keyItem
                        required property var modelData
                        objectName: "ukeKey_"+code
                        readonly property int code: modelData.code
                        readonly property string caption: { Keyboard.Laptop.keyRevision; return Keyboard.Laptop.keyLabel(code,modelData.dynamic ? "" : modelData.label); }
                        readonly property bool isLatched: { Keyboard.Laptop.keyRevision; return Keyboard.Laptop.latched(code); }
                        readonly property string category: modelData.modifier||code===58?"modifier":code===28?"accent":(code>=2&&code<=13)?"digit":(code===14||(code>=59&&code<=88)||code>=99)?"function":"normal"
                        readonly property var outline: Keyboard.Theme.current.keyOutlineFor(category)
                        readonly property real shadowStrength: Keyboard.Theme.current.keyShadowFor(category)
                        property bool down: false
                        property bool sent: false
                        property bool moved: false
                        property bool popupUsed: false
                        property real startX: 0
                        property real lastStep: 0
                        property var trace: []
                        property real began: 0
                        readonly property bool letter: caption.length===1 && caption.toLowerCase()!==caption.toUpperCase()
                        readonly property bool deferred: letter || code===57 || code===14
                        width: Math.max(1, (row.width-Math.max(0,row.modelData.length-1)*row.spacing)/row.units*modelData.units)
                        height: row.height
                        radius: Math.min(height/4,Keyboard.BreezeConstants.buttonRadius)
                        color: down ? Keyboard.Theme.current.keyColorFor(category,"pressed") : (isLatched ? Keyboard.Theme.current.categoryActiveColorFor(category) : Keyboard.Theme.current.keyColorFor(category,"normal"))
                        border.width: outline.width
                        border.color: outline.color
                        shadow.size: 3*shadowStrength
                        shadow.yOffset: shadowStrength
                        shadow.color: Qt.rgba(0,0,0,0.2*shadowStrength)
                        Accessible.role: Accessible.Button
                        Accessible.name: caption
                        Accessible.onPressAction: {keyboard.feedback();keyboard.tapKey(code,caption);}
                        Text {
                            anchors.fill: parent
                            anchors.margins: keyboard.perMm*0.8
                            text: keyItem.caption
                            color: Keyboard.Theme.current.keyTextColorFor(keyItem.category)
                            font.family: Keyboard.BreezeConstants.fontFamily
                            font.pixelSize: Math.max(7,Math.min(parent.height*0.36,keyboard.perMm*3.4))
                            font.bold: keyItem.modelData.modifier
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            minimumPixelSize: 6
                            fontSizeMode: Text.Fit
                            elide: Text.ElideRight
                        }
                        QQC2.ToolTip.visible: down && Keyboard.Laptop.options.keyPreview && letter
                        QQC2.ToolTip.text: caption
                        Timer {
                            id: hold
                            interval: 450
                            onTriggered: {
                                if(!keyItem.down||keyItem.moved)return;
                                if(keyItem.letter && PlasmaKeyboardSettings.diacriticsPopupEnabled) {
                                    const variants=Keyboard.Laptop.alternatives(keyItem.caption);
                                    if(variants.length>0){alternatePopup.choices=variants;alternatePopup.open();keyItem.popupUsed=true;}
                                } else if(keyItem.code===14 || keyItem.code===57) {
                                    keyItem.sent=true;keyboard.emitKey(keyItem.code,true,keyItem.caption);
                                }
                            }
                        }
                        QQC2.Popup {
                            id: alternatePopup
                            property var choices: []
                            y: -height-keyboard.perMm
                            x: Math.max(-keyItem.x,Math.min(0,keyboard.width-keyItem.x-implicitWidth))
                            padding: keyboard.perMm
                            contentItem: Row {
                                spacing: keyboard.gap
                                Repeater {
                                    model: alternatePopup.choices
                                    delegate: QQC2.Button {
                                        required property string modelData
                                        text: modelData
                                        implicitWidth: Math.max(30,9*keyboard.perMm)
                                        implicitHeight: Math.max(30,10*keyboard.perMm)
                                        focusPolicy: Qt.NoFocus
                                        onClicked: { if(keyboard.localEditor)keyboard.localEditor.insert(keyboard.localEditor.cursorPosition,modelData);else if(keyboard.target)keyboard.target.commitText(modelData);alternatePopup.close(); }
                                    }
                                }
                            }
                        }
                        MultiPointTouchArea {
                            anchors.fill: parent
                            minimumTouchPoints: 1
                            maximumTouchPoints: 1
                            mouseEnabled: true
                            onGestureStarted: gesture => gesture.grab()
                            onPressed: points => {
                                const p=points[0];keyItem.down=true;keyItem.sent=false;keyItem.moved=false;keyItem.popupUsed=false;
                                keyboard.feedback();
                                keyItem.startX=p.x;keyItem.lastStep=0;keyItem.began=Date.now();
                                const q=keyItem.mapToItem(keyboard,p.x,p.y);keyItem.trace=[{x:q.x,y:q.y,t:0}];
                                if(!keyItem.deferred){keyItem.sent=true;keyboard.emitKey(keyItem.code,true,keyItem.caption);}
                                hold.restart();
                            }
                            onUpdated: points => {
                                if(!keyItem.down||points.length===0)return;
                                const p=points[0];const dx=(p.x-keyItem.startX)/keyboard.perMm;
                                if(keyItem.code===57 && Keyboard.Laptop.options.spaceGesture && Math.abs(dx)>2) {
                                    const step=Math.trunc(dx/2);Keyboard.Laptop.moveCursor(step-keyItem.lastStep);keyItem.lastStep=step;keyItem.moved=true;hold.stop();
                                } else if(keyItem.code===14 && Keyboard.Laptop.options.deleteGesture && dx < -3) {
                                    const step=Math.floor(-dx/5);if(step>keyItem.lastStep)Keyboard.Laptop.deleteWords(step-keyItem.lastStep);keyItem.lastStep=step;keyItem.moved=true;hold.stop();
                                } else if(keyItem.letter) {
                                    const q=keyItem.mapToItem(keyboard,p.x,p.y);keyItem.trace.push({x:q.x,y:q.y,t:Date.now()-keyItem.began});
                                    if(Math.abs(dx)>5||Math.abs(p.y)/keyboard.perMm>10){keyItem.moved=true;hold.stop();}
                                }
                            }
                            onReleased: {
                                hold.stop();
                                if(keyItem.sent)keyboard.emitKey(keyItem.code,false,keyItem.caption);
                                else if(!keyItem.popupUsed) {
                                    if(keyItem.letter && keyItem.moved && keyboard.auxiliaryAllowed && PlasmaKeyboardSettings.swipeTypingEnabled && keyboard.recognizer && keyboard.recognizer.supports(Keyboard.Laptop.locale)) {
                                        const words=keyboard.recognizer.recognize(Keyboard.Laptop.locale,keyItem.trace,keyboard.keyMap(),[],5);
                                        if(words.length>0){keyboard.target.commitSwipeWord(words[0]);keyboard.swipeAccepted(words);}
                                    } else if(!keyItem.moved)keyboard.tapKey(keyItem.code,keyItem.caption);
                                }
                                keyItem.down=false;
                            }
                            onCanceled: {hold.stop();if(keyItem.sent)keyboard.emitKey(keyItem.code,false,keyItem.caption);keyItem.down=false;}
                        }
                        Component.onDestruction: { if(sent&&down)keyboard.emitKey(code,false,caption); }
                    }
                }
            }
        }
    }
}
