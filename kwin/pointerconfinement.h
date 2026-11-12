// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QObject>
#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
#include <utility>

namespace UkeKeyboard {
class DeferredPointerCorrection final : public QObject {
public:
    DeferredPointerCorrection(std::function<void()> apply,QObject *parent=nullptr)
        :QObject(parent),m_apply(std::move(apply)){}
    void request() {
        if(std::exchange(m_pending,true))return;
        QMetaObject::invokeMethod(this,[this]{m_pending=false;m_apply();},Qt::QueuedConnection);
    }
private:
    std::function<void()> m_apply;
    bool m_pending=false;
};
inline QPointF externalPointerPosition(const QList<QRectF> &outputs, QPointF position)
{
    // Match KWin's floored-point, half-open output membership. A position
    // already on an external output must stay exactly unchanged.
    const QPointF pixel(std::floor(position.x()), std::floor(position.y()));
    for (const auto &area : outputs) {
        if (pixel.x() >= area.left() && pixel.x() < area.right() &&
            pixel.y() >= area.top() && pixel.y() < area.bottom()) return position;
    }
    QPointF nearest = position;
    double distance = std::numeric_limits<double>::max();
    for (const auto &area : outputs) {
        // Integer interior targets survive KWin's own right/bottom minus-one
        // confinement. Fractional edge targets can otherwise request a warp
        // repeatedly after the compositor has adjusted the requested point.
        const double left=std::ceil(area.left()), right=std::floor(area.right()-1.0);
        const double top=std::ceil(area.top()), bottom=std::floor(area.bottom()-1.0);
        if (left>right || top>bottom) continue;
        const QPointF target(std::clamp(position.x(),left,right),std::clamp(position.y(),top,bottom));
        const double d=std::hypot(target.x()-position.x(),target.y()-position.y());
        if (d<distance) {distance=d;nearest=target;}
    }
    return nearest;
}
}
