// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "laptop/protocol.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <functional>
#include <unistd.h>

namespace UkeKeyboard {
// Ask the actual compositor parent, never an arbitrary session-bus owner.
// Input context activation alone does not identify the triggering device.
inline void requestTouchActivation(QObject *context, std::function<void(bool)> completed)
{
    auto bus = QDBusConnection::sessionBus();
    auto *interface = bus.interface();
    if (!interface) { completed(false); return; }
    const auto owner = interface->serviceOwner(QLatin1String(service));
    if (!owner.isValid() || owner.value().isEmpty()) { completed(false); return; }
    const auto pid = interface->servicePid(owner.value());
    if (!pid.isValid() || pid.value() != uint(getppid())) { completed(false); return; }
    auto request = QDBusMessage::createMethodCall(owner.value(), QLatin1String(path),
        QLatin1String(UkeKeyboard::interface), QStringLiteral("TouchActivationAllowed"));
    request.setAutoStartService(false);
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(request), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
        [completed = std::move(completed)](auto *call) {
            QDBusPendingReply<bool> reply = *call;
            call->deleteLater();
            completed(!reply.isError() && reply.value());
        });
}
}
