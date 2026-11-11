/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "wordpredictor.h"

#include "wordlookup.h"

#include <QFile>
#include <QLoggingCategory>
#include <QtEndian>

Q_LOGGING_CATEGORY(lcWordPrediction, "org.uke.plasma.keyboard.prediction.ngrams")

namespace
{
//! The header of the PKD2 resource: magic, keyCount, entryCount, wordCount.
constexpr qsizetype headerSize = 16;
} // namespace

bool WordPredictor::NgramList::isValid() const
{
    if (keyCount == 0 || !data.startsWith("PKD2")) {
        return false;
    }
    // Every block has to fit before the next one is looked at, so that the
    // offsets of a truncated resource are never read.
    const qsizetype keyOffsetsEnd = headerSize + qsizetype(keyCount + 1) * 4;
    if (data.size() < keyOffsetsEnd) {
        return false;
    }
    const qsizetype wordOffsetsEnd = wordOffsetsOffset() + qsizetype(wordCount + 1) * 4;
    if (data.size() < wordOffsetsEnd) {
        return false;
    }
    return data.size() >= totalSize();
}

qsizetype WordPredictor::NgramList::keyPoolOffset() const
{
    return headerSize + qsizetype(keyCount + 1) * 4;
}

qsizetype WordPredictor::NgramList::entryOffsetsOffset() const
{
    return keyPoolOffset() + keyOffset(keyCount);
}

qsizetype WordPredictor::NgramList::entryWordsOffset() const
{
    return entryOffsetsOffset() + qsizetype(keyCount + 1) * 4;
}

qsizetype WordPredictor::NgramList::wordOffsetsOffset() const
{
    return entryWordsOffset() + qsizetype(entryCount) * 4;
}

qsizetype WordPredictor::NgramList::wordPoolOffset() const
{
    return wordOffsetsOffset() + qsizetype(wordCount + 1) * 4;
}

qsizetype WordPredictor::NgramList::totalSize() const
{
    return wordPoolOffset() + wordOffset(wordCount);
}

quint32 WordPredictor::NgramList::keyOffset(quint32 index) const
{
    return qFromLittleEndian<quint32>(data.constData() + headerSize + qsizetype(index) * 4);
}

QByteArrayView WordPredictor::NgramList::key(quint32 index) const
{
    const qsizetype pool = keyPoolOffset();
    const quint32 start = keyOffset(index);
    return QByteArrayView(data.constData() + pool + start, keyOffset(index + 1) - start);
}

quint32 WordPredictor::NgramList::entryOffset(quint32 index) const
{
    return qFromLittleEndian<quint32>(data.constData() + entryOffsetsOffset() + qsizetype(index) * 4);
}

quint32 WordPredictor::NgramList::entryWord(quint32 index) const
{
    return qFromLittleEndian<quint32>(data.constData() + entryWordsOffset() + qsizetype(index) * 4);
}

quint32 WordPredictor::NgramList::wordOffset(quint32 index) const
{
    return qFromLittleEndian<quint32>(data.constData() + wordOffsetsOffset() + qsizetype(index) * 4);
}

QByteArrayView WordPredictor::NgramList::word(quint32 index) const
{
    const qsizetype pool = wordPoolOffset();
    const quint32 start = wordOffset(index);
    return QByteArrayView(data.constData() + pool + start, wordOffset(index + 1) - start);
}

quint32 WordPredictor::NgramList::findKey(const QByteArray &needle) const
{
    // The keys are ordered by the lookup key, so the key of the word that was
    // typed is one binary search away.
    quint32 low = 0;
    quint32 high = keyCount;
    while (low < high) {
        const quint32 middle = low + (high - low) / 2;
        if (lookupKey(key(middle)) < needle) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

WordPredictor::WordPredictor(QObject *parent)
    : QObject(parent)
{
}

WordPredictor::~WordPredictor() = default;

std::shared_ptr<const WordPredictor::NgramList> WordPredictor::listFor(const QString &locale) const
{
    const QString language = languageOf(locale);
    if (language.isEmpty()) {
        return {};
    }

    const auto cached = m_lists.constFind(language);
    if (cached != m_lists.constEnd()) {
        return *cached;
    }

    m_lists.clear();
    auto list = std::make_shared<NgramList>();

    QFile file(QStringLiteral(":/prediction/%1.ngrams").arg(language));
    if (file.open(QIODevice::ReadOnly)) {
        list->data = file.readAll();
        if (list->data.size() >= headerSize) {
            list->keyCount = qFromLittleEndian<quint32>(list->data.constData() + 4);
            list->entryCount = qFromLittleEndian<quint32>(list->data.constData() + 8);
            list->wordCount = qFromLittleEndian<quint32>(list->data.constData() + 12);
        }
        if (!list->isValid()) {
            qCWarning(lcWordPrediction) << "unusable bigram list" << language << list->data.size() << "bytes";
            list->data.clear();
            list->keyCount = 0;
            list->entryCount = 0;
            list->wordCount = 0;
        }
    } else {
        qCWarning(lcWordPrediction) << "no bigram list for" << language;
    }

    m_lists.insert(language, list);
    return list;
}

bool WordPredictor::supports(const QString &locale) const
{
    const auto list = listFor(locale);
    return list && list->isValid();
}

QStringList WordPredictor::predict(const QString &previousWord, int limit, const QString &locale) const
{
    if (previousWord.isEmpty() || limit <= 0) {
        return {};
    }

    const auto list = listFor(locale);
    if (!list || !list->isValid()) {
        return {};
    }

    const QByteArray needle = lookupKey(previousWord);
    if (needle.isEmpty()) {
        return {};
    }

    const quint32 index = list->findKey(needle);
    if (index >= list->keyCount || lookupKey(list->key(index)) != needle) {
        return {};
    }

    // The entries of a key are stored most frequent first, so the first ones
    // are the predictions to offer.
    QStringList candidates;
    const quint32 first = list->entryOffset(index);
    const quint32 last = list->entryOffset(index + 1);
    for (quint32 entry = first; entry < last && candidates.size() < limit; ++entry) {
        candidates.append(QString::fromUtf8(list->word(list->entryWord(entry))));
    }
    return candidates;
}
