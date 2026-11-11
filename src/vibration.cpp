// SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

#include "vibration.h"

#include "logging.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

namespace
{
constexpr auto s_inputPlumberService = "org.shadowblip.InputPlumber";
constexpr auto s_forceFeedbackInterface = "org.shadowblip.Output.ForceFeedback";
constexpr auto s_managerPath = "/org/shadowblip/InputPlumber/Manager";
constexpr auto s_dbusDevicePath = "/org/shadowblip/InputPlumber/devices/target/dbus0";
constexpr auto s_fallbackCompositePath = "/org/shadowblip/InputPlumber/CompositeDevice0";

// Rumble motors need some spin-up time before they can be felt at all.
constexpr int s_minimumRumbleMs = 30;

QString gamepadOrderPath()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(s_inputPlumberService),
                                                      QLatin1String(s_managerPath),
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    msg << QStringLiteral("org.shadowblip.InputManager") << QStringLiteral("GamepadOrder");
    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    const QVariant value = reply.arguments().first().value<QDBusVariant>().variant();
    return value.toStringList().value(0);
}

bool hasForceFeedback(const QString &path)
{
    QDBusMessage msg =
        QDBusMessage::createMethodCall(QLatin1String(s_inputPlumberService), path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    msg << QStringLiteral("org.shadowblip.Output.ForceFeedback") << QStringLiteral("Enabled");
    return QDBusConnection::systemBus().call(msg).type() == QDBusMessage::ReplyMessage;
}

QStringList forceFeedbackCandidates()
{
    QStringList candidates;
    const QString compositePath = gamepadOrderPath();
    if (!compositePath.isEmpty()) {
        candidates << compositePath;
    }
    candidates << QLatin1String(s_fallbackCompositePath) << QLatin1String(s_dbusDevicePath);
    return candidates;
}
} // namespace

Vibration::Vibration(QObject *parent)
    : QObject{parent}
{
    qDBusRegisterMetaType<VibrationEvent>();
    qDBusRegisterMetaType<VibrationEventList>();

    m_stopTimer.setSingleShot(true);
    connect(&m_stopTimer, &QTimer::timeout, this, &Vibration::stopRumble);
}

void Vibration::vibrate(int durationMs, int strengthPercent)
{
    const double strength = qBound(0, strengthPercent, 100) / 100.0;

    // On handhelds the rumble motors of the gamepad are the only way to
    // provide tactile feedback; InputPlumber provides them over D-Bus.
    if (rumbleGamepad(qMax(durationMs, s_minimumRumbleMs), strength)) {
        return;
    }

    vibrateFeedbackd(durationMs, strength);
}

bool Vibration::rumbleGamepad(int durationMs, double strength)
{
    if (m_forceFeedbackPath.isEmpty()) {
        const QStringList candidates = forceFeedbackCandidates();
        for (const QString &path : candidates) {
            if (!hasForceFeedback(path)) {
                continue;
            }
            m_forceFeedbackPath = path;
            qCDebug(PlasmaKeyboard) << "Vibration: using the gamepad rumble at" << path;
            break;
        }
    }

    if (m_forceFeedbackPath.isEmpty()) {
        return false;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(s_inputPlumberService),
                                                      m_forceFeedbackPath,
                                                      QLatin1String(s_forceFeedbackInterface),
                                                      QStringLiteral("Rumble"));
    msg << strength;
    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        // The device went away, look for it again on the next key press.
        m_forceFeedbackPath.clear();
        return false;
    }

    m_stopTimer.start(durationMs);
    return true;
}

void Vibration::stopRumble()
{
    if (m_forceFeedbackPath.isEmpty()) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(s_inputPlumberService),
                                                      m_forceFeedbackPath,
                                                      QLatin1String(s_forceFeedbackInterface),
                                                      QStringLiteral("Stop"));
    QDBusConnection::systemBus().call(msg);
}

void Vibration::vibrateFeedbackd(int durationMs, double strength)
{
    // Only create interface when needed.
    if (!m_interface) {
        const auto objectPath = QStringLiteral("/org/sigxcpu/Feedback");
        m_interface = new OrgSigxcpuFeedbackHapticInterface(QStringLiteral("org.sigxcpu.Feedback"), objectPath, QDBusConnection::sessionBus(), this);
    }

    const QString appId = QStringLiteral("org.uke.plasma.keyboard");
    const VibrationEvent event{strength, static_cast<quint32>(durationMs)};
    const VibrationEventList pattern = {event};

    m_interface->Vibrate(appId, pattern);
}

#include "moc_vibration.cpp"
