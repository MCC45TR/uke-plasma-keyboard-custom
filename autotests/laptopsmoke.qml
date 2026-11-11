// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigami as Kirigami
import org.uke.plasma.keyboard
Kirigami.ApplicationWindow {
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
            anchors.fill: parent
            inputTarget: input
            perMm: 3
        }
    }
}
