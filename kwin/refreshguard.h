// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QList>
#include <QSize>
#include <QVariantMap>
#include <functional>
#include <optional>
// Temporary KScreen mode ownership survives input-method crashes. The journal
// permits recovery after a compositor restart without enabling an output.
class RefreshGuard final : public QObject {
public:
 struct Mode {
  QSize size;
  int milliHz=0;
  bool operator==(const Mode&)const=default;
 };
 struct Snapshot {
  Mode current;
  QList<Mode> modes;
  int policy=0;
 };
 struct Backend {
  std::function<std::optional<Snapshot>(const QString&)> read;
  std::function<bool(const QString&,const Mode&,int)> apply;
 };
 explicit RefreshGuard(QObject *parent,std::function<void(const QString&)> error,Backend backend);
 void begin(const QString &name);
 bool set(const QString &profile);
 void end();
 void topologyChanged();
private:
 void process();
 void step();
 bool needsRestoration()const;
 void load();
 bool save();
 void clear();
 QString journal()const;
 std::function<void(const QString&)>m_error;
 Backend m_backend;
 QVariantMap m_saved;
 QString m_name,m_wanted=QStringLiteral("keep");
 bool m_active=false,m_busy=false,m_pending=false;
};
