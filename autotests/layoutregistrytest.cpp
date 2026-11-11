// SPDX-License-Identifier: MIT
#include "layoutregistry.h"
#include <QBuffer>
#include <QtTest>
class LayoutRegistryTest:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void languageMetadata_data(){
        QTest::addColumn<QString>("layout");QTest::addColumn<QString>("locale");
        QTest::newRow("Brazil-not-Breton")<<QStringLiteral("br")<<QStringLiteral("pt_BR");
        QTest::newRow("Slovenia-not-Sinhala")<<QStringLiteral("si")<<QStringLiteral("sl_SI");
        QTest::newRow("Serbia")<<QStringLiteral("rs")<<QStringLiteral("sr_RS");
        QTest::newRow("Turkey")<<QStringLiteral("tr")<<QStringLiteral("tr_TR");
    }
    void languageMetadata(){
        QFETCH(QString,layout);QFETCH(QString,locale);
        QCOMPARE(UkeKeyboard::LayoutRegistry::system().locale(layout,{},QLocale(QStringLiteral("en_US"))),locale);
    }
    void multilingualAndVariants(){
        const auto &registry=UkeKeyboard::LayoutRegistry::system();
        QCOMPARE(QLocale(registry.locale(QStringLiteral("be"),{},QLocale(QStringLiteral("fr_BE")))).language(),QLocale::French);
        QCOMPARE(registry.locale(QStringLiteral("tr"),QStringLiteral("f")),QStringLiteral("tr_TR"));
        QCOMPARE(registry.defaultLayout(QLocale(QStringLiteral("pt_BR"))),QStringLiteral("br"));
        QVERIFY(registry.layouts().size()>50);
        QVERIFY(!registry.variants(QStringLiteral("tr")).isEmpty());
    }
    void variantOverridesAndMalformed(){
        QBuffer input;input.setData(R"(<xkbConfigRegistry><layoutList><layout><configItem><name>test</name><description>Test</description><countryList><iso3166Id>US</iso3166Id></countryList><languageList><iso639Id>eng</iso639Id></languageList></configItem><variantList><variant><configItem><name>russian</name><description>Russian variant</description><languageList><iso639Id>rus</iso639Id></languageList><countryList><iso3166Id>RU</iso3166Id></countryList></configItem></variant></variantList></layout></layoutList></xkbConfigRegistry>)");
        QVERIFY(input.open(QIODevice::ReadOnly));UkeKeyboard::LayoutRegistry registry(&input);
        QCOMPARE(registry.locale(QStringLiteral("test")),QStringLiteral("en_US"));
        QCOMPARE(registry.locale(QStringLiteral("test"),QStringLiteral("russian")),QStringLiteral("ru_RU"));
        QBuffer malformed;malformed.setData("<xkbConfigRegistry><layout><configItem><name>tr</name>");malformed.open(QIODevice::ReadOnly);
        QVERIFY(UkeKeyboard::LayoutRegistry(&malformed).layouts().isEmpty());
    }
};
QTEST_GUILESS_MAIN(LayoutRegistryTest)
#include "layoutregistrytest.moc"
