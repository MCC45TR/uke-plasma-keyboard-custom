// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <memory>
#include <optional>

class QDBusMessage;
class QDBusServiceWatcher;

// A demand-leased, unprivileged client. Hardware ownership and sample freshness
// belong to the existing Control provider; there is no device access or poller.
class TabletPostureMonitor final : public QObject {
    Q_OBJECT
public:
    explicit TabletPostureMonitor(QObject *parent = nullptr);
    ~TabletPostureMonitor() override;
    void setRequested(bool requested);
    bool requested() const { return m_requested; }

Q_SIGNALS:
    void observation(bool known, double inclination);

private Q_SLOTS:
    void preparingSleep(bool sleeping);

private:
    struct Sample { bool known; double angle; qulonglong timestamp; };
    void openConnection(const QString &owner = {});
    void closeConnection();
    void resolveOwner(quint64 generation, bool activate);
    void claim(const QString &owner, quint64 generation);
    void accept(const Sample &sample);
    void invalidate();
    QDBusConnection m_watchBus;
    std::unique_ptr<QDBusConnection> m_claimBus;
    QObject *m_signalReceiver = nullptr;
    QString m_connectionName, m_owner;
    std::optional<Sample> m_pendingSample;
    quint64 m_generation = 0;
    qulonglong m_lastTimestamp = 0;
    bool m_requested = false, m_claimed = false, m_known = false, m_sleeping = false;
};
