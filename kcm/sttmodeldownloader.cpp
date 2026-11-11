/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sttmodeldownloader.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <QtCore/private/qzipreader_p.h>

Q_LOGGING_CATEGORY(lcSttDownload, "org.uke.plasma.keyboard.stt.download")

namespace PlasmaKeyboardStt
{

SttModelDownloader::SttModelDownloader(QObject *parent)
    : QObject(parent)
{
}

bool SttModelDownloader::isBusy() const
{
    return m_busy;
}

QString SttModelDownloader::modelId() const
{
    return m_entry.id;
}

qreal SttModelDownloader::progress() const
{
    return m_progress;
}

QString SttModelDownloader::error() const
{
    return m_error;
}

void SttModelDownloader::start(const SttModelEntry &entry)
{
    if (m_busy) {
        return;
    }

    const QString dir = SttModelCatalog::installDir(entry);
    if (!QDir().mkpath(dir)) {
        fail(tr("Cannot create “%1”.").arg(dir));
        Q_EMIT finished(entry.id, false, m_error);
        return;
    }

    m_entry = entry;
    m_error.clear();
    m_progress = 0;
    m_busy = true;
    m_partPath = dir + QLatin1Char('/') + entry.fileName + QStringLiteral(".part");

    m_file.setFileName(m_partPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(tr("Cannot write to “%1”.").arg(m_partPath));
        Q_EMIT finished(entry.id, false, m_error);
        return;
    }

    QNetworkRequest request{QUrl(entry.url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("uke-plasma-keyboard-custom"));
    m_reply = m_manager.get(request);

    connect(m_reply, &QNetworkReply::readyRead, this, &SttModelDownloader::handleReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &SttModelDownloader::handleProgress);
    connect(m_reply, &QNetworkReply::finished, this, &SttModelDownloader::handleFinished);

    qCDebug(lcSttDownload) << "downloading the model" << entry.id << "from" << entry.url;
    Q_EMIT stateChanged();
    Q_EMIT errorChanged();
    Q_EMIT progressChanged();
}

void SttModelDownloader::cancel()
{
    if (!m_busy) {
        return;
    }
    if (m_reply) {
        m_reply->abort();
    }
}

void SttModelDownloader::handleReadyRead()
{
    if (m_reply) {
        m_file.write(m_reply->readAll());
    }
}

void SttModelDownloader::handleProgress(qint64 received, qint64 total)
{
    const qint64 expected = total > 0 ? total : m_entry.size;
    if (expected > 0) {
        m_progress = qBound(0.0, qreal(received) / qreal(expected), 1.0);
        Q_EMIT progressChanged();
    }
}

void SttModelDownloader::handleFinished()
{
    if (!m_busy) {
        return;
    }

    const QString modelId = m_entry.id;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    const bool cancelled = reply && reply->error() == QNetworkReply::OperationCanceledError;
    const QString networkError = reply && reply->error() != QNetworkReply::NoError ? reply->errorString() : QString();
    if (reply) {
        m_file.write(reply->readAll());
        reply->deleteLater();
    }
    m_file.close();

    if (cancelled) {
        QFile::remove(m_partPath);
        m_busy = false;
        m_progress = 0;
        m_entry = SttModelEntry();
        Q_EMIT stateChanged();
        Q_EMIT progressChanged();
        Q_EMIT finished(modelId, false, tr("The download was cancelled."));
        return;
    }

    if (!networkError.isEmpty()) {
        fail(networkError);
        Q_EMIT finished(modelId, false, m_error);
        return;
    }

    QString error;
    if (!verifyFile(m_partPath, m_entry, &error)) {
        fail(error);
        Q_EMIT finished(modelId, false, m_error);
        return;
    }

    const QString dir = SttModelCatalog::installDir(m_entry);
    if (m_entry.archive) {
        // The archive is only a carrier: unpack it and drop it again, so the
        // model directory is what Vosk is pointed at.
        if (!unpackArchive(m_partPath, dir, &error)) {
            fail(error);
            Q_EMIT finished(modelId, false, m_error);
            return;
        }
        QFile::remove(m_partPath);
    } else {
        const QString target = dir + QLatin1Char('/') + m_entry.fileName;
        QFile::remove(target);
        if (!QFile::rename(m_partPath, target)) {
            fail(tr("Cannot move the downloaded model to “%1”.").arg(target));
            Q_EMIT finished(modelId, false, m_error);
            return;
        }
    }

    qCDebug(lcSttDownload) << "model" << modelId << "installed";
    m_busy = false;
    m_progress = 0;
    const SttModelEntry entry = m_entry;
    m_entry = SttModelEntry();
    SttModelCatalog::instance()->refresh();
    Q_EMIT stateChanged();
    Q_EMIT progressChanged();
    Q_EMIT finished(entry.id, true, QString());
}

void SttModelDownloader::fail(const QString &message)
{
    m_error = message;
    if (!m_partPath.isEmpty()) {
        QFile::remove(m_partPath);
    }
    m_busy = false;
    m_progress = 0;
    qCWarning(lcSttDownload) << message;
    Q_EMIT stateChanged();
    Q_EMIT errorChanged();
    Q_EMIT progressChanged();
}

bool SttModelDownloader::verifyFile(const QString &path, const SttModelEntry &entry, QString *error)
{
    const QFileInfo info(path);
    if (!info.exists() || info.size() == 0) {
        if (error) {
            *error = tr("The download of “%1” is empty.").arg(entry.name);
        }
        return false;
    }
    if (entry.size > 0 && info.size() != entry.size) {
        if (error) {
            *error = tr("The download of “%1” has the wrong size (%2 instead of %3 bytes).").arg(entry.name).arg(info.size()).arg(entry.size);
        }
        return false;
    }

    if (!entry.sha256.isEmpty()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = tr("Cannot read the downloaded model “%1”.").arg(entry.name);
            }
            return false;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file)) {
            if (error) {
                *error = tr("Cannot read the downloaded model “%1”.").arg(entry.name);
            }
            return false;
        }
        const QString digest = QString::fromLatin1(hash.result().toHex());
        if (digest.compare(entry.sha256, Qt::CaseInsensitive) != 0) {
            if (error) {
                *error = tr("The download of “%1” is damaged (checksum mismatch).").arg(entry.name);
            }
            return false;
        }
    }

    return true;
}

bool SttModelDownloader::unpackArchive(const QString &archive, const QString &destination, QString *error)
{
    QZipReader reader(archive);
    if (!reader.isReadable()) {
        if (error) {
            *error = tr("The downloaded archive “%1” cannot be read.").arg(archive);
        }
        return false;
    }
    if (!QDir().mkpath(destination) || !reader.extractAll(destination)) {
        if (error) {
            *error = tr("Cannot unpack “%1” into “%2”.").arg(archive, destination);
        }
        return false;
    }

    // The Vosk archives carry the model in a directory of their own; the model
    // has to sit directly in the install directory, so it is lifted out of it.
    QDir destinationDir(destination);
    const QStringList dirs = destinationDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const QStringList files = destinationDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (files.isEmpty() && dirs.size() == 1) {
        QDir inner(destination + QLatin1Char('/') + dirs.first());
        const QStringList children = inner.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &child : children) {
            const QString from = inner.absoluteFilePath(child);
            const QString to = destinationDir.absoluteFilePath(child);
            if (!QFile::rename(from, to) && !QDir().rename(from, to)) {
                if (error) {
                    *error = tr("Cannot unpack “%1” into “%2”.").arg(archive, destination);
                }
                return false;
            }
        }
        inner.removeRecursively();
    }

    return true;
}

} // namespace PlasmaKeyboardStt
