/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "whisperengine.h"

#include <KLocalizedString>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QThread>

#include <whisper.h>

Q_LOGGING_CATEGORY(lcSttWhisper, "org.uke.plasma.keyboard.stt.whisper")

namespace PlasmaKeyboardStt
{

namespace
{
//! whisper.cpp prints its progress to stderr by default; the keyboard logs its
//! own way, so the library output is dropped.
void silentLog(enum ggml_log_level level, const char *text, void *userData)
{
    Q_UNUSED(level)
    Q_UNUSED(text)
    Q_UNUSED(userData)
}
} // namespace

WhisperEngine::WhisperEngine(QObject *parent)
    : SttEngine(parent)
{
}

WhisperEngine::~WhisperEngine()
{
    unloadModel();
}

QString WhisperEngine::id() const
{
    return QStringLiteral("whisper");
}

QString WhisperEngine::displayName() const
{
    return QStringLiteral("Whisper");
}

bool WhisperEngine::isAvailable(QString *reason) const
{
    Q_UNUSED(reason)
    // The engine is compiled into the keyboard together with libwhisper.
    return true;
}

bool WhisperEngine::loadModel(const QString &modelPath, QString *error)
{
    unloadModel();

    const QFileInfo info(modelPath);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Whisper model file does not exist: %1").arg(modelPath);
        }
        return false;
    }

    whisper_log_set(silentLog, nullptr);

    struct whisper_context_params params = whisper_context_default_params();
    m_context = whisper_init_from_file_with_params(QFile::encodeName(info.absoluteFilePath()).constData(), params);
    if (!m_context) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "Cannot load the Whisper model: %1").arg(info.absoluteFilePath());
        }
        qCWarning(lcSttWhisper) << "cannot load the whisper model" << info.absoluteFilePath();
        return false;
    }

    m_modelPath = info.absoluteFilePath();
    qCDebug(lcSttWhisper) << "whisper model loaded" << m_modelPath;
    return true;
}

void WhisperEngine::unloadModel()
{
    if (m_context) {
        whisper_free(m_context);
        m_context = nullptr;
        qCDebug(lcSttWhisper) << "whisper model unloaded";
    }
    m_modelPath.clear();
}

bool WhisperEngine::hasModel() const
{
    return m_context != nullptr;
}

QString WhisperEngine::transcribe(const QVector<float> &samples, const QString &language, QString *error)
{
    if (!m_context) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Whisper model is not loaded.");
        }
        return QString();
    }
    if (samples.isEmpty()) {
        return QString();
    }

    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = qMax(1, QThread::idealThreadCount());
    // Without a language hint Whisper detects it; the keyboard passes the
    // language of its layout, or the fixed one from the settings.
    const QByteArray languageHint = language.isEmpty() ? QByteArray("auto") : language.left(2).toUtf8();
    params.language = languageHint.constData();
    params.translate = false;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;

    const int result = whisper_full(m_context, params, samples.constData(), samples.size());
    if (result != 0) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Whisper recognition failed (error %1).").arg(result);
        }
        qCWarning(lcSttWhisper) << "whisper_full failed" << result;
        return QString();
    }

    QString text;
    const int segments = whisper_full_n_segments(m_context);
    for (int i = 0; i < segments; ++i) {
        const char *segment = whisper_full_get_segment_text(m_context, i);
        if (segment) {
            text += QString::fromUtf8(segment);
        }
    }

    return text.trimmed();
}

} // namespace PlasmaKeyboardStt
