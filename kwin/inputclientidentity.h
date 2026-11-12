// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace KWin { class ClientConnection; }
namespace UkeKeyboard {
inline bool isInputMethodClient(const KWin::ClientConnection *client,const KWin::ClientConnection *selected)
{
    // A compositor-created socketpair reports the compositor's peer PID.
    // Only the exact connection selected by KWin identifies its input method.
    return client && selected && client==selected;
}
}
