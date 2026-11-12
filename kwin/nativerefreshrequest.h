// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <core/outputconfiguration.h>

namespace UkeKeyboard {
inline KWin::OutputChangeSet refreshChanges(const KWin::OutputModeline &mode,KWin::VrrPolicy policy)
{
 KWin::OutputChangeSet change;
 // currentMode is temporary. Leave the user's persistent desiredMode and all
 // geometry, power, scale, rotation, color and other-output fields untouched.
 change.currentMode=mode;
 change.vrrPolicy=policy;
 return change;
}
}
