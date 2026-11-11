/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "hunspelldictionary.h"

#include "wordlookup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QStringDecoder>

#include <algorithm>

Q_LOGGING_CATEGORY(lcHunspell, "org.uke.plasma.keyboard.prediction.hunspell")

//! The handle libhunspell works with. The library declares it as an opaque
//! struct, and so does this file: hunspell is loaded while the application
//! runs, so its header is not needed to build against.
struct Hunhandle;

namespace
{
/**
 * The part of the C API of libhunspell the engine uses, as hunspell.h declares
 * it. The functions are looked up in the library when it is loaded.
 */
struct HunspellApi {
    using Create = Hunhandle *(*)(const char *affixPath, const char *dictionaryPath);
    using Destroy = void (*)(Hunhandle *handle);
    using Spell = int (*)(Hunhandle *handle, const char *word);
    using Suggest = int (*)(Hunhandle *handle, char ***suggestions, const char *word);
    using FreeList = void (*)(Hunhandle *handle, char ***suggestions, int count);
    using DictionaryEncoding = char *(*)(Hunhandle * handle);

    Create create = nullptr;
    Destroy destroy = nullptr;
    Spell spell = nullptr;
    Suggest suggest = nullptr;
    FreeList freeList = nullptr;
    DictionaryEncoding dictionaryEncoding = nullptr;

    bool isValid() const
    {
        return create && destroy && spell && suggest && freeList;
    }
};

//! The names libhunspell is installed under, the versioned one first.
const char *const libraryNames[] = {
    "libhunspell-1.7.so.0",
    "libhunspell.so.0",
    "libhunspell.so",
    "hunspell-1.7",
    "hunspell",
};

//! The loaded library, kept alive as long as the functions found in it are
//! used.
QLibrary &hunspellLibrary()
{
    static QLibrary library;
    return library;
}

const HunspellApi &hunspellApi()
{
    static const HunspellApi api = [] {
        HunspellApi loaded;
        QLibrary &library = hunspellLibrary();
        for (const char *name : libraryNames) {
            library.setFileName(QString::fromLatin1(name));
            if (!library.load()) {
                continue;
            }

            loaded.create = reinterpret_cast<HunspellApi::Create>(library.resolve("Hunspell_create"));
            loaded.destroy = reinterpret_cast<HunspellApi::Destroy>(library.resolve("Hunspell_destroy"));
            loaded.spell = reinterpret_cast<HunspellApi::Spell>(library.resolve("Hunspell_spell"));
            loaded.suggest = reinterpret_cast<HunspellApi::Suggest>(library.resolve("Hunspell_suggest"));
            loaded.freeList = reinterpret_cast<HunspellApi::FreeList>(library.resolve("Hunspell_free_list"));
            loaded.dictionaryEncoding = reinterpret_cast<HunspellApi::DictionaryEncoding>(library.resolve("Hunspell_get_dic_encoding"));

            if (loaded.isValid()) {
                library.setLoadHints(QLibrary::PreventUnloadHint);
                qCDebug(lcHunspell) << "loaded" << library.fileName();
                return loaded;
            }

            library.unload();
        }

        qCDebug(lcHunspell) << "libhunspell is not installed";
        return loaded;
    }();
    return api;
}

//! The directories a dictionary may be installed in.
QStringList dictionaryDirectories()
{
    QStringList directories;
    const QStringList dataDirectories = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &dataDirectory : dataDirectories) {
        directories.append(dataDirectory + QStringLiteral("/hunspell"));
    }
    // The directories of the classic spell checkers and of Qt Virtual
    // Keyboard's own hunspell plugin, which does not use the standard ones.
    directories.append(QStringLiteral("/usr/share/myspell"));
    directories.append(QStringLiteral("/usr/share/myspell/dicts"));
    directories.append(QStringLiteral("/usr/share/qt6/qtvirtualkeyboard/hunspell"));
    directories.append(QDir::homePath() + QStringLiteral("/.hunspell"));
    directories.removeDuplicates();
    return directories;
}

//! The names a dictionary of @p locale may be installed under: the whole
//! locale first ("ru_RU"), then the language alone ("ru").
QStringList dictionaryNames(const QString &locale)
{
    QStringList names;
    QString normalized = locale;
    normalized.replace(QChar(u'-'), QChar(u'_'));
    if (!normalized.isEmpty()) {
        names.append(normalized);
    }
    const QString language = languageOf(locale);
    if (!language.isEmpty()) {
        names.append(language);
    }
    names.removeDuplicates();
    return names;
}

//! The .aff/.dic pair of @p locale, empty when none is installed.
QPair<QString, QString> dictionaryFiles(const QString &locale)
{
    const QStringList directories = dictionaryDirectories();
    const QStringList names = dictionaryNames(locale);
    for (const QString &directory : directories) {
        for (const QString &name : names) {
            const QString affix = directory + QLatin1Char('/') + name + QStringLiteral(".aff");
            const QString dictionary = directory + QLatin1Char('/') + name + QStringLiteral(".dic");
            if (QFileInfo::exists(affix) && QFileInfo::exists(dictionary)) {
                return {affix, dictionary};
            }
        }
    }
    return {};
}
} // namespace

struct HunspellDictionary::Handle {
    //! The opened dictionary, null when it could not be opened.
    std::shared_ptr<Hunhandle> hunspell;
    //! The .dic file, read for the list of words.
    QString dictionaryPath;
    bool valid = false;
};

HunspellDictionary::HunspellDictionary() = default;

HunspellDictionary::~HunspellDictionary() = default;

bool HunspellDictionary::isLibraryAvailable()
{
    return hunspellApi().isValid();
}

std::shared_ptr<HunspellDictionary::Handle> HunspellDictionary::handleFor(const QString &locale) const
{
    const auto cached = m_handles.constFind(locale);
    if (cached != m_handles.constEnd()) {
        return *cached;
    }

    m_handles.clear();
    m_words.clear();
    auto handle = std::make_shared<Handle>();
    const QPair<QString, QString> files = dictionaryFiles(locale);
    if (!files.first.isEmpty()) {
        handle->dictionaryPath = files.second;
        const HunspellApi &api = hunspellApi();
        if (api.isValid()) {
            Hunhandle *dictionary = api.create(files.first.toUtf8().constData(), files.second.toUtf8().constData());
            if (dictionary) {
                handle->hunspell = std::shared_ptr<Hunhandle>(dictionary, [](Hunhandle *open) {
                    hunspellApi().destroy(open);
                });
                handle->valid = true;
            } else {
                qCWarning(lcHunspell) << "could not open the hunspell dictionary" << files.first;
            }
        }
    }

    if(handle->valid)m_handles.insert(locale, handle);
    return handle;
}

bool HunspellDictionary::isAvailable(const QString &locale) const
{
    const auto handle = handleFor(locale);
    return handle && handle->valid;
}

bool HunspellDictionary::spell(const QString &word, const QString &locale) const
{
    if (word.isEmpty()) {
        return false;
    }

    const auto handle = handleFor(locale);
    if (!handle || !handle->valid) {
        return false;
    }

    return hunspellApi().spell(handle->hunspell.get(), word.toUtf8().constData()) != 0;
}

QStringList HunspellDictionary::suggest(const QString &word, int limit, const QString &locale) const
{
    if (word.isEmpty() || limit <= 0) {
        return {};
    }

    const auto handle = handleFor(locale);
    if (!handle || !handle->valid) {
        return {};
    }

    const HunspellApi &api = hunspellApi();
    char **suggestions = nullptr;
    const int count = api.suggest(handle->hunspell.get(), &suggestions, word.toUtf8().constData());
    QStringList words;
    for (int index = 0; index < count && words.size() < limit; ++index) {
        words.append(QString::fromUtf8(suggestions[index]));
    }
    api.freeList(handle->hunspell.get(), &suggestions, count);
    return words;
}

std::shared_ptr<const QStringList> HunspellDictionary::wordsFor(const QString &locale) const
{
    const auto cached = m_words.constFind(locale);
    if (cached != m_words.constEnd()) {
        return *cached;
    }

    auto words = std::make_shared<QStringList>();
    const auto handle = handleFor(locale);
    if (handle && handle->valid && !handle->dictionaryPath.isEmpty()) {
        QFile file(handle->dictionaryPath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray encoding = "UTF-8";
            const HunspellApi &api = hunspellApi();
            if (api.dictionaryEncoding) {
                if (const char *name = api.dictionaryEncoding(handle->hunspell.get())) {
                    encoding = name;
                }
            }
            // The .dic files are usually UTF-8; an encoding Qt cannot decode
            // falls back to UTF-8, which is what the dictionaries we ship with
            // use.
            QStringDecoder decoder(encoding.constData());
            if (!decoder.isValid()) {
                decoder = QStringDecoder(QStringDecoder::Utf8);
            }

            const QString text = decoder(file.readAll());
            const QStringList lines = text.split(QChar(u'\n'));
            // The first line is the number of words.
            for (int index = 1; index < lines.size(); ++index) {
                QString line = lines.at(index).trimmed();
                if (line.isEmpty()) {
                    continue;
                }
                // A line is the word, the flags after a slash and, sometimes,
                // the morphology after a tab or a space.
                int cut = line.indexOf(QChar(u'/'));
                if (cut < 0) {
                    cut = line.indexOf(QChar(u'\t'));
                }
                if (cut < 0) {
                    cut = line.indexOf(QChar(u' '));
                }
                if (cut >= 0) {
                    line = line.left(cut);
                }
                line = line.trimmed();
                if (!line.isEmpty()) {
                    words->append(line);
                }
            }
            words->removeDuplicates();
            std::sort(words->begin(), words->end(), [](const QString &left, const QString &right) {
                return lookupKey(left) < lookupKey(right);
            });
        } else {
            qCWarning(lcHunspell) << "could not read the hunspell word list" << handle->dictionaryPath;
        }
    }

    m_words.insert(locale, words);
    return words;
}

QStringList HunspellDictionary::complete(const QString &prefix, int limit, const QString &locale) const
{
    if (prefix.isEmpty() || limit <= 0) {
        return {};
    }

    const auto words = wordsFor(locale);
    if (!words || words->isEmpty()) {
        return {};
    }

    const QByteArray needle = lookupKey(prefix);
    if (needle.isEmpty()) {
        return {};
    }

    // The words are ordered by the lookup key, so the words starting with the
    // prefix are one range of it.
    const auto lowerBound = [words](const QByteArray &value) {
        int low = 0;
        int high = words->size();
        while (low < high) {
            const int middle = low + (high - low) / 2;
            if (lookupKey(words->at(middle)) < value) {
                low = middle + 1;
            } else {
                high = middle;
            }
        }
        return low;
    };

    QByteArray upper = needle;
    upper.append(char(0xff));
    const int first = lowerBound(needle);
    const int last = lowerBound(upper);

    QList<int> matches;
    matches.reserve(last - first);
    for (int index = first; index < last; ++index) {
        matches.append(index);
    }

    // There are no frequencies to rank the words with, so the shorter ones come
    // first: what is typed is more likely to be completed by a short word.
    const auto shorterFirst = [words](int left, int right) {
        const int leftLength = words->at(left).size();
        const int rightLength = words->at(right).size();
        if (leftLength != rightLength) {
            return leftLength < rightLength;
        }
        return words->at(left) < words->at(right);
    };
    if (matches.size() > limit) {
        std::partial_sort(matches.begin(), matches.begin() + limit, matches.end(), shorterFirst);
        matches.resize(limit);
    } else {
        std::sort(matches.begin(), matches.end(), shorterFirst);
    }

    QStringList candidates;
    candidates.reserve(matches.size());
    for (const int index : std::as_const(matches)) {
        candidates.append(matchCase(words->at(index), prefix));
    }
    return candidates;
}
