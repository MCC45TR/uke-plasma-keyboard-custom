// SPDX-License-Identifier: GPL-2.0-or-later
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source=fs.readFileSync(new URL('../src/qml/FloatingPanelMetrics.js',import.meta.url),'utf8');
const context=vm.createContext({});vm.runInContext(source.replace('.pragma library',''),context);
assert.equal(context.packedHeight([{width:90,height:20},{width:90,height:20},{width:90,height:20}],200,5),45);
assert.equal(context.packedHeight([{width:500,height:20}],100,5),20);
assert.equal(context.packedHeight([],100,5),0);
const tools=Array.from({length:7},()=>({width:60,height:34}));
const actions=Array.from({length:6},()=>({width:75,height:34}));
for(const [width,height] of [[800,600],[600,400],[1920,1080],[800,1280]]){
    for(const requested of [1,200,width*0.8,10000]){
        const result=context.fit(width,height,12,requested,340,2.6,tools,actions,4,42);
        assert(result.width>=result.minimum&&result.width<=result.maximum);
        assert(result.width+48<=width);
        assert(result.width/2.6+result.chrome+48<=height+0.001);
        assert.equal(context.fit(width,height,12,1,340,2.6,tools,actions,4,42).width,result.minimum);
    }
}
const wide=context.fit(1920,1080,12,1,800,2.6,tools,actions,4,42);
assert.equal(wide.minimum,800,'PC keys retain the requested comfortable minimum');
const invalid=context.fit(NaN,Infinity,-5,NaN,NaN,NaN,[],[],NaN,NaN);
assert(Number.isFinite(invalid.width)&&invalid.width>0);
console.log('Floating panel production metrics: wrapping, comfortable bounds, screen fit and invalid data passed');
