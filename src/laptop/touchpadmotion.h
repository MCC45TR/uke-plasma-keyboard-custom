// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QPointF>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace UkeKeyboard {
class TouchpadMotion {
public:
    void reset(qint64 timestampUs=-1) {m_last=timestampUs;m_velocity=0;m_direction={};}
    QPointF map(QPointF millimetres,qint64 timestampUs,bool accelerated=true) {
        if(!std::isfinite(millimetres.x())||!std::isfinite(millimetres.y())){reset(timestampUs);return {};}
        const auto elapsed=m_last>=0?timestampUs-m_last:8333;
        const double seconds=std::clamp(double(elapsed)/1000000.0,0.001,0.25);
        if(elapsed<=0||elapsed>120000||QPointF::dotProduct(millimetres,m_direction)<0)m_velocity=0;
        m_last=timestampUs;
        const double speed=std::hypot(millimetres.x(),millimetres.y())/seconds;
        if(speed<m_velocity*0.25)m_velocity=speed;
        else m_velocity+=(speed-m_velocity)*(1-std::exp(-seconds/0.025));
        if(!millimetres.isNull())m_direction=millimetres;
        const double t=std::clamp((m_velocity-12.0)/130.0,0.0,1.0);
        // Preserve the existing precision gain; fast swipes smoothly gain up
        // to five times that distance. Never round or introduce a dead zone.
        const double gain=8.0+(accelerated?32.0*t*t*(3.0-2.0*t):0.0);
        return millimetres*gain;
    }
private:
    qint64 m_last=-1;
    double m_velocity=0;
    QPointF m_direction;
};
}
