// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QPoint>
#include <array>
#include <cstddef>

namespace UkeKeyboard {
class PixelShift {
public:
    QPoint offset()const{return m_offset;}
    bool active()const{return m_active;}
    bool pending()const{return m_pending;}
    bool setActive(bool active) {
        if(active==m_active)return false;
        m_active=active;m_pending=false;m_index=0;
        const bool changed=!m_offset.isNull();m_offset={};return changed;
    }
    bool setBusy(bool busy) {m_busy=busy;return !busy&&m_pending?step():false;}
    bool step() {
        if(!m_active)return false;
        if(m_busy){m_pending=true;return false;}
        m_pending=false;m_index=(m_index+1)%pattern.size();
        m_offset=pattern[m_index];return true;
    }
private:
    inline static const std::array<QPoint,9> pattern{{{0,0},{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}}};
    QPoint m_offset;
    std::size_t m_index=0;
    bool m_active=false,m_busy=false,m_pending=false;
};
}
