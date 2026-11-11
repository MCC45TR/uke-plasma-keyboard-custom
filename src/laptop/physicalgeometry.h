// SPDX-License-Identifier: MIT
#pragma once
#include <QSizeF>
#include <algorithm>
#include <cmath>
namespace UkeKeyboard {
inline bool plausibleSize(QSizeF mm) {
    return std::isfinite(mm.width()) && std::isfinite(mm.height()) && mm.width() >= 60 && mm.height() >= 60 && mm.width() <= 1000 && mm.height() <= 1000;
}
inline double unitsPerMm(double logicalWidth, double widthMm) {
    return logicalWidth > 0 && widthMm >= 60 && widthMm <= 1000 ? logicalWidth / widthMm : 96.0 / 25.4;
}
inline double fittedKeyWidth(double available, double units, double targetMm, double perMm, double gapMm) {
    if (available <= 0 || units <= 0 || perMm <= 0) return 0;
    return std::max(1.0, std::min(targetMm*perMm, (available-std::max(0.0,units-1)*gapMm*perMm)/units));
}
}
