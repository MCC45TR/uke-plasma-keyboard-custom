// SPDX-License-Identifier: GPL-2.0-or-later
#include "layoutregistry.h"
#include <QFile>
#include <QXmlStreamReader>
namespace {
QVariantMap configItem(QXmlStreamReader &xml) {
    QVariantMap item;
    QStringList languages,countries;
    while(!xml.atEnd()) {
        xml.readNext();
        if(xml.isEndElement()&&xml.name()==QLatin1String("configItem"))break;
        if(!xml.isStartElement())continue;
        const auto name=xml.name().toString();
        if(name==QLatin1String("name"))item.insert(QStringLiteral("code"),xml.readElementText());
        else if(name==QLatin1String("description"))item.insert(QStringLiteral("label"),xml.readElementText());
        else if(name==QLatin1String("shortDescription"))item.insert(QStringLiteral("short"),xml.readElementText());
        else if(name==QLatin1String("iso639Id"))languages.append(xml.readElementText());
        else if(name==QLatin1String("iso3166Id"))countries.append(xml.readElementText());
    }
    if(!languages.isEmpty())item.insert(QStringLiteral("languages"),languages);
    if(!countries.isEmpty())item.insert(QStringLiteral("countries"),countries);
    return item;
}
QLocale::Language language(const QVariantMap &item,QLocale preferred) {
    const auto codes=item.value(QStringLiteral("languages")).toStringList();
    auto first=QLocale::AnyLanguage;
    for(const auto &code:codes){
        const auto candidate=QLocale::codeToLanguage(code);
        if(candidate==preferred.language())return candidate;
        if(first==QLocale::AnyLanguage&&candidate!=QLocale::AnyLanguage)first=candidate;
    }
    const auto shortLanguage=QLocale::codeToLanguage(item.value(QStringLiteral("short")).toString());
    if(shortLanguage!=QLocale::AnyLanguage)for(const auto &code:codes)if(QLocale::codeToLanguage(code)==shortLanguage)return shortLanguage;
    return first;
}
}
namespace UkeKeyboard {
LayoutRegistry::LayoutRegistry(QIODevice *device) {
    QXmlStreamReader xml(device);
    QVariantMap layout;
    QVariantList variants;
    bool inLayout=false,inVariant=false;
    while(!xml.atEnd()){
        xml.readNext();
        if(xml.isStartElement()){
            if(xml.name()==QLatin1String("layout")){inLayout=true;layout.clear();variants.clear();}
            else if(inLayout&&xml.name()==QLatin1String("variant"))inVariant=true;
            else if(inLayout&&xml.name()==QLatin1String("configItem")){const auto item=configItem(xml);if(inVariant)variants.append(item);else layout=item;}
        }else if(xml.isEndElement()){
            if(xml.name()==QLatin1String("variant"))inVariant=false;
            else if(xml.name()==QLatin1String("layout")){layout.insert(QStringLiteral("variants"),variants);if(!layout.value(QStringLiteral("code")).toString().isEmpty())m_layouts.append(layout);inLayout=false;}
        }
    }
    if(xml.hasError())m_layouts.clear();
}
const LayoutRegistry &LayoutRegistry::system(){
    static const LayoutRegistry registry=[] {
        QFile file(QStringLiteral("/usr/share/X11/xkb/rules/evdev.xml"));
        if(!file.open(QIODevice::ReadOnly)){file.setFileName(QStringLiteral("/usr/share/X11/xkb/rules/base.xml"));file.open(QIODevice::ReadOnly);}
        return LayoutRegistry(&file);
    }();
    return registry;
}
QVariantList LayoutRegistry::layouts()const{return m_layouts;}
QVariantList LayoutRegistry::variants(const QString &layout)const{return entry(layout,{}).value(QStringLiteral("variants")).toList();}
QVariantMap LayoutRegistry::entry(const QString &code,const QString &variant)const{
    for(const auto &value:m_layouts){auto item=value.toMap();if(item.value(QStringLiteral("code"))!=code)continue;
        if(!variant.isEmpty())for(const auto &value:item.value(QStringLiteral("variants")).toList()){
            const auto override=value.toMap();if(override.value(QStringLiteral("code"))==variant){for(auto i=override.begin();i!=override.end();++i)item.insert(i.key(),i.value());break;}
        }
        return item;
    }
    return {};
}
QString LayoutRegistry::locale(const QString &code,const QString &variant,QLocale preferred)const{
    const auto item=entry(code,variant);const auto lang=language(item,preferred);
    if(lang==QLocale::AnyLanguage||lang==QLocale::C)return QStringLiteral("en_US");
    auto territory=QLocale::AnyTerritory;const auto countries=item.value(QStringLiteral("countries")).toStringList();
    for(const auto &country:countries){const auto candidate=QLocale::codeToTerritory(country);if(territory==QLocale::AnyTerritory)territory=candidate;if(candidate==preferred.territory()){territory=candidate;break;}}
    return QLocale(lang,territory).name();
}
QString LayoutRegistry::defaultLayout(QLocale preferred)const{
    QString fallback;
    for(const auto &value:m_layouts){const auto item=value.toMap();if(language(item,preferred)!=preferred.language())continue;
        const auto code=item.value(QStringLiteral("code")).toString();if(fallback.isEmpty())fallback=code;
        for(const auto &country:item.value(QStringLiteral("countries")).toStringList())if(QLocale::codeToTerritory(country)==preferred.territory())return code;
    }
    return fallback.isEmpty()?QStringLiteral("us"):fallback;
}
}
