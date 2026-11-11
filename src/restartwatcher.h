/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QTimer;

/**
 * Restarts the keyboard when the binary this process runs from is updated.
 *
 * KWin keeps one uke-plasma-keyboard-custom process running for the whole session,
 * so after a package update that process keeps executing the old code until the
 * session is restarted by hand. This class notices that the file on disk has
 * changed and asks KWin to start the input method again, with the same
 * configuration toggle a manual restart uses:
 *
 *     kwinrc [Wayland] InputMethod -> "" -> the previous value
 *
 * Emptying the setting makes KWin tear the running process down, restoring it
 * makes KWin start a fresh one, which picks up the new binary. The restart is
 * deferred while the panel is visible, so an update does not interrupt typing,
 * but only for a couple of minutes - it must not stay unapplied just because
 * the keyboard happens to be on screen. When this process is not the configured
 * input method the watcher stays idle, as KWin keeps no process of ours around
 * then anyway.
 */
class RestartWatcher : public QObject
{
    Q_OBJECT

public:
    explicit RestartWatcher(QObject *parent = nullptr);

private:
    void checkForUpdate();
    void restart();

    QString m_binaryPath;
    QDateTime m_modified;
    qint64 m_size = -1;
    bool m_restartPending = false;
    QElapsedTimer m_sinceDetection;
    QTimer *m_timer = nullptr;
};

/**
 * Entry point of the detached helper (--restart-input-method): toggles the KWin
 * setting and returns.
 *
 * It deliberately runs without a QGuiApplication: it must not become a Wayland
 * client, and it has to survive the old input method process being killed by
 * the very toggle it performs.
 */
int restartInputMethod();

/**
 * Entry point of the detached watchdog (--watchdog): keeps the input method
 * process alive for the whole session.
 *
 * KWin starts the configured input method once but does not start a
 * replacement when it exits. The global shortcut to show the keyboard is
 * registered by that process, so once it is gone the shortcut stops working.
 * The watchdog blocks on a pidfd and file notifications, then asks KWin
 * to start the input method again. It stays idle while the virtual keyboard is
 * disabled, and only one watchdog runs at a time.
 */
int runInputMethodWatchdog();
