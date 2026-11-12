// SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
// SPDX-License-Identifier: MIT
#include "processidentity.h"
#include <QtTest>
#include <unistd.h>

class ProcessIdentityTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void acceptsTheRealParentOfAProcfsProcess()
    {
        QVERIFY(UkeKeyboard::hasProcessParent(getpid(), getppid()));
    }
    void rejectsAnUnrelatedParentAndInvalidProcesses()
    {
        QVERIFY(!UkeKeyboard::hasProcessParent(getpid(), getpid()));
        QVERIFY(!UkeKeyboard::hasProcessParent(0, getppid()));
        QVERIFY(!UkeKeyboard::hasProcessParent(getpid(), 0));
        QVERIFY(!UkeKeyboard::hasProcessParent(-1, getppid()));
    }
};

QTEST_GUILESS_MAIN(ProcessIdentityTest)
#include "processidentitytest.moc"
