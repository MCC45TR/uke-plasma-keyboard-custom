/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "inputplugin.h"
#include "inputcontentpolicy.h"
#include "inputmethod_p.h"

#include <QStringList>
#include <QDateTime>
#include <algorithm>
#include <cstdlib>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <xkbcommon/xkbcommon-names.h>

namespace
{
//! Wayland rejects any single message larger than 4096 bytes.
constexpr int s_maxCommitBytes = 3800;

/**
 * Splits @p text into pieces that fit into a single commit, without ever
 * cutting a character (or a surrogate pair) in half.
 */
QStringList splitCommit(const QString &text)
{
    QStringList chunks;
    QString chunk;
    int chunkBytes = 0;

    const QList<uint> codePoints = text.toUcs4();
    for (uint codePoint : codePoints) {
        const char32_t character = codePoint;
        const QString encoded = QString::fromUcs4(&character, 1);
        const int bytes = encoded.toUtf8().size();
        if (chunkBytes + bytes > s_maxCommitBytes && !chunk.isEmpty()) {
            chunks.append(chunk);
            chunk.clear();
            chunkBytes = 0;
        }
        chunk.append(encoded);
        chunkBytes += bytes;
    }

    if (!chunk.isEmpty()) {
        chunks.append(chunk);
    }

    return chunks;
}
}

InputPlugin::InputPlugin(InputMethod *inputMethod)
{
    connect(inputMethod, &InputMethod::deactivate, this, &InputPlugin::deactivate);
    connect(inputMethod, &InputMethod::activityChanged, this, [this, inputMethod]() {
        Q_EMIT contextAboutToChange();
        if (m_context) {
            disconnect(m_context.get(), nullptr, this, nullptr);
        }
        m_keyboard.reset();
        m_context = inputMethod->activeContext();
        Q_EMIT contextChanged();
        Q_EMIT surroundingTextChanged();
        if (!m_context) {
            return;
        }

        connect(m_context.get(), &InputMethodContext::receivedCommit, this, &InputPlugin::receivedCommit);
        connect(m_context.get(), &InputMethodContext::surroundingTextChanged, this, &InputPlugin::surroundingTextChanged);
        connect(m_context.get(), &InputMethodContext::contentTypeChanged, this, &InputPlugin::contentTypeChanged);
        connect(m_context.get(), &InputMethodContext::reset, this, &InputPlugin::resetRequested);
        connect(m_context.get(), &InputMethodContext::preferredLanguageChanged, this, &InputPlugin::preferredLanguageChanged);
        if (m_grabbing) {
            setGrabbingInternal();
        }
    });
}

InputPlugin::~InputPlugin()
{
}

void InputPlugin::setGrabbing(bool grabbing)
{
    m_grabbing = grabbing;
    if (m_context && grabbing) {
        setGrabbingInternal();
    }
    if (m_keyboard && !grabbing) {
        disconnect(m_keyboard.get(), nullptr, this, nullptr);
        m_keyboard.reset();
    }
}

void InputPlugin::setPreEditString(const QString &text)
{
    if (!m_context) {
        return;
    }
    m_context->preedit_string(m_context->m_latestSerial, text, text);
}

void InputPlugin::moveCursor(int cusorPosition, int anchorPosition)
{
    if (!m_context) {
        return;
    }
    m_context->cursor_position(cusorPosition, anchorPosition);
}

void InputPlugin::setPreEditCursor(int cursorPosition)
{
    if (!m_context) {
        return;
    }
    m_context->preedit_cursor(cursorPosition);
}

void InputPlugin::setPreEditStyle(int startPosition, int length, int style)
{
    if (!m_context) {
        return;
    }
    m_context->preedit_styling(startPosition, length, style);
}

void InputPlugin::deleteSurroundingText(int index, int length)
{
    if (!m_context) {
        return;
    }
    m_context->delete_surrounding_text(index, length);
}

void InputPlugin::commit(const QString &text)
{
    if (!m_context) {
        return;
    }

    // A long text (e.g. pasted from the clipboard) does not fit into one
    // Wayland message, so it is committed in several pieces.
    const QStringList chunks = splitCommit(text);
    for (const QString &chunk : chunks) {
        m_context->commit_string(m_context->m_latestSerial, chunk);
    }
}

void InputPlugin::keysym(uint timestamp, uint sym, KeyState state, uint modifiers)
{
    if (!m_context) {
        return;
    }
    m_context->keysym(m_context->m_latestSerial, timestamp, sym, state, modifiers);
}

void InputPlugin::editCursor(int steps)
{
    if(!m_context)return;
    const auto symbol=steps<0?XKB_KEY_Left:XKB_KEY_Right;
    const auto time=uint32_t(QDateTime::currentMSecsSinceEpoch());
    for(int i=0;i<std::abs(std::clamp(steps,-32,32));++i){keysym(time,symbol,Pressed,0);keysym(time,symbol,Released,0);}
}

void InputPlugin::deleteWords(int count)
{
    if(!m_context||!m_keyboard||count<=0)return;
    const auto code=keycodeForKeysym(XKB_KEY_BackSpace),control=controlMask();
    if(!code||!control)return;
    const auto original=m_keyboard->modifierState();
    sendModifiers(original[0]|control,original[1],original[2],original[3]);
    for(int i=0;i<std::min(count,32);++i){key(Pressed,code);key(Released,code);}
    sendModifiers(original[0],original[1],original[2],original[3]);
}

void InputPlugin::key(KeyState state, quint32 scancode)
{
    if (!m_context) {
        return;
    }
    m_context->key(m_context->m_lastKeyboardSerial, m_context->m_lastKeyboardTime, scancode, static_cast<uint32_t>(state));
}

void InputPlugin::sendModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
    if (!m_context) {
        return;
    }
    m_context->modifiers(m_context->m_latestSerial, depressed, latched, locked, group);
}

uint32_t InputPlugin::keycodeForKeysym(uint32_t keysym) const
{
    if (!m_keyboard) {
        return 0;
    }
    return m_keyboard->evdevKeycodeForKeysym(keysym);
}

uint32_t InputPlugin::controlMask() const
{
    return m_keyboard ? m_keyboard->modifierMask(XKB_MOD_NAME_CTRL) : 0;
}

uint32_t InputPlugin::altMask() const
{
    return m_keyboard ? m_keyboard->modifierMask(XKB_MOD_NAME_ALT) : 0;
}

uint32_t InputPlugin::metaMask() const
{
    return m_keyboard ? m_keyboard->modifierMask(XKB_MOD_NAME_LOGO) : 0;
}

uint32_t InputPlugin::shiftMask() const
{
    return m_keyboard ? m_keyboard->modifierMask(XKB_MOD_NAME_SHIFT) : 0;
}

bool InputPlugin::predictionsAllowed() const
{
    return InputContentPolicy::predictionsAllowed(hasContext(), m_context && m_context->m_contentTypeKnown, contentHint(), contentPurpose());
}

InputPlugin::ContentHint InputPlugin::contentHint() const
{
    if (!m_context) {
        return InputPlugin::content_hint_none;
    }
    return m_context->m_contentHint;
}

InputPlugin::ContentPurpose InputPlugin::contentPurpose() const
{
    if (!m_context) {
        return InputPlugin::content_purpose_normal;
    }
    return m_context->m_contentPurpose;
}

QString InputPlugin::surroundingText() const
{
    if (!m_context) {
        return QString();
    }
    return m_context->m_text;
}

uint32_t InputPlugin::cursorPos() const
{
    if (!m_context) {
        return 0;
    }
    return m_context->m_cursor;
}

uint32_t InputPlugin::anchorPos() const
{
    if (!m_context) {
        return 0;
    }
    return m_context->m_anchor;
}

void InputPlugin::setGrabbingInternal()
{
    Q_ASSERT(m_context);
    m_keyboard = m_context->keyboard();
    connect(m_keyboard.get(), &Keyboard::keyPressed, this, &InputPlugin::keyPressed);
    connect(m_keyboard.get(), &Keyboard::keyReleased, this, &InputPlugin::keyReleased);
}

#include "moc_inputplugin.cpp"
