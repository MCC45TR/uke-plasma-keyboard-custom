/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "parakeetengine.h"

#include <KLocalizedString>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QThread>

#include <parakeet.h>

Q_LOGGING_CATEGORY(lcSttParakeet, "org.uke.plasma.keyboard.stt.parakeet")

namespace PlasmaKeyboardStt
{

namespace
{
//! The model expects 16 kHz mono samples; the recorder already delivers them.
constexpr int SampleRate = 16000;

//! parakeet.cpp prints its progress to stderr by default; the keyboard logs its
//! own way, so the library output is dropped.
void silentLog(enum ggml_log_level level, const char *text, void *userData)
{
    Q_UNUSED(level)
    Q_UNUSED(text)
    Q_UNUSED(userData)
}
} // namespace

ParakeetEngine::ParakeetEngine(QObject *parent)
    : SttEngine(parent)
{
}

ParakeetEngine::~ParakeetEngine()
{
    unloadModel();
}

QString ParakeetEngine::id() const
{
    return QStringLiteral("parakeet");
}

QString ParakeetEngine::displayName() const
{
    return QStringLiteral("Parakeet v3");
}

bool ParakeetEngine::isAvailable(QString *reason) const
{
    Q_UNUSED(reason)
    // The engine is compiled into the keyboard together with libparakeet.
    return true;
}

bool ParakeetEngine::loadModel(const QString &modelPath, QString *error)
{
    unloadModel();

    const QFileInfo info(modelPath);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Parakeet model file does not exist: %1").arg(modelPath);
        }
        return false;
    }

    parakeet_log_set(silentLog, nullptr);

    struct parakeet_context_params params = parakeet_context_default_params();
    m_context = parakeet_init_from_file_with_params(QFile::encodeName(info.absoluteFilePath()).constData(), params);
    if (!m_context) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "Cannot load the Parakeet model: %1").arg(info.absoluteFilePath());
        }
        qCWarning(lcSttParakeet) << "cannot load the parakeet model" << info.absoluteFilePath();
        return false;
    }

    m_modelPath = info.absoluteFilePath();
    qCDebug(lcSttParakeet) << "parakeet model loaded" << m_modelPath << "sample rate" << SampleRate;
    return true;
}

void ParakeetEngine::unloadModel()
{
    if (m_context) {
        parakeet_free(m_context);
        m_context = nullptr;
        qCDebug(lcSttParakeet) << "parakeet model unloaded";
    }
    m_modelPath.clear();
}

bool ParakeetEngine::hasModel() const
{
    return m_context != nullptr;
}

QString ParakeetEngine::transcribe(const QVector<float> &samples, const QString &language, QString *error)
{
    // The v3 model is multilingual and does not take a language hint.
    Q_UNUSED(language)

    if (!m_context) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Parakeet model is not loaded.");
        }
        return QString();
    }
    if (samples.isEmpty()) {
        return QString();
    }

    struct parakeet_full_params params = parakeet_full_default_params(PARAKEET_SAMPLING_GREEDY);
    params.n_threads = qMax(1, QThread::idealThreadCount());

    const int result = parakeet_full(m_context, params, samples.constData(), samples.size());
    if (result != 0) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Parakeet recognition failed (error %1).").arg(result);
        }
        qCWarning(lcSttParakeet) << "parakeet_full failed" << result;
        return QString();
    }

    QString text;
    const int segments = parakeet_full_n_segments(m_context);
    for (int i = 0; i < segments; ++i) {
        const char *segment = parakeet_full_get_segment_text(m_context, i);
        if (segment) {
            text += QString::fromUtf8(segment);
        }
    }

    return text.trimmed();
}

} // namespace PlasmaKeyboardStt
