/*
    SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "keyboardvisibilitystate.h"
#include <QTest>

class KeyboardVisibilityStateTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void lockedToggleIgnoresDesktopStickiness()
    {
        KeyboardVisibilityState state;
        QVERIFY(!state.update(false, true, true, false, false));
        QCOMPARE(state.update(true, true, true, false, false), std::optional<bool>(true));
        QCOMPARE(state.update(false, true, true, false, false), std::optional<bool>(false));
        QCOMPARE(state.update(true, true, true, false, false), std::optional<bool>(true));
        QVERIFY(!state.update(true, true, true, false, false));
    }
    void preparingNeverShowsAndReadyIsNotAnIndependentShowIntent()
    {
        KeyboardVisibilityState state;
        QCOMPARE(state.update(true, true, false, false, false), std::optional<bool>(true));
        QCOMPARE(state.update(true, true, true, true, false), std::optional<bool>(false));
        QVERIFY(!state.update(false, true, true, true, false));
        QVERIFY(!state.update(false, true, true, false, false));
        QCOMPARE(state.update(true, true, true, false, false), std::optional<bool>(true));
    }
    void unlockRejectsStaleTrueUntilHidden()
    {
        KeyboardVisibilityState state;
        state.update(true, true, true, false, false);
        state.afterUnlock();
        QCOMPARE(state.update(true, true, false, false, false), std::optional<bool>(false));
        QVERIFY(!state.update(true, true, false, false, false));
        QVERIFY(!state.update(false, true, false, false, false));
        QCOMPARE(state.update(true, true, false, false, false), std::optional<bool>(true));
    }
    void desktopPreferenceAndLocalHideEchoArePreserved()
    {
        KeyboardVisibilityState state;
        QCOMPARE(state.update(true, true, false, false, false), std::optional<bool>(true));
        QVERIFY(!state.update(true, true, false, false, false));
        QVERIFY(!state.update(false, true, false, false, false));
        QCOMPARE(state.update(true, true, false, false, false), std::optional<bool>(true));
        QCOMPARE(state.update(false, true, false, false, true), std::optional<bool>(false));
    }
    void ownerLossIsRestrictive()
    {
        KeyboardVisibilityState state;
        QVERIFY(!state.update(true, false, false, false, false));
        QCOMPARE(state.update(true, true, true, false, false), std::optional<bool>(true));
        QCOMPARE(state.update(true, false, false, false, false), std::optional<bool>(false));
    }
};
QTEST_GUILESS_MAIN(KeyboardVisibilityStateTest)
#include "keyboardvisibilitystatetest.moc"
