// SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QFile>
#include <QString>

namespace UkeKeyboard {
inline bool hasProcessParent(qint64 pid, qint64 expectedParent)
{
    if (pid <= 0 || expectedParent <= 0) {
        return false;
    }
    QFile status(QStringLiteral("/proc/%1/status").arg(pid));
    if (!status.open(QIODevice::ReadOnly)) {
        return false;
    }
    // procfs reports size zero. QFile::atEnd() therefore cannot tell whether
    // status has readable data; read until the kernel returns no more bytes.
    for (QByteArray line; !(line = status.readLine(1024)).isEmpty();) {
        if (line.startsWith("PPid:")) {
            bool valid = false;
            const auto parent = line.mid(5).trimmed().toLongLong(&valid);
            return valid && parent == expectedParent;
        }
    }
    return false;
}
}
