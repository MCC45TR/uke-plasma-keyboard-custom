// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import org.uke.plasma.keyboard
import org.uke.plasma.keyboard.lib as Keyboard

LaptopSettings {
    id: window
    property int testStep: 0
    function findItem(item,name) {
        if(item.objectName===name)return item;
        for(const child of item.children||[]){const found=findItem(child,name);if(found)return found;}
        return null;
    }
    function advanceTest() {
        const earlier=findItem(window.contentItem,"moveEarlier_emoji");
        if(!earlier)return "Missing toolbar move action";
        if(!earlier.Accessible.name.includes(Keyboard.Laptop.toolLabel("emoji")))return "Move action lacks its tool name";
        if(testStep===0){window.requestActivate();earlier.forceActiveFocus();++testStep;return "WAIT";}
        if(testStep===1){if(!earlier.activeFocus)return "WAIT";earlier.clicked();++testStep;return "WAIT";}
        const emoji=findItem(window.contentItem,"toolbarSettings_emoji");
        const clipboard=findItem(window.contentItem,"toolbarSettings_clipboard");
        if(!emoji||!clipboard)return "Missing reordered toolbar rows";
        if(Keyboard.Laptop.options.toolbar.indexOf("emoji")!==3)return "Toolbar preference did not move";
        if(emoji.y>=clipboard.y)return "Settings rows do not match toolbar order";
        if(!earlier.activeFocus)return "Reordered action lost keyboard focus";
        return "PASS";
    }
}
