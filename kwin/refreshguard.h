// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QVariantMap>
#include <functional>
// Temporary KScreen mode ownership survives input-method crashes. The journal
// permits recovery after a compositor restart without enabling an output.
class RefreshGuard final : public QObject {
public:
 explicit RefreshGuard(QObject *parent,std::function<void(const QString&)> error);
 void begin(const QString &name);
 bool set(const QString &profile);
 void end();
 void topologyChanged();
private:
 void process();
 void finish();
 void load();
 bool save();
 void clear();
 QString journal()const;
 std::function<void(const QString&)>m_error;
 QVariantMap m_saved;
 QString m_name,m_wanted=QStringLiteral("keep");
 bool m_active=false,m_busy=false,m_pending=false;
};
