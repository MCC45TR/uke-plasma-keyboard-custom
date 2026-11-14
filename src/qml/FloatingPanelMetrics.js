// SPDX-License-Identifier: GPL-2.0-or-later
.pragma library

function finite(value,fallback) { return Number.isFinite(value) ? value : fallback; }
function packedHeight(items,width,spacing) {
    width=Math.max(1,finite(width,1));spacing=Math.max(0,finite(spacing,0));
    let x=0,y=0,row=0;
    for(const item of items){
        const w=Math.min(width,Math.max(0,finite(item.width,0))),h=Math.max(0,finite(item.height,0));
        if(w===0||h===0)continue;
        if(x>0&&x+spacing+w>width){y+=row+spacing;x=0;row=0;}
        if(x>0)x+=spacing;
        x+=w;row=Math.max(row,h);
    }
    return y+row;
}
function fit(viewWidth,viewHeight,padding,requested,comfortable,aspect,tools,actions,spacing,extraChrome) {
    viewWidth=Math.max(1,finite(viewWidth,1));viewHeight=Math.max(1,finite(viewHeight,1));
    padding=Math.max(0,finite(padding,0));aspect=Math.max(0.1,finite(aspect,2.6));
    const available=Math.max(1,viewWidth-4*padding);
    comfortable=Math.min(available,Math.max(1,finite(comfortable,available)));
    // Use the most wrapped toolbar arrangement to bound taller, wider sizes.
    // This avoids a width/Flow-height binding cycle while retaining the keys.
    const chrome=packedHeight(tools,comfortable+2*padding,spacing)+packedHeight(actions,comfortable+2*padding,spacing)+Math.max(0,finite(extraChrome,0));
    const maximum=Math.max(1,Math.min(available,Math.max(1,viewHeight-4*padding-chrome)*aspect));
    const minimum=Math.min(comfortable,maximum);
    return {width:Math.max(minimum,Math.min(maximum,finite(requested,minimum))),minimum:minimum,maximum:maximum,chrome:chrome};
}
