/*
    SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "panelstubgeometry.h"
#include "keyboardroleprivacy.h"
#include <QTest>

class PanelStubGeometryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void dockedLocked_data()
    {
        QTest::addColumn<QSize>("surface");
        QTest::addColumn<QRect>("panel");
        QTest::newRow("builtin-125-percent-logical") << QSize(1536, 864) << QRect(100, 564, 1336, 300);
        QTest::newRow("external-4k-125-percent-logical") << QSize(3072, 1728) << QRect(300, 1228, 2472, 500);
        QTest::newRow("portrait") << QSize(864, 1536) << QRect(0, 1136, 864, 400);
        QTest::newRow("full-width") << QSize(1920, 1080) << QRect(0, 700, 1920, 380);
    }
    void dockedLocked()
    {
        QFETCH(QSize, surface);
        QFETCH(QRect, panel);
        const auto mask = panelStubInputRegion(panel, surface, true, true);
        QCOMPARE(mask.boundingRect(), panel);
        QVERIFY(mask.contains(panel.topLeft()));
        QVERIFY(mask.contains(panel.bottomRight()));
        QVERIFY(!mask.contains(panel.center()));
        QVERIFY(!mask.contains(QPoint(panel.center().x(), panel.top())));
        int area = 0;
        for (const auto &rectangle : mask) {
            area += rectangle.width() * rectangle.height();
        }
        QCOMPARE(area, 2); // Not a filled transparent region over the keys.
    }
    void fallbackAndTransitions()
    {
        const QSize surface(1920, 1080);
        const QRect panel(0, 700, 1920, 380);
        const QRegion fallback(0, 0, 1, 1);
        QCOMPARE(panelStubInputRegion(panel, surface, false, true), fallback);
        QCOMPARE(panelStubInputRegion(panel, surface, true, false), fallback);
        QCOMPARE(panelStubInputRegion(QRect(), surface, true, true), fallback);
        QCOMPARE(panelStubInputRegion(panel, QSize(), true, true), fallback);
        // A native buffer resize may lag behind the new panel's geometry.
        QCOMPARE(panelStubInputRegion(panel, QSize(1280, 800), true, true), fallback);
        QCOMPARE(panelStubInputRegion(panel, surface, true, true).boundingRect(), panel);
        QCOMPARE(panelStubInputRegion(panel, surface, false, true), fallback);
        // A global output offset is not a valid surface-local rectangle.
        QCOMPARE(panelStubInputRegion(QRect(-1536, 700, 1920, 380), surface, true, true), fallback);
        QVERIFY(!fallback.isEmpty()); // Empty mask means unrestricted input in Qt.
    }
    void nativeRolePrivacy()
    {
        using Action = KeyboardRoleAction;
        QCOMPARE(keyboardRoleAction(std::nullopt, false, false, false), Action::KeepUnprivileged);
        QCOMPARE(keyboardRoleAction(std::nullopt, true, false, false), Action::KeepUnprivileged);
        QCOMPARE(keyboardRoleAction(std::nullopt, true, true, false), Action::AuthorizeRestricted);
        QCOMPARE(keyboardRoleAction(false, true, true, false), Action::Recreate);
        QCOMPARE(keyboardRoleAction(true, true, true, true), Action::KeepAuthorized);
        QCOMPARE(keyboardRoleAction(true, true, false, true), Action::Recreate);
        QCOMPARE(keyboardRoleAction(true, false, false, true), Action::Recreate);
        QCOMPARE(keyboardRoleAction(false, true, false, false), Action::KeepUnprivileged);
        QCOMPARE(keyboardRoleAction(true, true, true, false, true), Action::KeepUnprivileged);
        QCOMPARE(keyboardRoleAction(std::nullopt, true, true, false, false), Action::AuthorizeRestricted);
    }

    void inclusiveCorners()
    {
        const QRect pixel(3, 4, 1, 1);
        const auto mask = panelStubInputRegion(pixel, QSize(10, 10), true, true);
        QCOMPARE(mask.boundingRect(), pixel);
        QCOMPARE(mask.rectCount(), 1);
        const QRect strip(0, 9, 10, 1);
        QCOMPARE(panelStubInputRegion(strip, QSize(10, 10), true, true).boundingRect(), strip);
    }
};
QTEST_GUILESS_MAIN(PanelStubGeometryTest)
#include "panelstubgeometrytest.moc"
