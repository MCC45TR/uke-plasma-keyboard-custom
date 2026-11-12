/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "restartwatcher.h"
#include "logging.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QFile>
#include <QGuiApplication>
#include <QInputMethod>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QDBusServiceWatcher>
#include <sys/syscall.h>
#include <unistd.h>
#include <memory>

namespace
{
//! How often the installed binary is compared with the one this process runs.

//! Time KWin gets to tear the old input method down before starting a new one.
constexpr int s_kwinRestartDelayMs = 1000;
//! Longest the restart is postponed while the keyboard is on screen.
constexpr int s_maxDeferMs = 2 * 60 * 1000;

QString kwinInputMethod()
{
    auto config=KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    config->reparseConfiguration();
    const KConfigGroup group(config, QStringLiteral("Wayland"));
    return group.readEntry(QStringLiteral("InputMethod"));
}

bool customInputMethodIsConfigured()
{
    const QString desktopFile = QFileInfo(kwinInputMethod()).fileName();
    return desktopFile.startsWith(QStringLiteral("org.uke.plasma.keyboard"));
}

void writeKwinInputMethod(const QString &value)
{
    QProcess::execute(QStringLiteral("kwriteconfig6"),
                      {QStringLiteral("--notify"),
                       QStringLiteral("--file"),
                       QStringLiteral("kwinrc"),
                       QStringLiteral("--group"),
                       QStringLiteral("Wayland"),
                       QStringLiteral("--key"),
                       QStringLiteral("InputMethod"),
                       value});
}

//! Ask KWin, whose state is the authoritative one (the window of this process
//! keeps reporting itself as visible).
bool keyboardVisible()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("/VirtualKeyboard"),
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    msg << QStringLiteral("org.kde.kwin.VirtualKeyboard") << QStringLiteral("visible");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        return reply.arguments().constFirst().value<QDBusVariant>().variant().toBool();
    }
    return false;
}
}

int restartInputMethod()
{
    const QString inputMethod = kwinInputMethod();
    if (!QFileInfo(inputMethod).fileName().startsWith(QStringLiteral("org.uke.plasma.keyboard"))) {
        return 0;
    }

    qCInfo(PlasmaKeyboard) << "Restarting the keyboard, the installed binary changed";
    // Emptying the setting kills the process that runs this helper, restoring
    // it makes KWin start a fresh one.
    writeKwinInputMethod(QString());
    QThread::msleep(s_kwinRestartDelayMs);
    // The blank value is the temporary state owned by this restart. Preserve
    // a different choice made in System Settings while the old process exits.
    if (kwinInputMethod().isEmpty()) {
        writeKwinInputMethod(inputMethod);
    }
    return 0;
}

RestartWatcher::RestartWatcher(QObject *parent)
    : QObject(parent)
{
    // /proc/self/exe follows symlinks, so this is the file a package update
    // replaces.
    m_binaryPath = QFile::symLinkTarget(QStringLiteral("/proc/self/exe"));
    if (m_binaryPath.isEmpty()) {
        m_binaryPath = QCoreApplication::applicationFilePath();
    }

    const QFileInfo info(m_binaryPath);
    m_fileIdentity = UkeKeyboard::fileIdentity(m_binaryPath);
    m_modified = info.lastModified();
    m_size = info.size();
    m_sinceDetection.start();

    if (!customInputMethodIsConfigured()) {
        // This process is not the configured input method, so KWin will not
        // keep it alive: nothing to restart.
        return;
    }

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(s_maxDeferMs);
    connect(m_timer, &QTimer::timeout, this, &RestartWatcher::checkForUpdate);
    auto *files=new QFileSystemWatcher(this);
    files->addPath(m_binaryPath);
    files->addPath(QFileInfo(m_binaryPath).absolutePath());
    connect(files,&QFileSystemWatcher::fileChanged,this,[this,files]{if(QFile::exists(m_binaryPath)&&!files->files().contains(m_binaryPath))files->addPath(m_binaryPath);checkForUpdate();});
    connect(files,&QFileSystemWatcher::directoryChanged,this,[this,files]{if(QFile::exists(m_binaryPath)&&!files->files().contains(m_binaryPath))files->addPath(m_binaryPath);checkForUpdate();});
    connect(QGuiApplication::inputMethod(),&QInputMethod::visibleChanged,this,&RestartWatcher::checkForUpdate);
}

void RestartWatcher::checkForUpdate()
{
    const QFileInfo info(m_binaryPath);
    const auto identity = UkeKeyboard::fileIdentity(m_binaryPath);
    if (!identity) {
        // The package manager is in the middle of replacing the file.
        return;
    }

    // RPM timestamps are reproducible and two different executables can have
    // equal sizes. Atomic replacement still changes the file's inode.
    const bool changed = identity != m_fileIdentity || info.lastModified() != m_modified || info.size() != m_size;
    if (!changed && !m_restartPending) {
        return;
    }

    if (changed) {
        m_fileIdentity = identity;
        m_modified = info.lastModified();
        m_size = info.size();
        m_restartPending = true;
        m_sinceDetection.start();
        m_timer->start(s_maxDeferMs);
    }

    // Do not pull the keyboard away from under the user's fingers, but do not
    // wait forever either: an update has to reach the running keyboard.
    if (keyboardVisible() && m_sinceDetection.elapsed() < s_maxDeferMs) {
        return;
    }

    m_restartPending = false;
    restart();
}

void RestartWatcher::restart()
{
    qCInfo(PlasmaKeyboard) << "The installed binary changed, restarting the keyboard";
    // The helper survives the toggle below; this process is killed by KWin as
    // soon as the input method is torn down, so quit cleanly (which also
    // releases the single instance lock) and let the helper finish the job.
    if (!QProcess::startDetached(m_binaryPath, {QStringLiteral("--restart-input-method")})) {
        qCWarning(PlasmaKeyboard) << "Could not start the restart helper";
        return;
    }
    QCoreApplication::quit();
}

int runInputMethodWatchdog()
{
    int count=1;char name[]="uke-keyboard-watchdog";char *arguments[]={name,nullptr};
    QCoreApplication application(count,arguments);
    if(!customInputMethodIsConfigured())return 0;
    const QString runtimeDir=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString instancePath=runtimeDir+QStringLiteral("/uke-plasma-keyboard-custom.lock");
    QLockFile watchdog(runtimeDir+QStringLiteral("/uke-plasma-keyboard-custom-watchdog.lock"));
    watchdog.setStaleLockTime(0);if(!watchdog.tryLock(0))return 0;
    QFileSystemWatcher files;
    const QString configPath=QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+QStringLiteral("/kwinrc");
    files.addPath(configPath);files.addPath(QFileInfo(configPath).absolutePath());
    QSocketNotifier *notifier=nullptr;int processFd=-1;qint64 watchedPid=-1;
    bool restarting=false;
    std::function<void()> observe;
    observe=[&] {
        if(!customInputMethodIsConfigured()){application.quit();return;}
        QLockFile instance(instancePath);qint64 pid=0;QString host,app;
        if(!instance.getLockInfo(&pid,&host,&app)||pid<=0){
            if(!files.directories().contains(runtimeDir))files.addPath(runtimeDir);
            return;
        }
        if(pid==watchedPid)return;
        if(notifier){delete notifier;notifier=nullptr;}
        if(processFd>=0)close(processFd);
        processFd=int(syscall(SYS_pidfd_open,pid,0));
        if(processFd<0)return;
        watchedPid=pid;
        files.removePath(runtimeDir);
        notifier=new QSocketNotifier(processFd,QSocketNotifier::Read,&application);
        QObject::connect(notifier,&QSocketNotifier::activated,&application,[&]{
            notifier->setEnabled(false);watchedPid=-1;
            if(restarting)return;restarting=true;
            QTimer::singleShot(1000,&application,[&]{
                if(customInputMethodIsConfigured())restartInputMethod();
                restarting=false;observe();
            });
        });
    };
    QObject::connect(&files,&QFileSystemWatcher::fileChanged,&application,[&]{if(QFile::exists(configPath)&&!files.files().contains(configPath))files.addPath(configPath);observe();});
    QObject::connect(&files,&QFileSystemWatcher::directoryChanged,&application,[&]{observe();});
    QDBusServiceWatcher compositor(QStringLiteral("org.kde.KWin"),QDBusConnection::sessionBus(),QDBusServiceWatcher::WatchForUnregistration);
    QObject::connect(&compositor,&QDBusServiceWatcher::serviceUnregistered,&application,&QCoreApplication::quit);
    observe();const int result=application.exec();if(processFd>=0)close(processFd);return result;
}
