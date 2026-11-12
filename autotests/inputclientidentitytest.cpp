// SPDX-License-Identifier: GPL-2.0-or-later
#include "inputclientidentity.h"
#include <wayland/clientconnection.h>
#include <wayland/display.h>
#include <QtTest>
#include <QScopeGuard>
#include <sys/socket.h>
#include <unistd.h>

class InputClientIdentityTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void inheritedConnectionsHaveTheSamePidButDifferentOwnership() {
        KWin::Display display;QVERIFY(display.start());
        int firstPair[2],secondPair[2];
        QVERIFY(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,firstPair)==0);
        const auto firstPeer=qScopeGuard([&]{close(firstPair[1]);});
        auto *first=display.createClient(firstPair[0]);QVERIFY(first);
        QVERIFY(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,secondPair)==0);
        const auto secondPeer=qScopeGuard([&]{close(secondPair[1]);});
        auto *second=display.createClient(secondPair[0]);QVERIFY(second);
        QCOMPARE(first->processId(),getpid());QCOMPARE(second->processId(),getpid());
        QVERIFY(UkeKeyboard::isInputMethodClient(first,first));
        QVERIFY(!UkeKeyboard::isInputMethodClient(second,first));
        QVERIFY(!UkeKeyboard::isInputMethodClient(first,second));
        QVERIFY(!UkeKeyboard::isInputMethodClient(nullptr,first));
        QVERIFY(!UkeKeyboard::isInputMethodClient(first,nullptr));
    }
};
QTEST_GUILESS_MAIN(InputClientIdentityTest)
#include "inputclientidentitytest.moc"
