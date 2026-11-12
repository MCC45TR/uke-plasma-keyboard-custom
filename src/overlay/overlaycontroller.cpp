/*
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "overlaycontroller.h"

#include "inputplugin.h"
#include "logging.h"
#include "overlaytrigger.h"

OverlayController::OverlayController(InputPlugin *inputPlugin, QObject *parent)
    : QObject(parent)
    , m_inputPlugin(inputPlugin)
    , m_candidateModel(new CandidateModel(this))
{
    if (m_inputPlugin) {
        connect(m_inputPlugin, &InputPlugin::contextAboutToChange, this, [this] {
            cancelOverlay();
            // Release suppression belongs to the old grab, not the new field.
            m_swallowNextRelease = false;
            m_ignoreReleaseNativeScanCode = 0;
        });
    }

    m_holdTimer.setSingleShot(true);
    connect(&m_holdTimer, &QTimer::timeout, this, &OverlayController::handleTimerExpired);

    m_overlayGraceTimer.setSingleShot(true);
    connect(&m_overlayGraceTimer, &QTimer::timeout, this, &OverlayController::handleOverlayGraceTimer);

    // The settle timer is a single-shot guard that absorbs extra surrounding_text
    // echoes from clients that send more than one event per commit_string.
    // 100 ms is well above any realistic Wayland roundtrip but well below the
    // long-press threshold, so genuine external cursor moves are not masked.
    static constexpr int SURROUNDING_TEXT_SETTLE_DELAY_MS = 100;
    m_surroundingTextSettleTimer.setSingleShot(true);
    m_surroundingTextSettleTimer.setInterval(SURROUNDING_TEXT_SETTLE_DELAY_MS);

    // Initialize XKB compose state machine using the system locale.
    m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (m_xkbContext) {
        const char *locale = setlocale(LC_CTYPE, nullptr);
        if (!locale || locale[0] == '\0') {
            locale = "C";
        }
        m_xkbComposeTable = xkb_compose_table_new_from_locale(m_xkbContext, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (m_xkbComposeTable) {
            m_xkbComposeState = xkb_compose_state_new(m_xkbComposeTable, XKB_COMPOSE_STATE_NO_FLAGS);
        }
    }
}

OverlayController::~OverlayController()
{
    if (m_xkbComposeState)
        xkb_compose_state_unref(m_xkbComposeState);
    if (m_xkbComposeTable)
        xkb_compose_table_unref(m_xkbComposeTable);
    if (m_xkbContext)
        xkb_context_unref(m_xkbContext);
}

void OverlayController::registerTrigger(OverlayTrigger *trigger)
{
    if (!trigger) {
        return;
    }
    trigger->setParent(this);
    m_triggers.append(trigger);
}

bool OverlayController::processKeyPress(QKeyEvent *event)
{
    if (!event) {
        return false;
    }

    const InputPlugin::ContentPurpose contentPurpose = m_inputPlugin ? m_inputPlugin->contentPurpose() : InputPlugin::content_purpose_normal;
    // qCDebug(PlasmaKeyboard) << "Content purpose:" << contentPurpose;
    if (contentPurpose == InputPlugin::content_purpose_terminal) {
        // Don't trigger overlays in terminal input fields, since they don't usually offer
        // us surrounding_text information. This can be revisited with text-input v3.2,
        // which will support styling preedits again, and switching to preedit-based flow
        // will work for terminals.
        return false;
    }

    // Suppress auto-repeat key events for the pending key, so long-press for diacritics
    // works without accidentally triggering repeat input on the client side.
    //
    // Compare by native scan code (physical key) rather than Qt key code, because
    // the Qt key code can change if the modifier is released before the repeat fires
    // (e.g. Shift+/ produces Key_Question on press but Key_Slash on repeat).
    if (event->isAutoRepeat() && (event->nativeScanCode() == m_pendingNativeScanCode || event->nativeScanCode() == m_repeatNativeScanCode)) {
        // qCDebug(PlasmaKeyboard) << "Suppressing repeat press of pending overlay key" << m_pendingText;
        return true;
    }

    // If overlay is visible, handle Esc to cancel
    if (m_overlayVisible) {
        if (event->key() == Qt::Key_Escape) {
            qCDebug(PlasmaKeyboard) << "Esc pressed while overlay open; cancelling";
            cancelOverlay();
            return true;
        }

        // Handle number keys 1-9 for candidate selection
        if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
            int index = event->key() - Qt::Key_1;
            if (index < m_candidateModel->rowCount()) {
                qCDebug(PlasmaKeyboard) << "Committing an overlay candidate";
                commitCandidate(index);
                return true;
            }
            // Number beyond available candidates - fall through to cancel
        }

        // Handle arrow/Enter keys for overlay navigation
        switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Return:
            // qCDebug(PlasmaKeyboard) << "Navigation key" << event->key() << "pressed while overlay open; forwarding to overlay";
            Q_EMIT overlayNavigationKeyPressed(event->key());
            return true;
        }

        // Any other key cancels the overlay
        cancelOverlay();
        // Fall through to process the new key
    }

    // XKB compose state machine — handles Multi_key sequences and dead keys.
    // Keys that are part of a sequence are consumed here; the composed result
    // is committed via commit_string so it never reaches the trigger pipeline.
    if (m_xkbComposeState) {
        const xkb_keysym_t keysym = static_cast<xkb_keysym_t>(event->nativeVirtualKey());
        if (keysym != XKB_KEY_NoSymbol) {
            const xkb_compose_feed_result feedResult = xkb_compose_state_feed(m_xkbComposeState, keysym);
            if (feedResult == XKB_COMPOSE_FEED_ACCEPTED) {
                const xkb_compose_status status = xkb_compose_state_get_status(m_xkbComposeState);
                switch (status) {
                case XKB_COMPOSE_COMPOSING:
                    // Sequence started or continuing — cancel any pending overlay
                    // (it belongs to a key typed before the sequence began).
                    if (m_holdTimer.isActive() || m_overlayVisible) {
                        cancelOverlay();
                    }
                    return true; // Consume; do not forward to compositor.
                case XKB_COMPOSE_COMPOSED: {
                    if (m_holdTimer.isActive() || m_overlayVisible) {
                        cancelOverlay();
                    }
                    char buf[64] = {};
                    if (xkb_compose_state_get_utf8(m_xkbComposeState, buf, sizeof(buf)) > 0 && m_inputPlugin) {
                        ++m_pendingSurroundingTextUpdates;
                        m_surroundingTextSettleTimer.start();
                        m_inputPlugin->commit(QString::fromUtf8(buf));
                    }
                    xkb_compose_state_reset(m_xkbComposeState);
                    return true; // Consume the completing key.
                }
                case XKB_COMPOSE_CANCELLED:
                    xkb_compose_state_reset(m_xkbComposeState);
                    break; // Fall through — process cancelling key normally.
                case XKB_COMPOSE_NOTHING:
                    break; // Not in a sequence; fall through.
                }
            } else {
                // qCDebug(PlasmaKeyboard) << "XKB compose feed result:" << feedResult << "(keysym" << keysym << "not part of a compose sequence)";
            }
            // XKB_COMPOSE_FEED_IGNORED or post-cancel: fall through to triggers.
        }
    }

    // Dispatch to triggers in order
    for (auto *trigger : m_triggers) {
        if (!trigger->isEnabled()) {
            continue;
        }

        auto result = trigger->processEvent(OverlayInputEvent::KeyPress, event, event->text(), this);
        if (result.action != OverlayAction::None || result.consumeEvent) {
            executeAction(result, trigger);
            return result.consumeEvent;
        }
    }

    return false;
}

bool OverlayController::processKeyRelease(QKeyEvent *event)
{
    if (!event) {
        return false;
    }

    // Compare by native scan code (physical key) so that modifier changes
    // between press and release don't prevent the match.
    bool keyMatchesPending = (event->nativeScanCode() == m_pendingNativeScanCode);

    // If the overlay grace timer is running and the held key is released,
    // stop the grace timer — the overlay stays visible waiting for selection
    // or cancellation via the normal pathways.
    if (m_overlayGraceTimer.isActive() && keyMatchesPending) {
        // qCDebug(PlasmaKeyboard) << "Overlay grace timer stopped due to release of held key" << m_pendingNativeScanCode;
        m_overlayGraceTimer.stop();
    }

    bool keyRepeatActive = (m_repeatNativeScanCode != 0 && event->nativeScanCode() == m_repeatNativeScanCode);
    if (keyRepeatActive) {
        // If key repeat is active, it means we sent a synthetic key(Pressed) to the
        // client after the grace timer expired so that the client would do auto-repeat.
        //
        // Now that the physical key is released, we will simply not handle the release
        // event, so it gets forwarded and the client sees the release and stops repeating.
        // qCDebug(PlasmaKeyboard) << "Stopping repeat for" << m_repeatNativeScanCode;
        // This physical release is forwarded by Keyboard::keyboard_key().
        // Do not synthesize a second release during reset.
        m_repeatNativeScanCode = 0;
        resetState();
        return false;
    }

    // Swallow release of a discarded/committed pending key.
    if (m_swallowNextRelease && event->nativeScanCode() == m_ignoreReleaseNativeScanCode) {
        if (!event->isAutoRepeat()) {
            // qCDebug(PlasmaKeyboard) << "Swallowing release for discarded overlay key" << m_ignoreReleaseNativeScanCode;
            m_swallowNextRelease = false;
            m_ignoreReleaseNativeScanCode = 0;
        }
        return true;
    }

    // Handle pending key release.
    if (!m_pendingText.isEmpty() && keyMatchesPending) {
        // Stop timer if still active
        if (m_holdTimer.isActive()) {
            m_holdTimer.stop();
        }

        if (m_overlayVisible) {
            // Release doesn't close overlay; wait for selection or cancel
            m_pendingKeyReleased = true;
            return true;
        }

        // The client was already sent a synthetic key press and release, so we just need
        // to clear pending state and consume the release (which would be extra as far as the client is concerned).
        qCDebug(PlasmaKeyboard) << "Releasing before overlay; key released";
        resetState();
        return true;
    }

    // Dispatch to triggers for release handling
    for (auto *trigger : m_triggers) {
        if (!trigger->isEnabled()) {
            continue;
        }

        auto result = trigger->processEvent(OverlayInputEvent::KeyRelease, event, event->text(), this);
        if (result.action != OverlayAction::None || result.consumeEvent) {
            executeAction(result, trigger);
            return result.consumeEvent;
        }
    }

    return false;
}

bool OverlayController::processPreeditChanged(const QString &preedit)
{
    for (auto *trigger : m_triggers) {
        if (!trigger->isEnabled()) {
            continue;
        }

        auto result = trigger->processEvent(OverlayInputEvent::PreeditChanged, nullptr, preedit, this);
        if (result.action != OverlayAction::None) {
            executeAction(result, trigger);
            return true;
        }
    }
    return false;
}

bool OverlayController::processTextCommitted(const QString &text)
{
    for (auto *trigger : m_triggers) {
        if (!trigger->isEnabled()) {
            continue;
        }

        auto result = trigger->processEvent(OverlayInputEvent::TextCommitted, nullptr, text, this);
        if (result.action != OverlayAction::None) {
            executeAction(result, trigger);
            return true;
        }
    }
    return false;
}

bool OverlayController::overlayVisible() const
{
    return m_overlayVisible;
}

QString OverlayController::activeTriggerId() const
{
    return m_activeTriggerId;
}

QString OverlayController::pendingText() const
{
    return m_pendingText;
}

CandidateModel *OverlayController::candidateModel() const
{
    return m_candidateModel;
}

bool OverlayController::alternatesOnly() const
{
    return m_alternatesOnly;
}

int OverlayController::alternateSelection() const
{
    return m_alternateSelection;
}

quint32 OverlayController::pendingNativeScanCode() const
{
    return m_pendingNativeScanCode;
}

InputPlugin *OverlayController::inputPlugin() const
{
    return m_inputPlugin;
}

void OverlayController::setInputEngine(QVirtualKeyboardInputEngine *engine)
{
    m_inputEngine = engine;
}

void OverlayController::commitAlternate(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    // Qt Virtual Keyboard inserts a character picked from the alternate-keys
    // popup as a key click through its input engine, and clients take that for
    // ordinary typing. A bare commit_string instead makes a client (an address
    // bar, a search field) treat the input as finished, and the compositor
    // takes the keyboard down with it. Take the same route here.
    if (m_inputEngine && m_inputEngine->virtualKeyClick(Qt::Key_unknown, text, Qt::KeyboardModifiers())) {
        qCDebug(PlasmaKeyboard) << "Committing an alternate as a key click";
        setOverlayVisible(false);
        resetState();
        return;
    }

    commitText(text);
}

void OverlayController::commitCandidate(int index)
{
    const QString text = m_candidateModel->insertTextAt(index);
    if (text.isEmpty()) {
        return;
    }

    // A list the gamepad opened holds the layout's alternate characters, which
    // nothing was typed for: insert them the way the on-screen popup does.
    if (m_alternatesOnly) {
        commitAlternate(text);
        return;
    }

    commitText(text);
}

void OverlayController::commitText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    qCDebug(PlasmaKeyboard) << "Committing an overlay selection";

    if (m_inputPlugin) {
        // The base character (m_pendingText) is still present in the text field —
        // it was committed on key-press and never retracted. Delete it now and
        // replace it with the selected candidate in one atomic operation:
        //   delete_surrounding_text is deferred by the compositor until the
        //   following commit_string, so sending both requests back-to-back
        //   guarantees they are applied together at the current cursor position.
        if (!m_pendingText.isEmpty()) {
            const int charCount = m_pendingText.length();
            m_inputPlugin->deleteSurroundingText(-charCount, charCount);
        }
        m_inputPlugin->commit(text);
    }

    setOverlayVisible(false);

    // Only swallow the next release if the key is still physically held
    if (!m_pendingKeyReleased) {
        m_ignoreReleaseNativeScanCode = m_pendingNativeScanCode;
        m_swallowNextRelease = true;
    }

    resetState();
}

void OverlayController::cancelOverlay()
{
    // The base character was committed on key-press and is already present in
    // the text field. It was never retracted, so there is nothing to restore
    // regardless of whether the overlay was visible or only the hold timer was
    // running. Simply close the overlay and reset state.

    setOverlayVisible(false);

    if (!m_pendingText.isEmpty() && !m_pendingKeyReleased) {
        m_ignoreReleaseNativeScanCode = m_pendingNativeScanCode;
        m_swallowNextRelease = true;
    }

    // Reset all triggers
    for (auto *trigger : m_triggers) {
        trigger->reset();
    }

    resetState();
}

void OverlayController::handleSurroundingTextChanged()
{
    // If we are expecting a surrounding-text update from one of our own
    // commit/delete-surrounding-text operations, consume that credit and return.
    if (m_pendingSurroundingTextUpdates > 0) {
        qCDebug(PlasmaKeyboard) << "Ignoring self-caused surrounding-text update (" << m_pendingSurroundingTextUpdates << " remaining)";
        --m_pendingSurroundingTextUpdates;
        return;
    }

    // Counter exhausted — fall back to the settle timer.
    //
    // Some client apps (Firefox, Chromium, VS Code, …) send two or more
    // surrounding_text events for a single commit_string. The credit counter
    // above covers the first echo; the settle timer absorbs any remaining
    // echoes without a cursor-position comparison (which is unreliable when
    // the client reports a shifted surrounding-text window after insertion).
    if (m_surroundingTextSettleTimer.isActive()) {
        // qCDebug(PlasmaKeyboard) << "Ignoring extra surrounding-text echo (within settle window)";
        return;
    }

    // An external event changed the cursor (e.g. user tapped elsewhere in the
    // text field). Cancel any pending overlay state.
    if (m_holdTimer.isActive() || m_overlayVisible) {
        qCDebug(PlasmaKeyboard) << "External cursor move detected while overlay active; cancelling overlay";
        cancelOverlay();
    }
}

void OverlayController::openOverlay(const QString &triggerId, const QString &baseText, const QStringList &candidates)
{
    if (candidates.isEmpty()) {
        // No candidates: the base character was already committed on key-press
        // and is present in the text field, so no further commit is needed.
        // Simply reset state and leave the text as-is.
        resetState();
        return;
    }

    m_activeTriggerId = triggerId;
    m_pendingText = baseText;
    // Candidates offered without a typed base character (the gamepad, which
    // cannot type the key that is highlighted) are picked without any commit
    // happening first, so they need a way to be chosen without the command
    // buttons of the gamepad.
    const bool wasAlternatesOnly = m_alternatesOnly;
    m_alternatesOnly = baseText.isEmpty();
    m_alternateSelection = -1;
    if (wasAlternatesOnly != m_alternatesOnly) {
        Q_EMIT alternatesOnlyChanged();
    }
    Q_EMIT alternateSelectionChanged();

    m_candidateModel->setTriggerId(triggerId);
    m_candidateModel->setCandidates(candidates);

    setOverlayVisible(true);

    Q_EMIT activeTriggerIdChanged();
    Q_EMIT pendingTextChanged();
    Q_EMIT overlayRequested(triggerId, baseText);

    // Start grace timer for diacritics long-press overlays: if the user keeps
    // holding the key after the overlay opens, after a grace period the overlay
    // closes and the base character starts auto-repeating.
    //
    // Only start if there is a physically held pending key (not for emoji/text-
    // expansion overlays which don't use the hold timer pathway).
    if (!m_pendingText.isEmpty() && m_pendingTrigger && !m_pendingKeyReleased) {
        m_overlayGraceTimer.start(1000); // 1000 ms grace period after overlay opens
    }
}

void OverlayController::handleTimerExpired()
{
    if (m_pendingText.isEmpty() || !m_pendingTrigger) {
        return;
    }

    auto result = m_pendingTrigger->processEvent(OverlayInputEvent::TimerExpired, nullptr, m_pendingText, this);

    // If opening the overlay, the base character that was committed on key-press
    // is intentionally left in the text field. It will be replaced atomically
    // (delete + commit_string) when the user selects a candidate in commitText().
    // Do NOT call deleteSurroundingText here: delete_surrounding_text is deferred
    // by the compositor until the next commit_string, so a standalone delete
    // without a following commit would be consumed by a future unrelated commit
    // at a potentially different cursor position, corrupting the text.
    executeAction(result, m_pendingTrigger);
}

bool OverlayController::openAlternates(const QStringList &alternates)
{
    if (alternates.isEmpty()) {
        return false;
    }

    // Replacing a physical-key overlay must also end any synthetic repeat.
    cancelOverlay();

    // Opening this overlay is not caused by typing, so the base text is empty
    // and no commit_string is sent. The client may still report a surrounding
    // text update — the focus and the selection of the field change while the
    // keyboard moves — and such an update must not be mistaken for the user
    // moving the cursor elsewhere, which would cancel the overlay right away.
    ++m_pendingSurroundingTextUpdates;
    m_surroundingTextSettleTimer.start();

    // The characters come from the layout of the highlighted key, so the same
    // popup view is used as for a long press on that key.
    openOverlay(QStringLiteral("alternates"), QString(), alternates);

    if (!m_overlayVisible) {
        return false;
    }

    // Nothing has been typed yet, so the first character is selected right
    // away: the list is usable with the direction buttons alone.
    m_alternateSelection = 0;
    Q_EMIT alternateSelectionChanged();
    return true;
}

void OverlayController::navigateAlternates(int key)
{
    if (!m_overlayVisible || m_alternatesOnly == false) {
        return;
    }

    const int count = m_candidateModel->rowCount();
    if (count <= 0) {
        return;
    }

    switch (key) {
    case Qt::Key_Left:
        m_alternateSelection = m_alternateSelection <= 0 ? count - 1 : m_alternateSelection - 1;
        break;
    case Qt::Key_Right:
        m_alternateSelection = m_alternateSelection < 0 ? 0 : (m_alternateSelection + 1) % count;
        break;
    case Qt::Key_Up:
        m_alternateSelection = 0;
        break;
    case Qt::Key_Down:
        m_alternateSelection = count - 1;
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_alternateSelection >= 0 && m_alternateSelection < count) {
            commitCandidate(m_alternateSelection);
        } else {
            cancelOverlay();
        }
        return;
    case Qt::Key_Escape:
        cancelOverlay();
        return;
    default:
        return;
    }

    Q_EMIT alternateSelectionChanged();
}

void OverlayController::executeAction(const OverlayTriggerResult &result, OverlayTrigger *trigger)
{
    switch (result.action) {
    case OverlayAction::OpenOverlay: {
        const auto candidates = trigger->candidates(m_pendingText);
        openOverlay(trigger->triggerId(), m_pendingText, candidates);
        break;
    }
    case OverlayAction::CloseOverlay:
        cancelOverlay();
        break;
    case OverlayAction::CommitText:
        if (!result.commitText.isEmpty()) {
            commitText(result.commitText);
        } else if (!m_pendingText.isEmpty()) {
            commitText(m_pendingText);
        }
        break;
    case OverlayAction::ReplaceText:
        // For text expansion: delete chars before cursor, then commit.
        // Both operations are sent back-to-back; delete_surrounding_text is
        // deferred by the compositor until the following commit_string, so
        // they are applied atomically. No counter increment is needed —
        // resetState() is called immediately below before the compositor's
        // surrounding_text echo can arrive, so the echo is harmless.
        if (m_inputPlugin && result.deleteBeforeCursor > 0) {
            m_inputPlugin->deleteSurroundingText(-result.deleteBeforeCursor, result.deleteBeforeCursor);
        }
        if (!result.commitText.isEmpty() && m_inputPlugin) {
            m_inputPlugin->commit(result.commitText);
        }
        resetState();
        break;
    case OverlayAction::StartTimer:
        // Flush any previous pending state to prevent character drops during
        // key rollover.
        if (!m_pendingText.isEmpty()) {
            // The trigger has already processed the new press. Reset only
            // controller state, not that trigger's newly armed timer state.
            if (!m_pendingKeyReleased) {
                m_ignoreReleaseNativeScanCode = m_pendingNativeScanCode;
                m_swallowNextRelease = true;
            }
            resetState();
        }

        // Set new pending state and start the hold timer
        m_pendingText = result.pendingText;
        m_pendingNativeScanCode = result.pendingNativeScanCode;
        m_pendingKeyReleased = false;
        m_pendingTrigger = trigger;
        if (result.timerDurationMs > 0) {
            // qCDebug(PlasmaKeyboard) << "Starting timer for" << m_pendingText << "duration" << result.timerDurationMs << "ms";
            m_holdTimer.start(result.timerDurationMs);
        }

        // Forward the key press to the client immediately, so the client can see the
        // key event and insert the base character into the text field without delay.
        //
        // The client will respond by inserting text and sending surrounding_text
        // back through the text-input protocol.
        //
        // Increment the echo counter and start the settle timer to prevent this
        // echo from being misidentified as an external cursor move (which would cancel the overlay).
        if (m_inputPlugin && !m_pendingText.isEmpty()) {
            // qCDebug(PlasmaKeyboard) << "Forwarding key press for" << m_pendingText;
            // The primary credit counter absorbs the first echo. The settle
            // timer absorbs any additional echoes from clients that send
            // multiple surrounding_text events per commit_string.
            ++m_pendingSurroundingTextUpdates;
            m_surroundingTextSettleTimer.start();
            m_inputPlugin->key(InputPlugin::Pressed, m_pendingNativeScanCode);
            // We need to simulate a key release to prevent the client from doing
            // auto-repeat on the key. We will swallow the next release event for this
            // key, so the client doesn't see a double release.
            m_inputPlugin->key(InputPlugin::Released, m_pendingNativeScanCode);
        }
        break;
    case OverlayAction::None:
        break;
    }
}

void OverlayController::handleOverlayGraceTimer()
{
    // If the key was already released while the overlay was visible, don't
    // start repeating — the overlay is still up waiting for selection.
    if (m_pendingKeyReleased || m_pendingText.isEmpty()) {
        return;
    }

    // qCDebug(PlasmaKeyboard) << "Overlay grace timer expired; starting repeat for" << m_pendingText;

    setOverlayVisible(false);

    // Send a synthetic key press so the client will begin key repeat.
    m_repeatNativeScanCode = m_pendingNativeScanCode;
    m_inputPlugin->key(InputPlugin::Pressed, m_repeatNativeScanCode);
    // No key release is sent here — the physical release (handled in processKeyRelease) sends
    // the final key(Released) to balance it.
}

void OverlayController::resetState()
{
    if (m_holdTimer.isActive()) {
        m_holdTimer.stop();
    }
    if (m_repeatNativeScanCode != 0) {
        // A synthetic repeat press must be balanced on every cancellation,
        // including rollover and context replacement. Remember to consume the
        // later physical release, since the client already received key-up.
        const quint32 key = m_repeatNativeScanCode;
        m_repeatNativeScanCode = 0;
        if (m_inputPlugin) {
            m_inputPlugin->key(InputPlugin::Released, key);
        }
        m_ignoreReleaseNativeScanCode = key;
        m_swallowNextRelease = true;
    }
    m_overlayGraceTimer.stop();

    const bool wasVisible = m_overlayVisible;
    const bool hadPending = !m_pendingText.isEmpty();
    const bool hadTrigger = !m_activeTriggerId.isEmpty();

    m_overlayVisible = false;
    m_pendingText.clear();
    m_pendingNativeScanCode = 0;
    m_pendingKeyReleased = false;
    m_activeTriggerId.clear();
    m_pendingTrigger = nullptr;
    const bool hadSelection = m_alternateSelection >= 0;
    const bool wasAlternatesOnly = m_alternatesOnly;
    m_alternatesOnly = false;
    m_alternateSelection = -1;
    m_candidateModel->clear();
    m_pendingSurroundingTextUpdates = 0;
    m_surroundingTextSettleTimer.stop();
    if (m_xkbComposeState) {
        xkb_compose_state_reset(m_xkbComposeState);
    }

    if (hadSelection) {
        Q_EMIT alternateSelectionChanged();
    }
    if (wasAlternatesOnly) {
        Q_EMIT alternatesOnlyChanged();
    }

    if (wasVisible) {
        Q_EMIT overlayVisibleChanged();
    }
    if (hadPending) {
        Q_EMIT pendingTextChanged();
    }
    if (hadTrigger) {
        Q_EMIT activeTriggerIdChanged();
    }
}

void OverlayController::setOverlayVisible(bool visible)
{
    if (m_overlayVisible == visible) {
        return;
    }
    m_overlayVisible = visible;
    Q_EMIT overlayVisibleChanged();
}

#include "moc_overlaycontroller.cpp"
