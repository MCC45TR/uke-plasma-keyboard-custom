// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard
import org.uke.plasma.keyboard.lib as Keyboard
Kirigami.ApplicationWindow {
    id: window
    width: 1000
    height: 800
    visible: true
    QtObject {
        id: input
        property bool auxiliaryUiAllowed: true
        property var clipboardHistory: null
        signal predictionTargetChanged()
        function commitText(text) {}
        function applyPrediction(text) {}
        function commitSwipeWord(text) {}
        function pasteClipboardEntry(index) {}
        function forceActiveFocus() {}
    }
    pageStack.initialPage: Kirigami.Page {
        LaptopDeck {
            id: deck
            anchors.fill: parent
            inputTarget: input
            perMm: 3
        }
    }
    property int testStep: 0
    property real firstWidthMm: 0
    function findItem(item, name) {
        if(item.objectName===name)return item;
        for(const child of item.children||[]) {
            const found=findItem(child,name);
            if(found)return found;
        }
        return null;
    }
    function checkPad(pad, fraction, heightMm) {
        if(Math.abs(pad.width-pad.parent.width*fraction)>1)return "Touchpad width does not follow its setting";
        if(Math.abs(pad.height-Math.min(pad.parent.height,heightMm*deck.perMm))>1)return "Touchpad height does not follow its setting";
        if(Math.abs(pad.x-(pad.parent.width-pad.width)/2)>1)return "Touchpad is not centered";
        if(Math.abs(pad.y-(pad.parent.height-pad.height)/2)>1)return "Touchpad is not vertically centered";
        return "";
    }
    function advanceTest() {
        if(deck.border.width!==0)return "Laptop surface has an outer border";
        const key=deck.keyGeometry(30);
        if(!key)return "Missing A key";
        const pad=findItem(deck,"ukeTouchpad");
        if(!pad)return "Missing touchpad";
        const mm=key.width/deck.perMm;
        if(testStep===0){
            const failure=checkPad(pad,0.6,45);if(failure)return failure;
            Keyboard.Laptop.setOption("touchpadWidthPercent",40);
            Keyboard.Laptop.setOption("touchpadHeightMm",35);
        }else if(testStep===1){
            const failure=checkPad(pad,0.4,35);if(failure)return failure;
            Keyboard.Laptop.setOption("touchpadFullWidth",true);
        }else if(testStep===2){
            const failure=checkPad(pad,1,35);if(failure)return failure;
            Keyboard.Laptop.setOption("touchpadFullWidth",false);
            Keyboard.Laptop.setOption("touchpadWidthPercent",60);
            Keyboard.Laptop.setOption("touchpadHeightMm",45);
        }else if(testStep===3){
            firstWidthMm=mm;
            if(Math.abs(mm-14)>0.01)return "Physical target width: "+mm;
            window.width=Math.round(window.width/2.65);deck.perMm=3/2.65;
        }else if(testStep===4){
            if(Math.abs(mm-firstWidthMm)>0.01)return "Scale changed physical width: "+mm;
            window.width=400;window.height=1000;deck.perMm=4;
        }else{
            const failure=checkPad(pad,0.6,45);if(failure)return failure;
            if(mm<=0||mm>=14)return "Portrait did not fit: "+mm;
            if(key.x<0||key.x+key.width>key.viewport+0.1)return "Key outside viewport";
            if(!deck.toolbarFits)return "Toolbar outside viewport";
            if(!deck.toolbarPopupsFit)return "Toolbar popup outside viewport";
            Keyboard.Laptop.selectLayout(0);
            if(Keyboard.Laptop.keyLabel(16,"")!=="q"||Keyboard.Laptop.keyLabel(23,"")!=="ı")return "Turkish Q labels";
            Keyboard.Laptop.selectLayout(1);
            if(Keyboard.Laptop.keyLabel(16,"")!=="f")return "Turkish F variant";
            Keyboard.Laptop.selectLayout(2);
            if(Keyboard.Laptop.keyLabel(16,"")!=="q")return "English label";
            Keyboard.Laptop.selectLayout(3);
            if(Keyboard.Laptop.keyLabel(16,"")!=="ض")return "Arabic label";
            return "PASS";
        }
        ++testStep;
        return "WAIT";
    }
}
