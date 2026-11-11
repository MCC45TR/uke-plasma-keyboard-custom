// SPDX-License-Identifier: MIT
#include "versionguard.h"
#include <QtTest>
class KWinVersionTest:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void originalVersionOnly(){
        QVERIFY(UkeKeyboard::compatibleKWin(QStringLiteral("6.8.2"),QStringLiteral("6.8.2")));
        for(const auto &version:{QString(),QStringLiteral("6.8.3"),QStringLiteral("6.9.0"),QStringLiteral("6.8.2.1"),QStringLiteral("6.8.2-git")})
            QVERIFY(!UkeKeyboard::compatibleKWin(version,QStringLiteral("6.8.2")));
        QVERIFY(!UkeKeyboard::compatibleKWin({},{}));
    }
};
QTEST_GUILESS_MAIN(KWinVersionTest)
#include "kwinversiontest.moc"
