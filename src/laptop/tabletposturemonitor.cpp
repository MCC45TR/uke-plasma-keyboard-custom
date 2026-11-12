// SPDX-License-Identifier: GPL-2.0-or-later
#include "tabletposturemonitor.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QUuid>
#include <cmath>
#include <ctime>

namespace {
constexpr auto service = "org.senemos.Uke.Motion1";
constexpr auto path = "/org/senemos/Uke/Motion1";
constexpr auto interface = "org.senemos.Uke.Motion1";
constexpr qulonglong maximumDeliveryAgeUsec = 2000000;

qulonglong boottimeUsec() {
    timespec time{};
    if (clock_gettime(CLOCK_BOOTTIME, &time) != 0) return 0;
    return qulonglong(time.tv_sec) * 1000000 + qulonglong(time.tv_nsec) / 1000;
}

QDBusMessage request(const QString &owner, const QString &method) {
    auto message = QDBusMessage::createMethodCall(owner, QLatin1String(path), QLatin1String(interface), method);
    message.setAutoStartService(false);
    return message;
}
}

// Each lease gets its own receiver and unique bus connection. Queued signals
// and late replies from a released lease cannot be mistaken for a new claim.
class PostureSignals final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
public Q_SLOTS:
    void changed(bool known, double angle, qulonglong timestamp, const QDBusMessage &message) {
        Q_EMIT received(known, angle, timestamp, message.service());
    }
Q_SIGNALS:
    void received(bool known, double angle, qulonglong timestamp, const QString &owner);
};

TabletPostureMonitor::TabletPostureMonitor(QObject *parent)
    : QObject(parent), m_watchBus(QDBusConnection::systemBus()) {
    auto *watcher = new QDBusServiceWatcher(QLatin1String(service), m_watchBus,
                                           QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
        if (!m_requested) return;
        closeConnection();
        if (!owner.isEmpty()) openConnection(owner);
    });
    m_watchBus.connect(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                       QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("PrepareForSleep"),
                       this, SLOT(preparingSleep(bool)));
}

TabletPostureMonitor::~TabletPostureMonitor() { closeConnection(); }

void TabletPostureMonitor::setRequested(bool requested) {
    if (m_requested == requested) return;
    m_requested = requested;
    if (requested) openConnection();
    else closeConnection();
}

void TabletPostureMonitor::preparingSleep(bool sleeping) {
    if (m_sleeping == sleeping) return;
    m_sleeping = sleeping;
    if (sleeping) closeConnection();
    else if (m_requested) openConnection();
}

void TabletPostureMonitor::invalidate() {
    if (!m_known) return;
    m_known = false;
    Q_EMIT observation(false, 0);
}

void TabletPostureMonitor::closeConnection() {
    ++m_generation;
    m_claimed = false;
    m_pendingSample.reset();
    m_lastTimestamp = 0;
    if (m_signalReceiver) { m_signalReceiver->disconnect(this); m_signalReceiver->deleteLater(); }
    m_signalReceiver = nullptr;
    // Remove watchers before dropping their connection references. The ordered
    // Release is best effort; NameOwnerChanged is the provider's crash fallback.
    const auto calls = findChildren<QDBusPendingCallWatcher *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *call : calls) { call->disconnect(this); call->deleteLater(); }
    if (m_claimBus) {
        if (!m_owner.isEmpty()) m_claimBus->send(request(m_owner, QStringLiteral("Release")));
        QDBusConnection::disconnectFromBus(m_connectionName);
        m_claimBus.reset();
    }
    m_connectionName.clear();
    m_owner.clear();
    invalidate();
}

void TabletPostureMonitor::openConnection(const QString &owner) {
    if (!m_requested || m_sleeping || m_claimBus) return;
    const auto generation = ++m_generation;
    m_connectionName = QStringLiteral("uke-keyboard-posture-") + QUuid::createUuid().toString(QUuid::Id128);
    m_claimBus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SystemBus, m_connectionName));
    if (!m_claimBus->isConnected()) { closeConnection(); return; }
    if (owner.isEmpty()) resolveOwner(generation, true);
    else claim(owner, generation);
}

void TabletPostureMonitor::resolveOwner(quint64 generation, bool activate) {
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                                  QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    message << QString::fromLatin1(service);
    auto *call = new QDBusPendingCallWatcher(m_claimBus->asyncCall(message, 2000), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, generation, activate](auto *finished) {
        const auto reply = finished->reply();
        finished->deleteLater();
        if (generation != m_generation) return;
        if (reply.type() == QDBusMessage::ReplyMessage && reply.signature() == QLatin1String("s")) {
            claim(reply.arguments().first().toString(), generation);
        } else if (activate && reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")) {
            auto start = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                                        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("StartServiceByName"));
            start << QString::fromLatin1(service) << uint(0);
            auto *pending = new QDBusPendingCallWatcher(m_claimBus->asyncCall(start, 2000), this);
            connect(pending, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *started) {
                const bool success = started->reply().type() == QDBusMessage::ReplyMessage;
                started->deleteLater();
                if (generation != m_generation) return;
                if (success) resolveOwner(generation, false);
                else closeConnection();
            });
        } else closeConnection();
    });
}

void TabletPostureMonitor::claim(const QString &owner, quint64 generation) {
    if (generation != m_generation || !owner.startsWith(QLatin1Char(':'))) return;
    m_owner = owner;
    auto *receiver = new PostureSignals(this);
    m_signalReceiver = receiver;
    connect(receiver, &PostureSignals::received, this, [this, generation](bool known, double angle, qulonglong timestamp, const QString &sender) {
        if (generation != m_generation || sender != m_owner) return;
        const Sample sample{known, angle, timestamp};
        if (!m_claimed) {
            if (!m_pendingSample || timestamp > m_pendingSample->timestamp) m_pendingSample = sample;
        }
        else accept(sample);
    });
    if (!m_claimBus->connect(owner, QLatin1String(path), QLatin1String(interface), QStringLiteral("PostureChanged"),
                            QStringLiteral("bdt"), receiver, SLOT(changed(bool,double,qulonglong,QDBusMessage)))) {
        closeConnection(); return;
    }
    auto *call = new QDBusPendingCallWatcher(m_claimBus->asyncCall(request(owner, QStringLiteral("Claim")), 2000), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *finished) {
        const auto reply = finished->reply();
        finished->deleteLater();
        if (generation != m_generation) return;
        if (reply.type() != QDBusMessage::ReplyMessage || reply.service() != m_owner || reply.signature() != QLatin1String("bdt")) {
            closeConnection(); return;
        }
        m_claimed = true;
        // A signal sent after subscription supersedes the initial snapshot,
        // including an unknown/revocation edge that overtook its delayed reply.
        const auto args = reply.arguments();
        const Sample sample = m_pendingSample.value_or(Sample{args[0].toBool(), args[1].toDouble(), args[2].toULongLong()});
        m_pendingSample.reset();
        accept(sample);
    });
}

void TabletPostureMonitor::accept(const Sample &sample) {
    const auto now = boottimeUsec();
    const bool fresh = sample.timestamp > 0 && now >= sample.timestamp && now - sample.timestamp <= maximumDeliveryAgeUsec;
    if (!fresh) {
        invalidate(); return;
    }
    if (sample.timestamp <= m_lastTimestamp) return;
    m_lastTimestamp = sample.timestamp;
    if (!std::isfinite(sample.angle) || sample.angle < 0 || sample.angle > 180) { invalidate(); return; }
    if (!sample.known) { invalidate(); return; }
    m_known = true;
    Q_EMIT observation(true, sample.angle);
}

#include "tabletposturemonitor.moc"
