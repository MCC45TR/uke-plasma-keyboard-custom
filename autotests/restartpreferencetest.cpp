// SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
// SPDX-License-Identifier: MIT
#include "mockinputmethodcompositor_config.h"
#include <QFile>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class RestartPreferenceTest : public QObject
{
    Q_OBJECT
    static void writeConfig(const QString &path,const QString &provider)
    {
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));
        const auto text=QStringLiteral("[Wayland]\nInputMethod=%1\n\n[Unrelated]\nKeep=true\n").arg(provider).toUtf8();
        QCOMPARE(file.write(text),text.size());
    }
    static QString provider(const QString &path)
    {
        QFile file(path);if(!file.open(QIODevice::ReadOnly))return QStringLiteral("missing-file");
        const QByteArray prefix("InputMethod=");
        for(const auto &line:file.readAll().split('\n'))if(line.startsWith(prefix))return QString::fromUtf8(line.mid(prefix.size())).trimmed();
        return {};
    }
    static void start(QProcess &process,const QString &config)
    {
        auto environment=QProcessEnvironment::systemEnvironment();environment.insert(QStringLiteral("XDG_CONFIG_HOME"),config);
        process.setProcessEnvironment(environment);process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(QStringLiteral(PLASMA_KEYBOARD_BINARY_PATH),{QStringLiteral("--restart-input-method")});
        QVERIFY(process.waitForStarted());
    }
private Q_SLOTS:
    void leavesAnUnrelatedSelectedKeyboardAlone()
    {
        QTemporaryDir config;QVERIFY(config.isValid());const auto path=config.filePath(QStringLiteral("kwinrc"));
        const auto other=QStringLiteral("org.kde.plasma.keyboard.desktop");writeConfig(path,other);
        QFileSystemWatcher watcher({path});QSignalSpy writes(&watcher,&QFileSystemWatcher::fileChanged);
        QProcess process;start(process,config.path());QTRY_COMPARE(process.state(),QProcess::NotRunning);
        QCOMPARE(process.exitCode(),0);QCOMPARE(provider(path),other);QCOMPARE(writes.count(),0);
    }
    void restoresOnlyItsOwnBlankState()
    {
        QTemporaryDir config;QVERIFY(config.isValid());const auto path=config.filePath(QStringLiteral("kwinrc"));
        const auto ours=QStringLiteral("org.uke.plasma.keyboard.desktop");writeConfig(path,ours);
        QProcess process;start(process,config.path());QTRY_COMPARE(process.state(),QProcess::NotRunning);
        QCOMPARE(process.exitCode(),0);QCOMPARE(provider(path),ours);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.readAll().contains("Keep=true"));
    }
    void keepsASelectionMadeDuringRestart()
    {
        QTemporaryDir config;QVERIFY(config.isValid());const auto path=config.filePath(QStringLiteral("kwinrc"));
        writeConfig(path,QStringLiteral("org.uke.plasma.keyboard.desktop"));
        QProcess process;start(process,config.path());QTRY_COMPARE(provider(path),QString());
        const auto other=QStringLiteral("org.kde.plasma.keyboard.desktop");
        QCOMPARE(QProcess::execute(QStringLiteral("kwriteconfig6"),{QStringLiteral("--file"),path,QStringLiteral("--group"),QStringLiteral("Wayland"),QStringLiteral("--key"),QStringLiteral("InputMethod"),other}),0);
        QTRY_COMPARE(process.state(),QProcess::NotRunning);QCOMPARE(process.exitCode(),0);QCOMPARE(provider(path),other);
    }
};

QTEST_GUILESS_MAIN(RestartPreferenceTest)
#include "restartpreferencetest.moc"
