// SPDX-License-Identifier: MIT
#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include "mockinputmethodcompositor_config.h"
class QmlSmokeTest:public QObject {
 Q_OBJECT
private Q_SLOTS:
 void preferencesLoad_data(){QTest::addColumn<QString>("scene");QTest::newRow("settings")<<QString();QTest::newRow("keyboard-and-touchpad")<<QStringLiteral(LAPTOP_SMOKE_SCENE);}
 void preferencesLoad(){
  QFETCH(QString,scene);
  QTemporaryDir directory;QVERIFY(directory.isValid());
  QProcess process;auto env=QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("XDG_CONFIG_HOME"),directory.path());
  env.insert(QStringLiteral("QT_QPA_PLATFORM"),QStringLiteral("offscreen"));
  env.insert(QStringLiteral("QT_QUICK_BACKEND"),QStringLiteral("software"));
  env.insert(QStringLiteral("PLASMA_KEYBOARD_TEST_MODE"),QStringLiteral("1"));
  env.insert(QStringLiteral("UKE_KEYBOARD_TEST_SCENE"),scene);
  process.setProcessEnvironment(env);process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(QStringLiteral(PLASMA_KEYBOARD_BINARY_PATH),{QStringLiteral("--preferences")});
  QVERIFY(process.waitForStarted());
  const bool exited=process.waitForFinished(2000);const QByteArray output=process.readAll();
  QVERIFY2(!exited,output.constData());
  QVERIFY2(!output.contains("failed to load component"),output.constData());
  QVERIFY2(!output.contains("is not a type"),output.constData());
  QVERIFY2(!output.contains("Cannot assign to non-existent"),output.constData());
  QVERIFY2(!output.contains("ReferenceError:"),output.constData());
  QVERIFY2(!output.contains("TypeError:"),output.constData());
  process.terminate();if(!process.waitForFinished(2000)){process.kill();process.waitForFinished();}
 }
};
QTEST_GUILESS_MAIN(QmlSmokeTest)
#include "qmlsmoketest.moc"
