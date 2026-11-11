// SPDX-License-Identifier: GPL-2.0-or-later
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const source=fs.readFileSync(new URL('../src/qml/LocaleResolver.js',import.meta.url),'utf8').replace(/^\.pragma library\s*$/m,'');
const context=vm.createContext({});vm.runInContext(source,context);
const available=['en_US','ar_AR','fa_FA','sr_SP','tr_TR','zh_TW','zh_CN','ja_JP'];
for(const [requested,expected] of [['tr_TR','tr_TR'],['ar_SA','ar_AR'],['fa_IR','fa_FA'],['sr_RS','sr_SP'],['zh-TW','zh_TW'],['ja_JP','ja_JP'],['unknown','en_US']])assert.equal(context.forIme(requested,available),expected);
assert.deepEqual(Array.from(context.forImes(['tr_TR','tr_TR','ar_SA'],available)),['tr_TR','ar_AR']);
assert.equal(context.forIme('tr_TR',[]),'en_US');
console.log('Installed IME locale aliases passed');
