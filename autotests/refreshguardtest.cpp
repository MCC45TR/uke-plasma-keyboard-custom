// SPDX-License-Identifier: MIT
#include "refreshguard.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace {
const QString panel=QStringLiteral("Synthetic-Panel");
const QString second=QStringLiteral("Second-Panel");
RefreshGuard::Mode mode(int milliHz){return {QSize(3200,2136),milliHz};}
struct FakeOutputs {
 QHash<QString,RefreshGuard::Snapshot> states;
 QStringList errors,appliedNames;
 QList<int> appliedRates;
 bool connected=true;
 int failRate=0;
 std::function<void(int)> afterApply;
 FakeOutputs(){states.insert(panel,{mode(90000),{mode(30000),mode(59940),mode(60000),mode(90000)},2});}
 RefreshGuard::Backend backend(){return {
  [this](const QString &name)->std::optional<RefreshGuard::Snapshot>{
   if(!connected||!states.contains(name))return std::nullopt;return states.value(name);
  },
  [this](const QString &name,const RefreshGuard::Mode &next,int policy){
   appliedNames.append(name);appliedRates.append(next.milliHz);
   if(!connected||!states.contains(name)||next.milliHz==failRate)return false;
   states[name].current=next;states[name].policy=policy;
   if(afterApply)afterApply(next.milliHz);return true;
  }
 };}
 auto error(){return [this](const QString &reason){errors.append(reason);};}
};
}

class RefreshGuardTest:public QObject {
 Q_OBJECT
 std::unique_ptr<QTemporaryDir> runtime;
 QByteArray oldRuntime;
 FakeOutputs outputs;
 QString journal()const{return runtime->filePath(QStringLiteral("uke-keyboard-refresh.json"));}
 QJsonObject record()const{QFile f(journal());if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
private Q_SLOTS:
 void init(){
  oldRuntime=qgetenv("XDG_RUNTIME_DIR");runtime=std::make_unique<QTemporaryDir>();QVERIFY(runtime->isValid());
  qputenv("XDG_RUNTIME_DIR",runtime->path().toUtf8());outputs=FakeOutputs{};
 }
 void cleanup(){qputenv("XDG_RUNTIME_DIR",oldRuntime);runtime.reset();}
 void fixedModeRestoresItsOriginalPolicy(){
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);QVERIFY(guard.set(QStringLiteral("30")));
  QCOMPARE(outputs.states[panel].current.milliHz,30000);QCOMPARE(outputs.states[panel].policy,0);
  QCOMPARE(record().value(QStringLiteral("hz")).toDouble(),90.0);
  QCOMPARE(QFileInfo(journal()).permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther),QFile::Permissions{});
  guard.end();QCOMPARE(outputs.states[panel].current.milliHz,90000);QCOMPARE(outputs.states[panel].policy,2);QVERIFY(!QFile::exists(journal()));
 }
 void failedSwitchKeepsTheLastOwnedRecoveryRecord(){
  {
   RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));
   outputs.failRate=60000;guard.set(QStringLiteral("60"));
   QCOMPARE(outputs.states[panel].current.milliHz,30000);
   QCOMPARE(record().value(QStringLiteral("appliedHz")).toDouble(),30.0);
   QCOMPARE(record().value(QStringLiteral("hz")).toDouble(),90.0);QVERIFY(!outputs.errors.isEmpty());
  }
  outputs.failRate=0;
  RefreshGuard recovered(nullptr,outputs.error(),outputs.backend());
  QCOMPARE(outputs.states[panel].current.milliHz,90000);QCOMPARE(outputs.states[panel].policy,2);QVERIFY(!QFile::exists(journal()));
 }
 void aNearbyUserSelectedModeIsNotOwned(){
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("60"));
  QCOMPARE(outputs.states[panel].current.milliHz,60000);
  outputs.states[panel].current=mode(59940);guard.end();
  QCOMPARE(outputs.states[panel].current.milliHz,59940);QCOMPARE(outputs.appliedRates.size(),1);QVERIFY(!QFile::exists(journal()));
 }
 void anInterveningVrrPolicyIsPreserved(){
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));
  outputs.states[panel].policy=1;guard.end();
  QCOMPARE(outputs.states[panel].current.milliHz,30000);QCOMPARE(outputs.states[panel].policy,1);QCOMPARE(outputs.appliedRates.size(),1);QVERIFY(!QFile::exists(journal()));
 }
 void aFailedFirstApplyOwnsNothing(){
  outputs.failRate=30000;RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));
  QCOMPARE(outputs.states[panel].current.milliHz,90000);QVERIFY(!QFile::exists(journal()));guard.end();QCOMPARE(outputs.appliedRates.size(),1);
 }
 void recoveryWaitsForAnAlreadyEnabledPanel(){
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));
  outputs.connected=false;guard.end();QVERIFY(QFile::exists(journal()));QCOMPARE(outputs.appliedRates.size(),1);
  outputs.connected=true;guard.topologyChanged();QCOMPARE(outputs.states[panel].current.milliHz,90000);QVERIFY(!QFile::exists(journal()));
 }
 void anExistingFixedPreferenceIsNotClaimed(){
  outputs.states[panel].current=mode(30000);outputs.states[panel].policy=0;
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));guard.end();
  QVERIFY(outputs.appliedRates.isEmpty());QVERIFY(!QFile::exists(journal()));
 }
 void profilesNeverChangeResolution(){
  outputs.states[panel].modes={{{1920,1080},60000}};
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("60"));
  QVERIFY(outputs.appliedRates.isEmpty());QVERIFY(!outputs.errors.isEmpty());QVERIFY(!QFile::exists(journal()));
 }
 void insecureCrashStateIsNotLoaded(){
  {RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));}
  QVERIFY(QFile::setPermissions(journal(),QFile::ReadOwner|QFile::WriteOwner|QFile::ReadOther));
  RefreshGuard recovered(nullptr,outputs.error(),outputs.backend());
  QCOMPARE(outputs.states[panel].current.milliHz,30000);QCOMPARE(outputs.appliedRates.size(),1);
 }
 void aSymlinkIsNotLoadedForRecovery(){
  {RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));}
  const auto target=runtime->filePath(QStringLiteral("other.json"));QVERIFY(QFile::rename(journal(),target));QVERIFY(QFile::link(target,journal()));
  RefreshGuard recovered(nullptr,outputs.error(),outputs.backend());
  QCOMPARE(outputs.states[panel].current.milliHz,30000);QCOMPARE(outputs.appliedRates.size(),1);
 }
 void aWithdrawnOriginalModeIsRetriedOnTopology(){
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));
  outputs.states[panel].modes.removeAll(mode(90000));guard.end();QVERIFY(QFile::exists(journal()));QCOMPARE(outputs.appliedRates.size(),1);
  outputs.states[panel].modes.append(mode(90000));guard.topologyChanged();QCOMPARE(outputs.states[panel].current.milliHz,90000);QVERIFY(!QFile::exists(journal()));
 }
 void onePanelsOwnershipIsRestoredBeforeAnotherIsChanged(){
  outputs.states.insert(second,outputs.states[panel]);
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));
  guard.begin(second);guard.set(QStringLiteral("60"));guard.end();
  QCOMPARE(outputs.appliedNames,QStringList({panel,panel,second,second}));
  QCOMPARE(outputs.appliedRates,QList<int>({30000,90000,60000,90000}));
  QCOMPARE(outputs.states[panel].policy,2);QCOMPARE(outputs.states[second].policy,2);
 }
 void aReentrantExitRestoresAfterTheSynchronousApply(){
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());
  outputs.afterApply=[&guard](int rate){if(rate==30000)guard.end();};
  guard.begin(panel);guard.set(QStringLiteral("30"));
  QCOMPARE(outputs.appliedRates,QList<int>({30000,90000}));QVERIFY(!QFile::exists(journal()));
 }
 void aJournalWriteFailureDoesNotApplyAMode(){
  QVERIFY(QDir().mkdir(journal()));
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());guard.begin(panel);guard.set(QStringLiteral("30"));
  QVERIFY(outputs.appliedRates.isEmpty());QVERIFY(!outputs.errors.isEmpty());QCOMPARE(outputs.states[panel].current.milliHz,90000);
 }
 void adaptiveAndInactiveRequestsCannotApply(){
  RefreshGuard guard(nullptr,outputs.error(),outputs.backend());QVERIFY(!guard.set(QStringLiteral("30")));
  guard.begin(panel);QVERIFY(!guard.set(QStringLiteral("adaptive")));QVERIFY(!guard.set(QStringLiteral("invalid")));
  QVERIFY(outputs.appliedRates.isEmpty());QVERIFY(!QFile::exists(journal()));
 }
};
QTEST_GUILESS_MAIN(RefreshGuardTest)
#include "refreshguardtest.moc"
