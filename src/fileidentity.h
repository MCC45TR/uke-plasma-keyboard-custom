// SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QFile>
#include <optional>
#include <sys/stat.h>

namespace UkeKeyboard {
struct FileIdentity {
    quint64 device;
    quint64 inode;
    bool operator==(const FileIdentity &) const = default;
};

inline std::optional<FileIdentity> fileIdentity(const QString &path)
{
    struct stat status {};
    if (::stat(QFile::encodeName(path).constData(), &status) != 0 || !S_ISREG(status.st_mode)) {
        return std::nullopt;
    }
    return FileIdentity{quint64(status.st_dev), quint64(status.st_ino)};
}
}
