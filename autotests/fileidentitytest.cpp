// SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors
// SPDX-License-Identifier: MIT
#include "fileidentity.h"
#include <QDateTime>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

class FileIdentityTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void detectsAnAtomicReplacementWithEqualSizeAndTimestamp()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("installed-binary"));
        const auto timestamp = QDateTime::fromSecsSinceEpoch(1794441600);
        QFile old(path);
        QVERIFY(old.open(QIODevice::ReadWrite));
        QCOMPARE(old.write("AAAA"), qint64(4));
        QVERIFY(old.flush());
        QVERIFY(old.setFileTime(timestamp, QFileDevice::FileModificationTime));
        const QFileInfo before(path);
        const auto beforeSize = before.size();
        const auto beforeModified = before.lastModified();
        const auto identity = UkeKeyboard::fileIdentity(path);
        QVERIFY(identity.has_value());
        QVERIFY(identity == UkeKeyboard::fileIdentity(path));

        // Keep the original inode open, as a running process does after RPM
        // atomically replaces its executable with a reproducibly dated file.
        QSaveFile replacement(path);
        QVERIFY(replacement.open(QIODevice::WriteOnly));
        QCOMPARE(replacement.write("BBBB"), qint64(4));
        QVERIFY(replacement.commit());
        QFile current(path);
        QVERIFY(current.open(QIODevice::ReadWrite));
        QVERIFY(current.setFileTime(timestamp, QFileDevice::FileModificationTime));
        const QFileInfo after(path);
        QCOMPARE(beforeSize, after.size());
        QCOMPARE(beforeModified, after.lastModified());
        QVERIFY(identity != UkeKeyboard::fileIdentity(path));
        QVERIFY(old.seek(0));
        QCOMPARE(old.readAll(), QByteArray("AAAA"));
        QCOMPARE(current.readAll(), QByteArray("BBBB"));
    }

    void rejectsMissingAndNonregularPaths()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!UkeKeyboard::fileIdentity(dir.path()));
        QVERIFY(!UkeKeyboard::fileIdentity(dir.filePath(QStringLiteral("absent"))));
    }
};

QTEST_GUILESS_MAIN(FileIdentityTest)
#include "fileidentitytest.moc"
