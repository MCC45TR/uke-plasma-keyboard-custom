/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "voskengine.h"

#include <KLocalizedString>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSttVosk, "org.uke.plasma.keyboard.stt.vosk")

namespace PlasmaKeyboardStt
{

namespace
{
//! The names libvosk is installed under.
const char *const libraryNames[] = {
    "libvosk.so.0",
    "libvosk.so",
    "vosk",
};

//! The sample rate the recognizer is created for; the recorder delivers this.
constexpr float SampleRate = 16000.0f;
} // namespace

/**
 * The part of the C API of libvosk the engine uses. The functions are looked up
 * when the library is loaded, so vosk_api.h is not needed to build against.
 */
struct VoskEngine::Api {
    using ModelNew = void *(*)(const char *modelPath);
    using ModelFree = void (*)(void *model);
    using RecognizerNew = void *(*)(void *model, float sampleRate);
    using RecognizerFree = void (*)(void *recognizer);
    using AcceptWaveform = int (*)(void *recognizer, const float *data, int length);
    using FinalResult = const char *(*)(void *recognizer);
    using SetLogLevel = void (*)(int level);

    ModelNew modelNew = nullptr;
    ModelFree modelFree = nullptr;
    RecognizerNew recognizerNew = nullptr;
    RecognizerFree recognizerFree = nullptr;
    AcceptWaveform acceptWaveform = nullptr;
    FinalResult finalResult = nullptr;
    SetLogLevel setLogLevel = nullptr;

    bool isValid() const
    {
        return modelNew && modelFree && recognizerNew && recognizerFree && acceptWaveform && finalResult;
    }
};

namespace
{
//! The loaded library, kept alive as long as the functions found in it are used.
QLibrary &voskLibrary()
{
    static QLibrary library;
    return library;
}
} // namespace

const VoskEngine::Api *VoskEngine::api()
{
    static const Api *loaded = []() -> const Api * {
        static Api api;
        QLibrary &library = voskLibrary();
        for (const char *name : libraryNames) {
            library.setFileName(QString::fromLatin1(name));
            if (!library.load()) {
                continue;
            }
            api.modelNew = reinterpret_cast<Api::ModelNew>(library.resolve("vosk_model_new"));
            api.modelFree = reinterpret_cast<Api::ModelFree>(library.resolve("vosk_model_free"));
            api.recognizerNew = reinterpret_cast<Api::RecognizerNew>(library.resolve("vosk_recognizer_new"));
            api.recognizerFree = reinterpret_cast<Api::RecognizerFree>(library.resolve("vosk_recognizer_free"));
            api.acceptWaveform = reinterpret_cast<Api::AcceptWaveform>(library.resolve("vosk_recognizer_accept_waveform_f"));
            api.finalResult = reinterpret_cast<Api::FinalResult>(library.resolve("vosk_recognizer_final_result"));
            api.setLogLevel = reinterpret_cast<Api::SetLogLevel>(library.resolve("vosk_set_log_level"));
            if (!api.isValid()) {
                qCWarning(lcSttVosk) << "libvosk was found but does not provide the expected functions";
                library.unload();
                return nullptr;
            }
            if (api.setLogLevel) {
                // -1 keeps the library quiet; the keyboard logs its own way.
                api.setLogLevel(-1);
            }
            qCDebug(lcSttVosk) << "libvosk loaded from" << library.fileName();
            return &api;
        }
        qCDebug(lcSttVosk) << "libvosk is not installed";
        return nullptr;
    }();
    return loaded;
}

VoskEngine::VoskEngine(QObject *parent)
    : SttEngine(parent)
{
}

VoskEngine::~VoskEngine()
{
    unloadModel();
}

QString VoskEngine::id() const
{
    return QStringLiteral("vosk");
}

QString VoskEngine::displayName() const
{
    return QStringLiteral("Vosk");
}

bool VoskEngine::isAvailable(QString *reason) const
{
    if (api()) {
        return true;
    }
    if (reason) {
        *reason = i18nd("uke-plasma-keyboard-custom", "The Vosk library (vosk-api) is not installed.");
    }
    return false;
}

bool VoskEngine::loadModel(const QString &modelPath, QString *error)
{
    unloadModel();

    const Api *vosk = api();
    if (!vosk) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Vosk library (vosk-api) is not installed.");
        }
        return false;
    }

    const QFileInfo info(modelPath);
    if (!info.exists() || !info.isDir()) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Vosk model directory does not exist: %1").arg(modelPath);
        }
        return false;
    }

    m_model = vosk->modelNew(QFile::encodeName(info.absoluteFilePath()).constData());
    if (!m_model) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "Cannot load the Vosk model: %1").arg(info.absoluteFilePath());
        }
        qCWarning(lcSttVosk) << "cannot load the vosk model" << info.absoluteFilePath();
        return false;
    }

    m_recognizer = vosk->recognizerNew(m_model, SampleRate);
    if (!m_recognizer) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "Cannot create the Vosk recognizer for “%1”.").arg(info.absoluteFilePath());
        }
        vosk->modelFree(m_model);
        m_model = nullptr;
        return false;
    }

    m_modelPath = info.absoluteFilePath();
    qCDebug(lcSttVosk) << "vosk model loaded" << m_modelPath;
    return true;
}

void VoskEngine::unloadModel()
{
    const Api *vosk = api();
    if (!vosk) {
        m_model = nullptr;
        m_recognizer = nullptr;
        m_modelPath.clear();
        return;
    }
    if (m_recognizer) {
        vosk->recognizerFree(m_recognizer);
        m_recognizer = nullptr;
    }
    if (m_model) {
        vosk->modelFree(m_model);
        m_model = nullptr;
        qCDebug(lcSttVosk) << "vosk model unloaded";
    }
    m_modelPath.clear();
}

bool VoskEngine::hasModel() const
{
    return m_model != nullptr && m_recognizer != nullptr;
}

QString VoskEngine::transcribe(const QVector<float> &samples, const QString &language, QString *error)
{
    // The model of Vosk is built for one language, so the hint is not used.
    Q_UNUSED(language)

    const Api *vosk = api();
    if (!vosk || !m_recognizer) {
        if (error) {
            *error = i18nd("uke-plasma-keyboard-custom", "The Vosk model is not loaded.");
        }
        return QString();
    }
    if (samples.isEmpty()) {
        return QString();
    }

    // Vosk (Kaldi) takes the samples in the range of 16 bit PCM, not in -1..1
    // like the other engines, so the recorded audio is scaled up for it.
    QVector<float> scaled(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        scaled[i] = samples.at(i) * 32768.0f;
    }

    // The audio is fed in blocks: Vosk keeps an internal buffer and reports the
    // text of the finished phrases through the final result.
    constexpr int BlockSize = 8000;
    int accepted = 0;
    for (int offset = 0; offset < scaled.size(); offset += BlockSize) {
        const int length = qMin(BlockSize, scaled.size() - offset);
        accepted = vosk->acceptWaveform(m_recognizer, scaled.constData() + offset, length);
    }
    const char *result = vosk->finalResult(m_recognizer);
    if (!result) {
        return QString();
    }
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(result));
    qCDebug(lcSttVosk) << "vosk accepted" << accepted << "samples" << samples.size();
    return document.object().value(QStringLiteral("text")).toString().trimmed();
}

} // namespace PlasmaKeyboardStt
