// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
namespace UkeKeyboard {
inline bool compatibleKWin(const QString &running,const QString &built) {
    return !built.isEmpty() && running==built;
}
}
