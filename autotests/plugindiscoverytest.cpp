// SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
// SPDX-License-Identifier: MIT
#include "protocol.h"
#include <KPluginMetaData>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonObject>
#include <QtTest>

class PluginDiscoveryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void stagedModuleMatchesTheCompositorLookup()
    {
        const auto root=qEnvironmentVariable("UKE_KEYBOARD_PLUGIN_ROOT");
        QVERIFY2(!root.isEmpty(),"The staged RPM plugin root must be provided");
        QCoreApplication::addLibraryPath(root);
        const auto id=QString::fromLatin1(UkeKeyboard::pluginId);
        const auto metadata=KPluginMetaData::findPluginById(QStringLiteral("kwin/plugins"),id);
        QVERIFY(metadata.isValid());
        QCOMPARE(metadata.pluginId(),id);
        QCOMPARE(QFileInfo(metadata.fileName()).canonicalFilePath(),QFileInfo(root+QStringLiteral("/kwin/plugins/")+id+QStringLiteral(".so")).canonicalFilePath());
        QVERIFY(!metadata.rawData().value(QStringLiteral("KPlugin")).toObject().value(QStringLiteral("EnabledByDefault")).toBool());
    }
};

QTEST_GUILESS_MAIN(PluginDiscoveryTest)
#include "plugindiscoverytest.moc"
