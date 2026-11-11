// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard.lib as Keyboard
ColumnLayout {
    id: root
    property real perMm: Keyboard.Laptop.pixelsPerMm
    spacing: perMm
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 2*root.perMm
        color: Kirigami.Theme.alternateBackgroundColor
        border.color: Kirigami.Theme.disabledTextColor
        border.width: 1
        Accessible.role: Accessible.Pane
        Accessible.name: i18nd("uke-plasma-keyboard-custom", "Touchpad")
        QQC2.Label { anchors.centerIn: parent; text: i18nd("uke-plasma-keyboard-custom", "Touchpad"); opacity: 0.35; visible: !touch.sequence; }
        MultiPointTouchArea {
            id: touch
            anchors.fill: parent
            mouseEnabled: false
            minimumTouchPoints: 1
            maximumTouchPoints: 5
            touchPoints: [TouchPoint{id:a},TouchPoint{id:b},TouchPoint{id:c},TouchPoint{id:d},TouchPoint{id:e}]
            property bool sequence: false
            property bool blocked: false
            property bool dragged: false
            property bool moved: false
            property int fingers: 0
            property int maxFingers: 0
            property string gesture: ""
            property var previous: ({x:0,y:0,distance:0,angle:0})
            property var origin: ({x:0,y:0,distance:0,angle:0})
            property real started: 0
            property real lastTap: 0
            function snapshot() {
                const points=[a,b,c,d,e].filter(p=>p.pressed);
                if(points.length===0)return {n:0,x:0,y:0,distance:0,angle:0};
                let x=0,y=0;for(const p of points){x+=p.x;y+=p.y;}
                const dx=points.length>1?points[1].x-points[0].x:0;
                const dy=points.length>1?points[1].y-points[0].y:0;
                return {n:points.length,x:x/points.length,y:y/points.length,distance:Math.hypot(dx,dy),angle:Math.atan2(dy,dx)*180/Math.PI};
            }
            function stopGesture(cancel) {
                if(gesture==="swipe")Keyboard.Laptop.swipe(fingers,cancel?3:2,0,0);
                if(gesture==="pinch")Keyboard.Laptop.pinch(cancel?3:2,1,0,0,0);
                if(gesture==="scroll")Keyboard.Laptop.scroll(0,0);
                gesture="";
            }
            function update() {
                const p=snapshot();
                if(p.n===0) {
                    if(!sequence)return;
                    if(dragged)Keyboard.Laptop.button(272,false);
                    else if(!blocked&&!moved&&Date.now()-started<300&&maxFingers<=2){const button=maxFingers===2?273:272;Keyboard.Laptop.button(button,true);Keyboard.Laptop.button(button,false);lastTap=maxFingers===1?Date.now():0;}
                    stopGesture(false);sequence=false;fingers=0;return;
                }
                if(!sequence) {
                    sequence=true;started=Date.now();maxFingers=p.n;moved=false;gesture="";dragged=false;
                    blocked=Keyboard.Laptop.options.palmGuard&&Keyboard.Laptop.typing&&!Keyboard.Laptop.modifierActive;
                    if(!blocked&&p.n===1&&Date.now()-lastTap<300){Keyboard.Laptop.button(272,true);dragged=true;}
                    fingers=p.n;origin=p;previous=p;return;
                }
                maxFingers=Math.max(maxFingers,p.n);
                if(p.n!==fingers){stopGesture(true);fingers=p.n;origin=p;previous=p;return;}
                if(blocked)return;
                const dx=p.x-previous.x,dy=p.y-previous.y;
                if(Math.hypot(p.x-origin.x,p.y-origin.y)>1.5*root.perMm)moved=true;
                if(p.n===1)Keyboard.Laptop.motion(dx/root.perMm*8,dy/root.perMm*8);
                else if(p.n===2) {
                    const ratio=origin.distance>0?p.distance/origin.distance:1;
                    if(gesture===""&&(Math.abs(ratio-1)>0.06||Math.abs(p.angle-origin.angle)>7)){gesture="pinch";moved=true;Keyboard.Laptop.pinch(0,1,0,0,0);}
                    if(gesture==="pinch")Keyboard.Laptop.pinch(1,ratio,p.angle-previous.angle,dx,dy);
                    else if(moved){gesture="scroll";Keyboard.Laptop.scroll(dx,dy);}
                } else if(p.n<=4 && moved) {
                    if(gesture!=="swipe"){gesture="swipe";Keyboard.Laptop.swipe(p.n,0,0,0);}
                    Keyboard.Laptop.swipe(p.n,1,dx,dy);
                }
                previous=p;
            }
            onPressed: update()
            onUpdated: update()
            onReleased: update()
            onCanceled: {stopGesture(true);if(dragged)Keyboard.Laptop.button(272,false);sequence=false;fingers=0;}
            onGestureStarted: gesture => gesture.grab()
            Component.onDestruction: {stopGesture(true);if(dragged)Keyboard.Laptop.button(272,false);}
        }
    }
    RowLayout {
        Layout.fillWidth: true
        visible: Keyboard.Laptop.options.leftButton||Keyboard.Laptop.options.middleButton||Keyboard.Laptop.options.rightButton
        Repeater {
            model: [{option:"leftButton",label:i18nd("uke-plasma-keyboard-custom", "Left"),code:272},{option:"middleButton",label:i18nd("uke-plasma-keyboard-custom", "Middle"),code:274},{option:"rightButton",label:i18nd("uke-plasma-keyboard-custom", "Right"),code:273}]
            delegate: QQC2.Button {
                required property var modelData
                visible: Keyboard.Laptop.options[modelData.option]
                Layout.fillWidth: true
                Layout.preferredHeight: root.perMm*9
                text: modelData.label
                focusPolicy: Qt.NoFocus
                onPressed: Keyboard.Laptop.button(modelData.code,true)
                onReleased: Keyboard.Laptop.button(modelData.code,false)
                onCanceled: Keyboard.Laptop.button(modelData.code,false)
            }
        }
    }
}
