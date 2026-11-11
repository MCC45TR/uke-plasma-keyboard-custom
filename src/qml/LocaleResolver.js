// SPDX-License-Identifier: GPL-2.0-or-later
.pragma library

// Qt's installed layout names may use historical territories (ar_AR, fa_FA,
// sr_SP) while the XKB registry and Hunspell use current locale identifiers.
function forIme(requested, available) {
    const wanted=String(requested).replace(/-/g,"_").toLowerCase();
    const languages=available||[];
    for(const locale of languages)if(locale.toLowerCase()===wanted)return locale;
    const language=wanted.split("_")[0];
    for(const locale of languages)if(locale.toLowerCase().split("_")[0]===language)return locale;
    return languages.includes("en_US")?"en_US":(languages[0]||"en_US");
}
function forImes(requested, available) {
    const result=[];
    for(const locale of requested){const resolved=forIme(locale,available);if(!result.includes(resolved))result.push(resolved);}
    return result;
}
