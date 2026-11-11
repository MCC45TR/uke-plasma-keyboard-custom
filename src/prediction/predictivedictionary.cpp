/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "predictivedictionary.h"

#include "wordlookup.h"

#include <QFile>
#include <QLoggingCategory>
#include <QSet>
#include <QtEndian>

#include <algorithm>

Q_LOGGING_CATEGORY(lcPrediction, "org.uke.plasma.keyboard.prediction")

namespace
{
//! The letters a word of the language is typed with. The corrections put them
//! in place of the letters that were typed, add one of them or drop one.
QString alphabetFor(const QString &language)
{
    if (language == QLatin1String("ru")) {
        return QStringLiteral("абвгдеёжзийклмнопрстуфхцчшщъыьэюя");
    }
    // The apostrophe belongs to the English words that are written with one
    // («don't»), and a correction may be the one that was left out.
    return QStringLiteral("abcdefghijklmnopqrstuvwxyz'");
}
} // namespace

quint32 PredictiveDictionary::WordList::offset(quint32 index) const
{
    return qFromLittleEndian<quint32>(data.constData() + 8 + qsizetype(index) * 4);
}

quint32 PredictiveDictionary::WordList::frequency(quint32 index) const
{
    const qsizetype frequencies = 8 + qsizetype(count + 1) * 4 + offset(count);
    return qFromLittleEndian<quint32>(data.constData() + frequencies + qsizetype(index) * 4);
}

QByteArrayView PredictiveDictionary::WordList::word(quint32 index) const
{
    const qsizetype pool = 8 + qsizetype(count + 1) * 4;
    const quint32 start = offset(index);
    return QByteArrayView(data.constData() + pool + start, offset(index + 1) - start);
}

quint32 PredictiveDictionary::WordList::indexOf(const QByteArray &needle) const
{
    // The words are ordered by the lookup key, so the word itself is one binary
    // search away.
    quint32 low = 0;
    quint32 high = count;
    while (low < high) {
        const quint32 middle = low + (high - low) / 2;
        if (lookupKey(word(middle)) < needle) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }

    if (low < count && lookupKey(word(low)) == needle) {
        return low;
    }
    return count;
}

PredictiveDictionary::PredictiveDictionary(QObject *parent)
    : QObject(parent)
{
}

PredictiveDictionary::~PredictiveDictionary() = default;

std::shared_ptr<const PredictiveDictionary::WordList> PredictiveDictionary::listFor(const QString &locale) const
{
    const QString language = languageOf(locale);
    if (language.isEmpty()) {
        return {};
    }

    const auto cached = m_lists.constFind(language);
    if (cached != m_lists.constEnd()) {
        return *cached;
    }

    auto list = std::make_shared<WordList>();

    QFile file(QStringLiteral(":/prediction/%1.bin").arg(language));
    if (file.open(QIODevice::ReadOnly)) {
        list->data = file.readAll();
        if (list->data.size() >= 8) {
            list->count = qFromLittleEndian<quint32>(list->data.constData() + 4);
        }
        if (!list->isValid()) {
            qCWarning(lcPrediction) << "unusable predictive word list" << language << list->data.size() << "bytes";
            list->data.clear();
            list->count = 0;
        }
    } else {
        qCWarning(lcPrediction) << "no predictive word list for" << language;
    }

    m_lists.insert(language, list);
    return list;
}

bool PredictiveDictionary::supports(const QString &locale) const
{
    const auto list = listFor(locale);
    if (list && list->isValid()) {
        return true;
    }
    // A language the word lists have none for may still be covered by an
    // installed hunspell dictionary.
    return m_hunspell.isAvailable(locale);
}

QStringList PredictiveDictionary::complete(const QString &prefix, int limit, const QString &locale) const
{
    if (prefix.isEmpty() || limit <= 0) {
        return {};
    }

    const auto list = listFor(locale);
    if (!list || !list->isValid()) {
        // The word lists have no words for this language; an installed hunspell
        // dictionary may still have some.
        return m_hunspell.complete(prefix, limit, locale);
    }

    const QByteArray needle = lookupKey(prefix);
    if (needle.isEmpty()) {
        return {};
    }

    // The words are ordered by the lookup key, so the words starting with the
    // prefix are one range of it: from the prefix itself up to the prefix with
    // a byte that cannot start a character after it.
    const auto lowerBound = [list](const QByteArray &value) {
        quint32 low = 0;
        quint32 high = list->count;
        while (low < high) {
            const quint32 middle = low + (high - low) / 2;
            if (lookupKey(list->word(middle)) < value) {
                low = middle + 1;
            } else {
                high = middle;
            }
        }
        return low;
    };

    QByteArray upper = needle;
    upper.append(char(0xff));

    const quint32 first = lowerBound(needle);
    const quint32 last = lowerBound(upper);

    QList<quint32> matches;
    matches.reserve(last - first);
    for (quint32 index = first; index < last; ++index) {
        matches.append(index);
    }

    const auto moreFrequent = [list](quint32 left, quint32 right) {
        return list->frequency(left) > list->frequency(right);
    };
    if (matches.size() > limit) {
        std::partial_sort(matches.begin(), matches.begin() + limit, matches.end(), moreFrequent);
        matches.resize(limit);
    } else {
        std::sort(matches.begin(), matches.end(), moreFrequent);
    }

    QStringList candidates;
    candidates.reserve(matches.size());
    for (const quint32 index : std::as_const(matches)) {
        candidates.append(matchCase(QString::fromUtf8(list->word(index)), prefix));
    }
    return candidates;
}

QStringList PredictiveDictionary::correct(const QString &word, int limit, const QString &locale) const
{
    if (word.isEmpty() || limit <= 0) {
        return {};
    }

    const auto list = listFor(locale);
    if (!list || !list->isValid()) {
        // The language has no word list of its own: the hunspell dictionary, if
        // one is installed, is the only thing that knows its words.
        return correctWithHunspell(word, limit, locale);
    }

    const QString alphabet = alphabetFor(languageOf(locale));
    const QString typed = word.toLower();

    QList<quint32> matches;
    QSet<quint32> seen;
    // A correction is a word of the list itself: a word that is one typo away
    // from what was typed.
    const auto consider = [&](const QString &candidate) {
        const quint32 index = list->indexOf(lookupKey(candidate));
        if (index == list->count || seen.contains(index)) {
            return;
        }
        seen.insert(index);
        matches.append(index);
    };

    // A letter was dropped («привет» -> «привт»).
    for (int position = 0; position < typed.size(); ++position) {
        QString candidate = typed;
        candidate.remove(position, 1);
        consider(candidate);
    }
    // Two neighbouring letters were swapped («привет» -> «првиет»).
    for (int position = 0; position + 1 < typed.size(); ++position) {
        QString candidate = typed;
        const QChar first = candidate.at(position);
        candidate[position] = candidate.at(position + 1);
        candidate[position + 1] = first;
        consider(candidate);
    }
    // A letter was typed instead of another one («привет» -> «превет»).
    for (int position = 0; position < typed.size(); ++position) {
        for (const QChar letter : alphabet) {
            QString candidate = typed;
            candidate[position] = letter;
            consider(candidate);
        }
    }
    // A letter was typed too many («привет» -> «привтет»).
    for (int position = 0; position <= typed.size(); ++position) {
        for (const QChar letter : alphabet) {
            QString candidate = typed;
            candidate.insert(position, letter);
            consider(candidate);
        }
    }

    // The word list found nothing one typo away, so the word may have more than
    // one typo in it: hunspell looks further than the word list does.
    if (matches.isEmpty()) {
        return correctWithHunspell(word, limit, locale);
    }

    const auto moreFrequent = [list](quint32 left, quint32 right) {
        return list->frequency(left) > list->frequency(right);
    };
    if (matches.size() > limit) {
        std::partial_sort(matches.begin(), matches.begin() + limit, matches.end(), moreFrequent);
        matches.resize(limit);
    } else {
        std::sort(matches.begin(), matches.end(), moreFrequent);
    }

    QStringList candidates;
    candidates.reserve(matches.size());
    for (const quint32 index : std::as_const(matches)) {
        candidates.append(matchCase(QString::fromUtf8(list->word(index)), word));
    }
    return candidates;
}

QStringList PredictiveDictionary::correctWithHunspell(const QString &word, int limit, const QString &locale) const
{
    // The word lists know how frequent their words are, so they rank the
    // corrections better than hunspell does; this is what is asked when they
    // have nothing to offer, or when the language has no word list at all.
    if (!m_hunspell.isAvailable(locale)) {
        return {};
    }

    // A word hunspell knows is spelled right, there is nothing to correct.
    if (m_hunspell.spell(word, locale)) {
        return {};
    }

    QStringList suggestions = m_hunspell.suggest(word, limit, locale);
    for (QString &suggestion : suggestions) {
        suggestion = matchCase(suggestion, word);
    }
    return suggestions;
}
