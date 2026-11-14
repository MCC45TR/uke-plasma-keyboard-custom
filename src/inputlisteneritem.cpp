/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "inputlisteneritem.h"
#include "laptop/laptopcontroller.h"
#include "clipboard/clipboardhistory.h"
#include "inputmethod_p.h"
#include "logging.h"
#include "plasmakeyboardsettings.h"
#include "touchholdwatcher.h"

#include "inputcontentpolicy.h"
#include "inputactivation.h"
#include "overlay/longpresstrigger.h"
#include "overlay/overlaycontroller.h"
#include "overlay/prefixquerytrigger.h"
#include "overlay/textexpansiontrigger.h"
#include "prediction/wordatcursor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextFormat>
#include <QTimer>
#include <QTouchEvent>
#include <functional>
#include <algorithm>
#include <utility>

#include <atomic>
#include <set>

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <qpa/qwindowsysteminterface.h>

#include <linux/input-event-codes.h>

Q_GLOBAL_STATIC(InputMethod, s_im)

namespace
{
// Timestamp (ms since epoch) until which the next input activation must show
// the keyboard immediately, bypassing the long-press mode.
std::atomic<qint64> s_forceShowUntil{0};
QPointer<InputListenerItem> s_inputListener;

// The keyboard window is a layer-shell surface, which the compositor never
// activates on its own, so Qt has to be told that the window holding the input
// item is the window the input goes to: without that Qt Virtual Keyboard does
// not consider the panel visible and refuses to show it (see
// PlatformInputContext::evaluateInputPanelVisible(), which needs the focus
// object of the focus window). The activation is announced the same way the
// compositor-driven show does it (see activateKeyboardWindow() in main.cpp).
void activateKeyboardWindow(QWindow *window)
{
    if (window) {
        QWindowSystemInterface::handleFocusWindowChanged(window, Qt::ActiveWindowFocusReason);
    }
}

//! Asks Qt Virtual Keyboard for the panel. The activation is announced first:
//! the panel itself appears when the activation reaches the input item, whose
//! onActiveChanged handler asks for the panel again with the input item focused.
void showInputPanel(QWindow *window)
{
    activateKeyboardWindow(window);
    QGuiApplication::inputMethod()->show();
}
} // namespace

void setInputPanelForceShowOnNextActivation()
{
    s_forceShowUntil.store(QDateTime::currentMSecsSinceEpoch() + 5000);

    // KWin does not re-activate an already active input context, so the
    // pending show must be applied right away in that case.
    if (s_inputListener && s_im->hasContext()) {
        s_forceShowUntil.store(0);
        s_inputListener->requestAutomaticShow(true);
    }
}

void requestInputPanelAutomaticShow()
{
    if (s_inputListener) s_inputListener->requestAutomaticShow();
}

void InputListenerItem::requestAutomaticShow(bool force, bool completedHold)
{
    if (!m_input.hasContext() || !m_sessionLockState.allowsPredictions()) return;
    if (auto *laptop = LaptopController::instance(); laptop && laptop->active()) return;
    if (!force && !completedHold && PlasmaKeyboardSettings::self()->showOnLongTap()) return;
    const auto generation = ++m_activationGeneration;
    if (force) {
        m_hiddenByUser = false;
        showInputPanel(window());
        return;
    }
    m_hiddenByUser = true;
    UkeKeyboard::requestTouchActivation(this, [this, generation](bool allowed) {
        if (!allowed || generation != m_activationGeneration || !m_input.hasContext() ||
            !m_sessionLockState.allowsPredictions()) return;
        if (auto *laptop = LaptopController::instance(); laptop && laptop->active()) return;
        m_hiddenByUser = false;
        showInputPanel(window());
    });
}

KeyboardModifiers::KeyboardModifiers(QObject *parent)
    : QObject(parent)
{
}

KeyboardModifiers *KeyboardModifiers::instance()
{
    static KeyboardModifiers s_instance;
    return &s_instance;
}

bool KeyboardModifiers::ctrl() const
{
    return m_ctrl;
}

void KeyboardModifiers::setCtrl(bool ctrl)
{
    if (m_ctrl == ctrl) {
        return;
    }
    m_ctrl = ctrl;
    Q_EMIT ctrlChanged();
}

bool KeyboardModifiers::alt() const
{
    return m_alt;
}

void KeyboardModifiers::setAlt(bool alt)
{
    if (m_alt == alt) {
        return;
    }
    m_alt = alt;
    Q_EMIT altChanged();
}

bool KeyboardModifiers::meta() const
{
    return m_meta;
}

void KeyboardModifiers::setMeta(bool meta)
{
    if (m_meta == meta) {
        return;
    }
    m_meta = meta;
    Q_EMIT metaChanged();
}

bool KeyboardModifiers::gamepadAvailable() const
{
    return m_gamepadAvailable;
}

void KeyboardModifiers::setGamepadAvailable(bool available)
{
    if (m_gamepadAvailable == available) {
        return;
    }
    m_gamepadAvailable = available;
    qCDebug(PlasmaKeyboard) << "KeyboardModifiers::gamepadAvailable ->" << available;
    Q_EMIT gamepadAvailableChanged();
}

bool KeyboardModifiers::extraRowsFocused() const
{
    return m_extraRowsFocused;
}

void KeyboardModifiers::setExtraRowsFocused(bool focused)
{
    if (focused == m_extraRowsFocused) {
        return;
    }

    m_extraRowsFocused = focused;
    Q_EMIT extraRowsFocusedChanged();
}

void KeyboardModifiers::reset()
{
    setCtrl(false);
    setAlt(false);
    setMeta(false);
}

Q_GLOBAL_STATIC_WITH_ARGS(const std::set<int>,
                          IGNORED_KEYS,
                          {
                              Qt::Key_Context1 // Triggered by "special keys" button
                          });
// Keys to always capture for keyboard navigation
QList<Qt::Key> initCapture()
{
    return {
        Qt::Key_Left,
        Qt::Key_Right,
        Qt::Key_Up,
        Qt::Key_Down,
    };
}
Q_GLOBAL_STATIC_WITH_ARGS(const QList<Qt::Key>, KEYBOARD_NAVIGATION_CAPTURE_KEYS, (initCapture()));

// Keys to capture when keyboard navigation is active
Q_GLOBAL_STATIC_WITH_ARGS(const QList<Qt::Key>, KEYBOARD_NAVIGATION_ACTIVE_CAPTURE_KEYS, (initCapture() + QList<Qt::Key>{Qt::Key_Return}));

InputListenerItem::InputListenerItem()
    : m_input(&(*s_im))
    , m_sessionLockState(*SessionLockState::instance())
    , m_overlayController(new OverlayController(&m_input, this))
    , m_clipboardHistory(new ClipboardHistory(&m_sessionLockState, this))
    , m_uinput(std::make_unique<UinputKeyboard>(this))
{
    s_inputListener = this;
    connect(this, &QQuickItem::activeFocusChanged, this, [this] { if (!hasActiveFocus()) releaseNormalKeys(); });
    // Grab and listen to physical keyboard input
    m_input.setGrabbing(true);

    // Register overlay triggers
    m_overlayController->registerTrigger(new LongPressTrigger(m_overlayController));
    m_overlayController->registerTrigger(new PrefixQueryTrigger(m_overlayController));
    m_overlayController->registerTrigger(new TextExpansionTrigger(m_overlayController));

    qApp->installEventFilter(this);
    connect(&m_input, &InputPlugin::contextAboutToChange, this, &InputListenerItem::releaseNormalKeys);
    if (auto *laptop = LaptopController::instance()) {
        connect(laptop, &LaptopController::stateChanged, this, [this, laptop, wasActive = laptop->active()]() mutable {
            if (wasActive == laptop->active()) return;
            wasActive = laptop->active();
            ++m_activationGeneration;
            m_touchHold.disarm();
            m_longPressNeedsHitTest = false;
            releaseNormalKeys();
            if (!laptop->active()) {
                m_hiddenByUser = true;
                QGuiApplication::inputMethod()->hide();
            }
        });
    }

    // Applications drop and re-take the text input for a moment on all kinds of
    // things (a click elsewhere in the window, a popup, a cursor move), so the
    // keyboard is only hidden once the input stayed gone for a while; see the
    // contextChanged handler below, which cancels a pending hide.
    m_hideDelay = new QTimer(this);
    m_hideDelay->setSingleShot(true);
    m_hideDelay->setInterval(300);
    connect(m_hideDelay, &QTimer::timeout, this, [] {
        QGuiApplication::inputMethod()->setVisible(false);
        QGuiApplication::inputMethod()->reset();
    });

    connect(this, &InputListenerItem::predictionTargetChanged, this, [this] {
        m_clipboardHistory->setAccessAllowed(predictionActive());
        if (!predictionActive() && m_overlayController) {
            m_overlayController->cancelOverlay();
        }
    });

    connect(&m_sessionLockState, &SessionLockState::unlocked, this, [this] {
        releaseNormalKeys();
        ++m_activationGeneration;
        // The panel belongs to the lock-screen field, not the desktop field
        // which may acquire focus immediately after authentication.
        m_hideDelay->stop();
        m_touchHold.disarm();
        m_longPressNeedsHitTest = false;
        m_hiddenByUser = true;
        QGuiApplication::inputMethod()->hide();
        QGuiApplication::inputMethod()->reset();
    });
    connect(&m_sessionLockState, &SessionLockState::stateChanged, this, [this] {
        Q_EMIT predictionTargetChanged();
        updatePredictionWords();
        // A lock notification is not an input-panel request. In particular,
        // the current context may still belong to an ordinary desktop field.
        if (!m_sessionLockState.isKnown() || m_sessionLockState.isPreparingLock()) {
            releaseNormalKeys();
            ++m_activationGeneration;
            m_hideDelay->stop();
            m_touchHold.disarm();
            m_longPressNeedsHitTest = false;
            m_hiddenByUser = true;
            QGuiApplication::inputMethod()->hide();
            QGuiApplication::inputMethod()->reset();
        }
    });

    connect(&m_input, &InputPlugin::contextChanged, this, [this] {
        ++m_activationGeneration;
        const bool hasContext = m_input.hasContext();
        if(auto *laptop=LaptopController::instance())laptop->textFocusChanged(hasContext);

        // Invalidate asynchronous results even if the new field has the same word.
        Q_EMIT predictionTargetChanged();
        updatePredictionWords();

        // Overlay cleanup runs on contextAboutToChange, before the old
        // context is dropped, so synthetic releases reach the correct client.

        // During lock, KWin's explicit panel visibility is authoritative.
        // An activation/adopt can still carry a stale desktop context.
        if (!m_sessionLockState.allowsPredictions()) {
            m_hideDelay->stop();
            m_touchHold.disarm();
            m_longPressNeedsHitTest = false;
            QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
            return;
        }

        if(auto *laptop=LaptopController::instance();laptop && laptop->active()) {
            m_hideDelay->stop();m_touchHold.disarm();m_longPressNeedsHitTest=false;
            m_hiddenByUser=!laptop->active();
            QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
            return;
        }

        if (hasContext && m_hideDelay->isActive()) {
            m_hideDelay->stop();
            if (window()->isVisible()) {
                // The input came back before the pending hide: the keyboard
                // stays as it was, without waiting for a long press again.
                m_hiddenByUser = false;
                QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
                return;
            }
        }

        if (hasContext) {
            // A new activation is a new request to type: the keyboard may be
            // shown again even if it was hidden before.
            m_hiddenByUser = false;

            // In long-press mode the keyboard stays hidden until a touch is
            // held on the screen long enough; fall back to the immediate
            // show when no touchscreen can be watched.
            // The global shortcut force-activates the input method; show
            // immediately in that case instead of waiting for a long press.
            const bool forceShow = s_forceShowUntil.load() > QDateTime::currentMSecsSinceEpoch();
            if (forceShow) {
                s_forceShowUntil.store(0);
            }

            const bool longTapSetting = PlasmaKeyboardSettings::self()->showOnLongTap();
            // The lock screen cannot expose the regular session's AT-SPI tree,
            // so long-press hit testing can never prove that the touch belongs
            // to its password field. A lock-screen input context is already an
            // explicit request to type and must show immediately.
            const bool lockScreenActive = m_sessionLockState.isKnown() && m_sessionLockState.isLocked();
            const bool waitForLongTouch = !forceShow && !lockScreenActive && longTapSetting;
            const int longTouchThreshold = PlasmaKeyboardSettings::self()->showOnLongTapThresholdMs();
            const bool watchingTouch = waitForLongTouch && m_touchHold.arm(longTouchThreshold);
            // evdev input is global, so only the touch which caused this text
            // input activation may arm the hold timer. A mouse click (or any
            // other non-touch activation) must not bypass long-press mode.
            const bool armed = watchingTouch && m_touchHold.touchIsDown();
            const bool completedLongTouch = watchingTouch && m_touchHold.takeCompletedLongPress(longTouchThreshold);
            if (watchingTouch && !armed && !completedLongTouch) {
                // A mouse activation (or an already completed short tap) has
                // not proved that the next hold belongs to the text field.
                // Keep watching, but require the AT-SPI hit test on every hold.
                m_touchHold.watchContinuously(longTouchThreshold);
            }
            m_longPressNeedsHitTest = watchingTouch && !armed && !completedLongTouch;
            // Suppress both the immediate show and the surrounding-text
            // re-show path for every automatic activation in long-press mode.
            // A successful hold clears this flag in the longPress handler.
            m_hiddenByUser = watchingTouch && !completedLongTouch;
            QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
            qCDebug(PlasmaKeyboard) << "contextChanged hasContext=" << hasContext << "showOnLongTap=" << longTapSetting << "forceShow=" << forceShow
                                    << "lockScreenActive=" << lockScreenActive << "armed=" << armed;
            if (!watchingTouch || completedLongTouch) {
                requestAutomaticShow(forceShow, completedLongTouch);
            }
        } else if (PlasmaKeyboardSettings::self()->hideOnInputFocusLoss()) {
            m_hideDelay->start();
        }
    });
    connect(&m_input, &InputPlugin::surroundingTextChanged, this, [this] {
        // The word being typed changed with the text around the cursor, so the
        // suggestions have to follow it.
        updatePredictionWords();

        // Notify the overlay controller first so it can cancel the overlay if an
        // external cursor movement is detected (e.g. user tapped elsewhere in the
        // text field while the diacritics overlay was open or the hold timer was
        // still running). The controller uses an internal counter to distinguish
        // its own commit_string echoes from external cursor moves.
        if (predictionActive() && m_overlayController) {
            m_overlayController->handleSurroundingTextChanged();
        }

        if (m_input.hasContext()) {
            // Re-activate when text input activates, and there is context.
            // Never do it while the keyboard was hidden on purpose: the field
            // keeps sending updates and the panel would pop up again at once.
            if (m_sessionLockState.allowsPredictions() && !m_hiddenByUser && !m_touchHold.isArmed() && !window()->isVisible()) {
                requestAutomaticShow();
            }

            // Update vkbd input method only if the virtual keyboard panel is shown
            if (window()->isExposed()) {
                QGuiApplication::inputMethod()->update(Qt::ImSurroundingText);
            }
        }
    });
    connect(&m_input, &InputPlugin::contentTypeChanged, this, [this] {
        // A normal field can become protected without changing input context.
        // Cancel in-flight results before exposing any new word to QML.
        Q_EMIT predictionTargetChanged();
        updatePredictionWords();
        QGuiApplication::inputMethod()->update(Qt::ImHints | Qt::ImSurroundingText);
    });
    connect(&m_input, &InputPlugin::cursorChanged, this, [this] {
        // Moving the cursor moves the word that is being typed.
        updatePredictionWords();
    });

    connect(&m_input, &InputPlugin::deactivate, this, [this] {
        ++m_activationGeneration;
        m_touchHold.disarm();
        // The input context is gone: the next activation may show the keyboard
        // again, even if the user had hidden it.
        m_hiddenByUser = false;
        updatePredictionWords();
        if (PlasmaKeyboardSettings::self()->hideOnInputFocusLoss()) {
            m_hideDelay->start();
        }
    });

    connect(&m_touchHold, &TouchHoldWatcher::longPress, this, [this] {
        if (!m_sessionLockState.allowsPredictions()) {
            return;
        }
        if (m_longPressNeedsHitTest) {
            if (!m_focusedEditable.hasFocusedEditable()) {
                // A previously mouse-focused field does not authorize an
                // unrelated touch when its bounds are unknown.
                return;
            } else if (!m_focusedEditable.containsNormalizedTouch(m_touchHold.normalizedPosition())) {
                qCDebug(PlasmaKeyboard) << "TouchHoldWatcher: hold was outside the focused editable widget";
                return;
            }
        }
        qCDebug(PlasmaKeyboard) << "TouchHoldWatcher: long press detected, showing keyboard";
        m_touchHold.disarm();
        m_longPressNeedsHitTest = false;
        requestAutomaticShow(false, true);
    });
    connect(&m_input, &InputPlugin::resetRequested, this, [this] {
        QGuiApplication::inputMethod()->reset();
    });
    connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged, this, [this] {
        const bool visible = QGuiApplication::inputMethod()->isVisible();
        if (!visible) {
            releaseNormalKeys();
            ++m_activationGeneration;
            // The keyboard was hidden (by the user, or because the input
            // context went away). Remember it, so that a text field which keeps
            // sending updates cannot bring the panel back on its own.
            m_hiddenByUser = true;
            if (m_sessionLockState.allowsPredictions() && m_input.hasContext() && PlasmaKeyboardSettings::self()->showOnLongTap()
                && m_touchHold.watchContinuously(PlasmaKeyboardSettings::self()->showOnLongTapThresholdMs())) {
                m_longPressNeedsHitTest = true;
            }
        } else {
            m_touchHold.disarm();
            m_longPressNeedsHitTest = false;
        }
        window()->setVisible(visible || (LaptopController::instance() && LaptopController::instance()->active()));
    });

    connect(&m_input, &InputPlugin::keyPressed, this, [this](QKeyEvent *keyEvent) {
        if (m_nativePcInput) return;
        // qCDebug(PlasmaKeyboard) << "keyPressed. keycode:" << keyEvent->key() << "text:" << keyEvent->text() << "modifiers:" << keyEvent->modifiers();

        // Delegate to overlay controller for diacritics/emoji/text expansion
        if (predictionActive() && m_overlayController && m_overlayController->processKeyPress(keyEvent)) {
            keyEvent->accept();
            return;
        }
        // The window can have isVisible() = true, but not be shown by the compositor if the input panel is suppressed
        if (!window()->isExposed()) {
            return;
        }

        if (PlasmaKeyboardSettings::self()->keyboardNavigationEnabled()) {
            // Keys to capture for keyboard navigation
            const auto keys = m_keyboardNavigationActive ? *KEYBOARD_NAVIGATION_ACTIVE_CAPTURE_KEYS : *KEYBOARD_NAVIGATION_CAPTURE_KEYS;

            // Forward and accept keyboard navigation events
            for (const auto key : keys) {
                if (keyEvent->key() == key) {
                    keyEvent->accept();
                    Q_EMIT keyNavigationPressed(key);
                    break;
                }
            }
        }
    });
    connect(&m_input, &InputPlugin::keyReleased, this, [this](QKeyEvent *keyEvent) {
        if (m_nativePcInput) return;
        // qCDebug(PlasmaKeyboard) << "keyReleased. keycode:" << keyEvent->key() << "text:" << keyEvent->text();

        // Let the overlay controller handle release events (e.g. diacritics, emoji)
        if (m_overlayController && m_overlayController->processKeyRelease(keyEvent)) {
            keyEvent->accept();
            return;
        }
        // The window can have isVisible() = true, but not be shown by the compositor if the input panel is suppressed
        if (!window()->isExposed()) {
            return;
        }

        if (PlasmaKeyboardSettings::self()->keyboardNavigationEnabled()) {
            // Keys to capture for keyboard navigation
            const auto keys = m_keyboardNavigationActive ? *KEYBOARD_NAVIGATION_ACTIVE_CAPTURE_KEYS : *KEYBOARD_NAVIGATION_CAPTURE_KEYS;

            // Forward and accept keyboard navigation events
            for (const auto key : keys) {
                if (keyEvent->key() == key) {
                    keyEvent->accept();
                    Q_EMIT keyNavigationReleased(key);
                    break;
                }
            }
        }
    });

    // Don't hook into the &InputPlugin::receivedCommit signal and call setVisible(true)
    // -> it can cause a race condition as receivedCommit is emitted when the text field loses focus.
    //
    // If the virtual keyboard (vkbd) is manually closed and then the text field loses focus,
    // setVisible(true/false) calls in quick succession may reopen the vkbd in a broken state.
    //
    // Series of events:
    // - vkbd is manually closed (by user), but text field still focused -> setVisible(false)
    // - User unfocuses the text field
    // - receivedCommit() emitted -> setVisible(true)
    // - contextChanged() (m_input.hasContext() = false) -> setVisible(false)
    // - keyboard gets reopened by KWin because the input context gets from setVisible(true) finishes initializing!
    //
    // This happens because of a delay in InputPanelV1Window initialization, which confuses KWin
    // into creating a new input context even when no text field is focused.
    //
    // TODO: Investigate other places this might happen and how to fix it.

    QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
}

OverlayController *InputListenerItem::overlayController() const
{
    return m_overlayController;
}

ClipboardHistory *InputListenerItem::clipboardHistory() const
{
    return m_clipboardHistory;
}

void InputListenerItem::commitText(const QString &text)
{
    if (text.isEmpty() || !m_input.hasContext()) {
        return;
    }

    m_input.commit(text);
}

void InputListenerItem::pasteClipboardEntry(int row)
{
    if (!predictionActive()) {
        return;
    }
    commitText(m_clipboardHistory->textAt(row));
}

QString InputListenerItem::textBeforeCursor() const
{
    if (!predictionActive() || !PlasmaKeyboardSettings::self()->predictiveTextEnabled()) {
        return {};
    }

    // The cursor position is in bytes, the word is cut in characters.
    const QByteArray surrounding = m_input.surroundingText().toUtf8();
    const int cursorBytes = qBound(0, int(m_input.cursorPos()), surrounding.size());
    return QString::fromUtf8(surrounding.first(cursorBytes));
}

QString InputListenerItem::predictionPrefix() const
{
    return wordBeforeCursor(textBeforeCursor());
}

QString InputListenerItem::predictionContext() const
{
    return previousWordBeforeCursor(textBeforeCursor());
}

bool InputListenerItem::sensitiveInput() const
{
    return m_sensitiveInput;
}

bool InputListenerItem::atSentenceStart() const
{
    const QString before = textBeforeCursor();
    if (before.trimmed().isEmpty()) {
        return true;
    }
    // The space after a sentence end may already be inserted by the keyboard.
    static const QRegularExpression endOfSentence(QStringLiteral("(^|[.!?…]|\\n)[\\s\"'»”’)\\]]*$"));
    return endOfSentence.match(before).hasMatch();
}

void InputListenerItem::updatePredictionWords()
{
    const QString prefix = predictionPrefix();
    if (prefix != m_predictionPrefix) {
        m_predictionPrefix = prefix;
        Q_EMIT predictionPrefixChanged();
    }

    const QString context = predictionContext();
    if (context != m_predictionContext) {
        m_predictionContext = context;
        Q_EMIT predictionContextChanged();
    }
}

void InputListenerItem::applyPrediction(const QString &word)
{
    if (word.isEmpty() || !predictionActive() || !PlasmaKeyboardSettings::self()->predictiveTextEnabled()) {
        return;
    }

    // A picked word is typed as a whole, with the space after it so that the
    // next word can be typed right away, the way the on-screen keyboards do it.
    const QString committed = word + QLatin1Char(' ');

    const QString prefix = predictionPrefix();
    if (prefix.isEmpty()) {
        commitText(committed);
        return;
    }

    // Replace the word that was typed with the suggestion: the replacement is
    // what the clients expect for a completion, and inputMethodEvent() already
    // knows how to send it over the wayland protocol.
    QInputMethodEvent event;
    event.setCommitString(committed, -prefix.size(), prefix.size());
    inputMethodEvent(&event);
}

void InputListenerItem::commitSwipeWord(const QString &word)
{
    if(!predictionActive()){m_committedSwipe.clear();return;}
    if (word.isEmpty() || !m_input.hasContext()) {
        return;
    }

    const QString committed = word + QLatin1Char(' ');
    const QString prefix = predictionPrefix();
    if (prefix.isEmpty()) {
        commitText(committed);
    } else {
        QInputMethodEvent event;
        event.setCommitString(committed, -prefix.size(), prefix.size());
        inputMethodEvent(&event);
    }
    // An alternative offered for this swipe may replace exactly this text.
    m_committedSwipe = committed;
}

void InputListenerItem::replaceSwipeWord(const QString &word)
{
    if(!predictionActive()){m_committedSwipe.clear();return;}
    if (word.isEmpty() || m_committedSwipe.isEmpty() || !m_input.hasContext()) {
        return;
    }
    // Only replace when the inserted word is still right before the cursor: the
    // text may have been edited in the field since the swipe.
    if (!textBeforeCursor().endsWith(m_committedSwipe)) {
        m_committedSwipe.clear();
        return;
    }

    const QString committed = word + QLatin1Char(' ');
    QInputMethodEvent event;
    event.setCommitString(committed, -m_committedSwipe.size(), m_committedSwipe.size());
    inputMethodEvent(&event);
    m_committedSwipe = committed;
}

void InputListenerItem::setEngine(QVirtualKeyboardInputEngine *engine)
{
    if (m_engine != engine) {
        if (m_engine) disconnect(m_engine, nullptr, this, nullptr);
        m_engine = engine;
        if (m_engine) connect(m_engine, &QVirtualKeyboardInputEngine::activeKeyChanged, this, [this](Qt::Key key) {
            if (!m_engine || (!m_touchOwnedKeys.contains(key) && !m_touchOwnedModifiers.contains(key))) return;
            // Qt's engine commits on release, too late for physical chords.
            // Cancel only the keys already forwarded by our own touch path.
            m_engine->virtualKeyCancel();
            Q_EMIT physicalChordCaptured();
            for (const auto &item : std::as_const(m_heldItems)) if (item) item->setProperty("active", true);
        });
    }
    // The overlay controller inserts a picked alternate character as a key
    // click through the engine, the way the on-screen alternate-keys popup
    // does; a bare commit_string makes clients end the input session instead.
    if (m_overlayController) {
        m_overlayController->setInputEngine(engine);
    }
}

QVariant InputListenerItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (!m_input.hasContext()) {
        if (query == Qt::ImEnabled) {
            return false;
        }
        return {};
    }

    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImSurroundingText:
        return m_input.surroundingText();
    case Qt::ImHints: {
        const auto imHints = m_input.contentHint();
        Qt::InputMethodHints qtHints = InputContentPolicy::privacyHints(imHints, m_input.contentPurpose());

        // Disable Qt VirtualKeyboard's hunspell plugin
        // It deals poorly with current external cursor position change
        // logic, and causes problems when selecting text with a mouse.
        qtHints |= Qt::ImhNoPredictiveText;

        // if (imHints & InputPlugin::content_hint_default) { }
        if (imHints & InputPlugin::content_hint_password) {
            qtHints |= Qt::ImhSensitiveData;
        }
        if ((imHints & InputPlugin::content_hint_auto_capitalization) == 0 || !PlasmaKeyboardSettings::self()->autoCapitalizationEnabled()) {
            qtHints |= Qt::ImhNoAutoUppercase;
        }
        // if (imHints & InputPlugin::content_hint_titlecase) { }
        if (imHints & InputPlugin::content_hint_lowercase) {
            qtHints |= Qt::ImhPreferLowercase;
        }
        if (imHints & InputPlugin::content_hint_uppercase) {
            qtHints |= Qt::ImhPreferUppercase;
        }
        if (imHints & InputPlugin::content_hint_hidden_text) {
            qtHints |= Qt::ImhSensitiveData;
        }
        if (imHints & InputPlugin::content_hint_sensitive_data) {
            qtHints |= Qt::ImhSensitiveData;
        }
        // The personal words of the swipe engine are not learned from a field
        // that asks not to be remembered.
        const bool sensitive =
            (imHints & (InputPlugin::content_hint_password | InputPlugin::content_hint_hidden_text | InputPlugin::content_hint_sensitive_data))
            || m_input.contentPurpose() == InputPlugin::content_purpose_password;
        m_sensitiveInput = sensitive;
        if (imHints & InputPlugin::content_hint_latin) {
            qtHints |= Qt::ImhPreferLatin;
        }
        if (imHints & InputPlugin::content_hint_multiline) {
            qtHints |= Qt::ImhMultiLine;
        }
        const auto imPurpose = m_input.contentPurpose();
        switch (imPurpose) {
        case InputPlugin::content_purpose_normal:
        case InputPlugin::content_purpose_alpha:
        case InputPlugin::content_purpose_name:
            break;
        case InputPlugin::content_purpose_digits:
            qtHints |= Qt::ImhDigitsOnly;
            break;
        case InputPlugin::content_purpose_number:
            qtHints |= Qt::ImhPreferNumbers;
            break;
        case InputPlugin::content_purpose_phone:
            qtHints |= Qt::ImhDialableCharactersOnly;
            break;
        case InputPlugin::content_purpose_url:
            qtHints |= Qt::ImhUrlCharactersOnly;
            break;
        case InputPlugin::content_purpose_email:
            qtHints |= Qt::ImhEmailCharactersOnly;
            break;
        case InputPlugin::content_purpose_password:
            qtHints |= Qt::ImhSensitiveData;
            break;
        case InputPlugin::content_purpose_date:
            qtHints |= Qt::ImhDate;
            break;
        case InputPlugin::content_purpose_time:
            qtHints |= Qt::ImhTime;
            break;
        case InputPlugin::content_purpose_datetime:
            qtHints |= Qt::ImhDate;
            qtHints |= Qt::ImhTime;
            break;
        case InputPlugin::content_purpose_terminal:
            qtHints |= Qt::ImhPreferLatin;
            break;
        }
        return QVariant::fromValue<int>(qtHints);
    }
    case Qt::ImCurrentSelection: {
        // cursorPos and anchorPos are in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int start = qMin(m_input.cursorPos(), m_input.anchorPos());
        int end = qMax(m_input.cursorPos(), m_input.anchorPos());
        return QString::fromUtf8(surroundingText.mid(start, end - start));
    }
    case Qt::ImAnchorPosition: {
        // anchorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int anchorBytes = qBound(0, int(m_input.anchorPos()), surroundingText.size());
        return QString::fromUtf8(surroundingText.first(anchorBytes)).length();
    }
    case Qt::ImCursorPosition: {
        // cursorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int cursorBytes = qBound(0, int(m_input.cursorPos()), surroundingText.size());
        return QString::fromUtf8(surroundingText.first(cursorBytes)).length();
    }
    case Qt::ImTextBeforeCursor: {
        // cursorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int cursorBytes = qBound(0, int(m_input.cursorPos()), surroundingText.size());
        return QString::fromUtf8(surroundingText.first(cursorBytes));
    }
    case Qt::ImTextAfterCursor: {
        // cursorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        return QString::fromUtf8(surroundingText.mid(m_input.cursorPos()));
    }
    case Qt::ImCursorRectangle:
    case Qt::ImFont:
    case Qt::ImMaximumTextLength:
    case Qt::ImPreferredLanguage:
    case Qt::ImPlatformData:
    case Qt::ImAbsolutePosition:
    case Qt::ImEnterKeyType:
    case Qt::ImAnchorRectangle:
    case Qt::ImInputItemClipRectangle:
    case Qt::ImReadOnly:
        // We don't do that
        break;
    default:
        qWarning() << "Unhandled query" << query;
        break;
    }
    return {};
}

InputListenerItem::~InputListenerItem()
{
    blockSignals(true);
    releaseNormalKeys();
}

void InputListenerItem::setNativePcInput(bool native)
{
    if (m_nativePcInput == native) return;
    releaseNormalKeys();
    m_nativePcInput = native;
}

bool InputListenerItem::touchHoldsModifier(int key) const
{
    return std::any_of(m_modifierTouches.cbegin(), m_modifierTouches.cend(), [key](int held) { return held == key; });
}

QQuickItem *InputListenerItem::keyAt(const QPointF &position) const
{
    if (!window()) return nullptr;
    std::function<QQuickItem *(QQuickItem *)> find = [&](QQuickItem *item) -> QQuickItem * {
        if (!item->isVisible() || !item->isEnabled() || item->opacity() <= 0) return nullptr;
        const auto local = item->mapFromScene(position);
        if (item->clip() && !item->contains(local)) return nullptr;
        const auto children = item->childItems();
        for (auto it = children.crbegin(); it != children.crend(); ++it) {
            if (auto *key = find(*it)) return key;
        }
        const int key = item->property("key").toInt();
        return key && key != Qt::Key_unknown && item->contains(local) ? item : nullptr;
    };
    return find(window()->contentItem());
}

bool InputListenerItem::eventFilter(QObject *watched, QEvent *event)
{
    if (m_nativePcInput || watched != window() || !hasActiveFocus()) return false;
    if (event->type() == QEvent::TouchCancel) {
        releaseNormalKeys();
    } else if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd) {
        const auto *touch = static_cast<QTouchEvent *>(event);
        // Admit modifiers first, including simultaneous two-finger presses.
        for (const auto &point : touch->points()) {
            if (point.state() != QEventPoint::State::Pressed) continue;
            auto *item = keyAt(point.position());
            const int key = item ? item->property("key").toInt() : 0;
            if (key != Qt::Key_Control && key != Qt::Key_Alt && key != Qt::Key_Meta) continue;
            m_modifierTouches.insert(point.id(), key);
            m_touchOwnedModifiers.insert(key);
            m_heldItems.insert(key, item);
            setModifierHeld(static_cast<Qt::Key>(key), true);
        }
        for (const auto &point : touch->points()) {
            if (point.state() != QEventPoint::State::Released) continue;
            if (m_modifierTouches.contains(point.id())) {
                const int key = m_modifierTouches.take(point.id());
                if (!touchHoldsModifier(key)) {
                    setModifierHeld(static_cast<Qt::Key>(key), false);
                    m_touchOwnedModifiers.remove(key);
                    if (auto item = m_heldItems.take(key)) item->setProperty("active", false);
                }
            }
            if (m_shortcutTouches.contains(point.id())) {
                const int key = m_shortcutTouches.take(point.id());
                if (!m_shortcutTouches.values().contains(key)) {
                    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
                    handleModifiedKey(&release, false);
                    m_touchOwnedKeys.remove(key);
                    if (auto item = m_heldItems.take(key)) item->setProperty("active", false);
                }
            }
        }
        const auto *held = KeyboardModifiers::instance();
        if (held->ctrl() || held->alt() || held->meta()) {
            for (const auto &point : touch->points()) {
                if (point.state() != QEventPoint::State::Pressed || m_modifierTouches.contains(point.id())) continue;
                auto *item = keyAt(point.position());
                if (!item) continue;
                const int key = item->property("key").toInt();
                const bool uppercase = item->property("uppercased").toBool();
                const auto text = item->property("text").toString();
                QKeyEvent press(QEvent::KeyPress, key, uppercase ? Qt::ShiftModifier : Qt::NoModifier, uppercase ? text.toUpper() : text);
                if (!handleModifiedKey(&press, true)) continue;
                m_shortcutTouches.insert(point.id(), key);
                m_touchOwnedKeys.insert(key);
                m_heldItems.insert(key, item);
            }
        }
    }
    return false; // Qt retains layout, visual touch and ordinary IME handling.
}

void InputListenerItem::syncModifiers(Qt::KeyboardModifiers extra)
{
    if (m_uinput && m_uinput->isValid()) return;
    const auto base = m_input.modifierState();
    const auto *held = KeyboardModifiers::instance();
    const uint32_t depressed = base[0] | (held->ctrl() ? m_input.controlMask() : 0)
        | (held->alt() ? m_input.altMask() : 0) | (held->meta() ? m_input.metaMask() : 0)
        | ((extra & Qt::ShiftModifier) ? m_input.shiftMask() : 0);
    m_input.sendModifiers(depressed, base[1], base[2], base[3]);
}

void InputListenerItem::setModifierHeld(Qt::Key key, bool held)
{
    auto *state = KeyboardModifiers::instance();
    switch (key) {
    case Qt::Key_Control: state->setCtrl(held); break;
    case Qt::Key_Alt: state->setAlt(held); break;
    case Qt::Key_Meta: state->setMeta(held); break;
    default: return;
    }
    if (held) pressUinputModifier(key);
    else releaseUinputModifierIfHeld(key);
    syncModifiers();
}

void InputListenerItem::releaseNormalKeys()
{
    for (const auto &key : std::as_const(m_modifiedKeys)) {
        if (key.uinput && m_uinput) m_uinput->releaseKey(key.code);
        else m_input.key(InputPlugin::Released, key.code);
    }
    m_modifiedKeys.clear();
    if (m_engine && (!m_modifierTouches.isEmpty() || !m_shortcutTouches.isEmpty())) {
        m_engine->virtualKeyCancel();
        Q_EMIT physicalChordCaptured();
    }
    for (const auto &item : std::as_const(m_heldItems)) if (item) item->setProperty("active", false);
    m_heldItems.clear();
    m_shortcutTouches.clear();
    m_touchOwnedKeys.clear();
    m_modifierTouches.clear();
    m_touchOwnedModifiers.clear();
    m_engineModifiers.clear();
    releaseAllHeldUinputModifiers();
    KeyboardModifiers::instance()->reset();
    syncModifiers();
}

void InputListenerItem::pressUinputModifier(Qt::Key key)
{
    if (!m_uinput || !m_uinput->isValid()) {
        return;
    }
    switch (key) {
    case Qt::Key_Control:
        if (!m_uinputCtrlHeld) {
            m_uinput->pressKey(KEY_LEFTCTRL);
            m_uinputCtrlHeld = true;
        }
        break;
    case Qt::Key_Alt:
        if (!m_uinputAltHeld) {
            m_uinput->pressKey(KEY_LEFTALT);
            m_uinputAltHeld = true;
        }
        break;
    case Qt::Key_Meta:
        if (!m_uinputMetaHeld) {
            m_uinput->pressKey(KEY_LEFTMETA);
            m_uinputMetaHeld = true;
        }
        break;
    default:
        break;
    }
}

void InputListenerItem::releaseUinputModifierIfHeld(Qt::Key key)
{
    if (!m_uinput) {
        return;
    }
    switch (key) {
    case Qt::Key_Control:
        if (m_uinputCtrlHeld) {
            m_uinput->releaseKey(KEY_LEFTCTRL);
            m_uinputCtrlHeld = false;
        }
        break;
    case Qt::Key_Alt:
        if (m_uinputAltHeld) {
            m_uinput->releaseKey(KEY_LEFTALT);
            m_uinputAltHeld = false;
        }
        break;
    case Qt::Key_Meta:
        if (m_uinputMetaHeld) {
            m_uinput->releaseKey(KEY_LEFTMETA);
            m_uinputMetaHeld = false;
        }
        break;
    default:
        break;
    }
}

void InputListenerItem::releaseAllHeldUinputModifiers()
{
    releaseUinputModifierIfHeld(Qt::Key_Control);
    releaseUinputModifierIfHeld(Qt::Key_Alt);
    releaseUinputModifierIfHeld(Qt::Key_Meta);
}

bool InputListenerItem::handleModifiedKey(QKeyEvent *event, bool press)
{
    // The release follows the original route even if the modifier finger was
    // lifted first. Releasing a letter must never release other held keys.
    if (!press) {
        const auto found = m_modifiedKeys.find(event->key());
        if (found == m_modifiedKeys.end()) return false;
        const auto key = *found;
        m_modifiedKeys.erase(found);
        if (key.uinput && m_uinput) m_uinput->releaseKey(key.code);
        else { m_input.key(InputPlugin::Released, key.code); syncModifiers(); }
        return true;
    }
    if (m_modifiedKeys.contains(event->key())) return true;
    const auto *held = KeyboardModifiers::instance();
    if (!held->ctrl() && !held->alt() && !held->meta()) return false;
    const auto symbols = QXkbCommon::toKeysym(event);
    const uint32_t code = symbols.isEmpty() ? 0 : m_input.keycodeForKeysym(symbols.first());
    if (!code) return false;
    const bool uinput = m_uinput && m_uinput->isValid();
    m_modifiedKeys.insert(event->key(), {code, uinput});
    if (uinput) m_uinput->pressKey(code);
    else { syncModifiers(event->modifiers()); m_input.key(InputPlugin::Pressed, code); }
    return true;
}

void InputListenerItem::sendKeyEvent(int key, const QString &text)
{
    QKeyEvent pressEvent(QEvent::KeyPress, key, Qt::NoModifier, text);
    QKeyEvent releaseEvent(QEvent::KeyRelease, key, Qt::NoModifier, text);
    keyPressEvent(&pressEvent);
    keyReleaseEvent(&releaseEvent);
}

QStringList InputListenerItem::alternatesFor(const QVariantList &alternatives, bool uppercase) const
{
    QStringList result;
    result.reserve(alternatives.size());

    for (const QVariant &entry : alternatives) {
        const QString character = entry.toString();
        if (character.isEmpty()) {
            continue;
        }
        // Qt Virtual Keyboard uppercases its own alternate-keys popup from the
        // keyboard state, not from the key text, so a shifted keyboard offers
        // shifted alternates here too.
        result.append(uppercase ? character.toUpper() : character);
    }

    return result;
}

void InputListenerItem::keyPressEvent(QKeyEvent *event)
{
    if (m_touchOwnedKeys.contains(event->key())) { event->accept(); return; }
    if (IGNORED_KEYS->find(event->key()) != IGNORED_KEYS->end()) {
        return;
    }

    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        m_engineModifiers.insert(event->key());
        if (!m_touchOwnedModifiers.contains(event->key())) setModifierHeld(static_cast<Qt::Key>(event->key()), true);
        event->accept();
        return;
    }

    if (handleModifiedKey(event, true)) {
        event->accept();
        return;
    }

    const QList<xkb_keysym_t> keys = QXkbCommon::toKeysym(event);
    for (auto key : keys) {
        // Simulate key press only if it's not textual
        if (event->text().isEmpty() || key == XKB_KEY_Return) { // (return is technically "\n")
            m_input.keysym(QDateTime::currentMSecsSinceEpoch(), key, InputPlugin::Pressed, 0);
        }
    }
}

void InputListenerItem::keyReleaseEvent(QKeyEvent *event)
{
    if (m_touchOwnedKeys.contains(event->key())) { event->accept(); return; }
    if (IGNORED_KEYS->find(event->key()) != IGNORED_KEYS->end()) {
        return;
    }

    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        m_engineModifiers.remove(event->key());
        if (m_touchOwnedModifiers.contains(event->key())) {
            if (!touchHoldsModifier(event->key())) m_touchOwnedModifiers.remove(event->key());
        } else {
            setModifierHeld(static_cast<Qt::Key>(event->key()), false);
        }
        event->accept();
        return;
    }
    if (handleModifiedKey(event, false)) {
        event->accept();
        return;
    }

    const QList<xkb_keysym_t> keys = QXkbCommon::toKeysym(event);
    for (auto key : keys) {
        if (event->text().isEmpty() || key == XKB_KEY_Return) { // (return is technically "\n")
            // Simulate the keyboard press for non textual keys
            m_input.keysym(QDateTime::currentMSecsSinceEpoch(), key, InputPlugin::Released, 0);
        } else {
            // If we have text coming as a key event, use it to commit the string
            m_input.commit(event->text());
        }
    }
}

void InputListenerItem::inputMethodEvent(QInputMethodEvent *event)
{
    // Don't forward events from Qt VKBD to KWin if the keyboard window isn't open
    // This avoids the possibility of Qt VKBD causing text changes from receiving
    // events (ex. cursor movement) done by its input engine (for features like text prediction).
    if (!window()->isExposed()) {
        return;
    }

    QString commit = event->commitString();
    QString preedit = event->preeditString();
    bool needsReplacement = event->replacementStart() != 0 || event->replacementLength() != 0;

    // Delete characters that are supposed to be replaced
    if (needsReplacement) {
        // The positions we send to need to be in bytes (to support UTF8)
        QString surroundingText = m_input.surroundingText();
        QByteArray surroundingUtf8 = surroundingText.toUtf8();
        int boundedCursorBytes = qBound(0, int(m_input.cursorPos()), surroundingUtf8.size());
        int cursorChars = QString::fromUtf8(surroundingUtf8.first(boundedCursorBytes)).size();
        int startChars = qBound(0, cursorChars + event->replacementStart(), surroundingText.size());
        int endChars = qBound(startChars, startChars + event->replacementLength(), surroundingText.size());

        int startBytes = surroundingText.first(startChars).toUtf8().size();
        int endBytes = surroundingText.first(endChars).toUtf8().size();

        if (endBytes > startBytes) {
            m_input.deleteSurroundingText(startBytes - boundedCursorBytes, endBytes - startBytes);
        }
    }

    // Commit string if there is something to commit, or we did a replacement
    if (needsReplacement || !commit.isEmpty()) {
        m_input.commit(commit);
    }

    // Set attributes for style (ex. needed for CJK)
    for (const auto &x : event->attributes()) {
        if (x.type == QInputMethodEvent::TextFormat) {
            int startBytes = preedit.first(qBound(0, x.start, preedit.size())).toUtf8().size();
            int endBytes = preedit.first(qBound(0, x.start + x.length, preedit.size())).toUtf8().size();
            m_input.setPreEditStyle(startBytes, endBytes - startBytes, x.value.value<QTextFormat>().type());
        }
    }

    // Send cursor position (must be before preedit string)
    int preEditCursorPos = preedit.toUtf8().size();
    for (const auto &attribute : event->attributes()) {
        if (attribute.type != QInputMethodEvent::Cursor) {
            continue;
        }

        // Set special attribute preedit cursor position (ex. needed for CJK)
        if (!attribute.value.isValid() || !attribute.value.toBool()) {
            preEditCursorPos = -1;
        } else {
            preEditCursorPos = preedit.first(qBound(0, attribute.start, preedit.size())).toUtf8().size();
        }
        break;
    }
    m_input.setPreEditCursor(preEditCursorPos);

    // Send currently being edited string
    m_input.setPreEditString(preedit);
}

#include "moc_inputlisteneritem.cpp"
