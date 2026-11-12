// SPDX-License-Identifier: MIT
#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include "mockinputmethodcompositor_config.h"
class QmlSmokeTest:public QObject {
 Q_OBJECT
private Q_SLOTS:
 void preferencesLoad_data(){
  QTest::addColumn<QString>("scene");
  QTest::newRow("settings")<<QString();
  QTest::newRow("keyboard-and-touchpad")<<QStringLiteral(LAPTOP_SMOKE_SCENE);
  QTest::newRow("toolbar-order-and-focus")<<QStringLiteral(TOOLBAR_SETTINGS_SCENE);
 }
 void preferencesLoad(){
  QFETCH(QString,scene);
  QTemporaryDir directory;QVERIFY(directory.isValid());
  QFile layouts(directory.path()+QStringLiteral("/kxkbrc"));QVERIFY(layouts.open(QIODevice::WriteOnly));
  layouts.write("[Layout]\nUse=true\nLayoutList=tr,tr,us,ara\nVariantList=,f,,\n");layouts.close();
  QProcess process;auto env=QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("XDG_CONFIG_HOME"),directory.path());
  env.insert(QStringLiteral("QT_QPA_PLATFORM"),QStringLiteral("offscreen"));
  env.insert(QStringLiteral("QT_QUICK_BACKEND"),QStringLiteral("software"));
  env.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"),QStringLiteral("1"));
  env.insert(QStringLiteral("PLASMA_KEYBOARD_TEST_MODE"),QStringLiteral("1"));
  env.insert(QStringLiteral("UKE_KEYBOARD_TEST_SCENE"),scene);
  process.setProcessEnvironment(env);process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(QStringLiteral(PLASMA_KEYBOARD_BINARY_PATH),{QStringLiteral("--preferences")});
  QVERIFY(process.waitForStarted());
  const bool exited=process.waitForFinished(3000);QByteArray output=process.readAll();
  if(exited&&process.exitStatus()==QProcess::CrashExit){
   QProcess trace;trace.setProcessEnvironment(env);trace.setProcessChannelMode(QProcess::MergedChannels);
   trace.start(QStringLiteral("gdb"),{QStringLiteral("-q"),QStringLiteral("-nx"),QStringLiteral("-batch"),QStringLiteral("-iex"),QStringLiteral("set auto-load off"),QStringLiteral("-iex"),QStringLiteral("set debuginfod enabled off"),QStringLiteral("-ex"),QStringLiteral("run"),QStringLiteral("-ex"),QStringLiteral("thread apply all bt 12"),QStringLiteral("--args"),QStringLiteral(PLASMA_KEYBOARD_BINARY_PATH),QStringLiteral("--preferences")});
   if(!trace.waitForFinished(20000)){trace.kill();trace.waitForFinished();}
   qWarning().noquote()<<QString::fromUtf8(trace.readAll());
  }
  const QString diagnosis=QStringLiteral("Exit code %1, status %2: %3").arg(process.exitCode()).arg(int(process.exitStatus())).arg(QString::fromUtf8(output));
  if(!exited){process.terminate();if(!process.waitForFinished(2000)){process.kill();process.waitForFinished();}output+=process.readAll();}
  QVERIFY2(!exited,qPrintable(diagnosis));
  QVERIFY2(!output.contains("failed to load component"),output.constData());
  QVERIFY2(!output.contains("is not a type"),output.constData());
  QVERIFY2(!output.contains("Cannot assign to non-existent"),output.constData());
  QVERIFY2(!output.contains("ReferenceError:"),output.constData());
  QVERIFY2(!output.contains("TypeError:"),output.constData());
  QVERIFY2(!output.contains("Binding loop detected"),output.constData());
  QVERIFY2(!output.contains("UKE_QML_FAILURE"),output.constData());
  if(!scene.isEmpty())QVERIFY2(output.contains("UKE_QML_GEOMETRY_PASS"),output.constData());
 }
};
QTEST_GUILESS_MAIN(QmlSmokeTest)
#include "qmlsmoketest.moc"
