// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
#include <optional>

namespace UkeKeyboard {
// Session policy for a fresh, screen-relative gravity observation. In-plane
// display rotation never changes the angle between the screen and horizontal.
class TabletPosture {
public:
    bool known() const { return m_angle.has_value(); }
    bool allowed() const { return known() && m_allowed; }
    std::optional<double> angle() const { return m_angle; }
    void reset() { m_angle.reset(); m_allowed = false; m_waitForFlat = false; }

    bool update(std::optional<double> angle) {
        const bool wasKnown = known(), wasAllowed = allowed();
        if (!angle || !std::isfinite(*angle) || *angle < 0 || *angle > 180) {
            m_angle.reset(); m_allowed = false;
        } else {
            m_angle = angle;
            if (*angle > 45) m_waitForFlat = true;
            else if (*angle < 40) m_waitForFlat = false;
            m_allowed = !m_waitForFlat && *angle <= 45;
        }
        return known() != wasKnown || allowed() != wasAllowed;
    }

private:
    std::optional<double> m_angle;
    bool m_allowed = false, m_waitForFlat = false;
};
}
