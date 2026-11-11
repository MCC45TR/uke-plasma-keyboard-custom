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
    function findKey(item, name) {
        if(item.objectName===name)return item;
        for(const child of item.children||[]){const found=findKey(child,name);if(found)return found;}
        return null;
    }
    Timer {
        interval: 150
        running: true
        repeat: true
        property int step: 0
        property real firstWidthMm: 0
        onTriggered: {
            const key=window.findKey(deck,"ukeKey_30");
            if(!key){console.error("UKE_QML_FAILURE: missing A key");stop();return;}
            const mm=key.width/deck.perMm;
            if(step===0){
                firstWidthMm=mm;
                if(Math.abs(mm-14)>0.01)console.error("UKE_QML_FAILURE: physical target width",mm);
                window.width=Math.round(window.width/2.65);deck.perMm=3/2.65;
            }else if(step===1){
                if(Math.abs(mm-firstWidthMm)>0.01)console.error("UKE_QML_FAILURE: scale changed physical width",mm);
                window.width=400;window.height=1000;deck.perMm=4;
            }else{
                if(mm<=0||mm>=14)console.error("UKE_QML_FAILURE: portrait did not fit",mm);
                const point=key.mapToItem(deck,0,0);
                if(point.x<0||point.x+key.width>deck.width+0.1)console.error("UKE_QML_FAILURE: key outside viewport");
                const toolbar=window.findKey(deck,"ukeToolbar");
                if(!toolbar||toolbar.childrenRect.width>toolbar.width+0.1)console.error("UKE_QML_FAILURE: toolbar outside viewport");
                Keyboard.Laptop.selectLayout(0);
                if(Keyboard.Laptop.keyLabel(16,"")!=="q"||Keyboard.Laptop.keyLabel(23,"")!=="ı")console.error("UKE_QML_FAILURE: Turkish Q labels");
                Keyboard.Laptop.selectLayout(1);
                if(Keyboard.Laptop.keyLabel(16,"")!=="f")console.error("UKE_QML_FAILURE: Turkish F variant");
                Keyboard.Laptop.selectLayout(2);
                if(Keyboard.Laptop.keyLabel(16,"")!=="q")console.error("UKE_QML_FAILURE: English label");
                Keyboard.Laptop.selectLayout(3);
                if(Keyboard.Laptop.keyLabel(16,"")!=="ض")console.error("UKE_QML_FAILURE: Arabic label");
                console.log("UKE_QML_GEOMETRY_PASS");stop();
            }
            ++step;
        }
    }
}
