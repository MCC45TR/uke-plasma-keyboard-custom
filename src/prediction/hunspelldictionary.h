/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <memory>

/**
 * The hunspell dictionaries installed in the system, used when there are any.
 *
 * Hunspell knows the spelling of a language and the words a misspelled one may
 * have been meant to be, and its dictionaries cover the languages the compiled
 * word lists have none for. The dictionaries are looked for in the usual places
 * (/usr/share/hunspell, /usr/share/myspell, the Qt Virtual Keyboard directory,
 * ~/.local/share/hunspell) and are only opened when they are asked for.
 *
 * The library itself is loaded while the application runs, so that hunspell
 * stays an optional dependency: without libhunspell, and without a dictionary,
 * every question is simply answered with «no».
 */
class HunspellDictionary
{
public:
    HunspellDictionary();
    ~HunspellDictionary();

    /**
     * Whether libhunspell could be loaded. False when it is not installed.
     */
    static bool isLibraryAvailable();
    //! Presence check for settings; does not load the library or dictionary.
    static bool hasDictionaryFiles(const QString &locale);

    /**
     * Whether a dictionary for @p locale is installed.
     */
    bool isAvailable(const QString &locale) const;

    /**
     * Whether @p word is spelled the way the dictionary of @p locale spells it.
     */
    bool spell(const QString &word, const QString &locale) const;

    /**
     * The words the dictionary of @p locale suggests for @p word, the most
     * likely first. Empty when no dictionary is installed or nothing is close
     * enough to @p word.
     */
    QStringList suggest(const QString &word, int limit, const QString &locale) const;

    /**
     * The words of the dictionary of @p locale starting with @p prefix, the
     * shortest first. Used for the languages the word lists have none for: the
     * dictionary has no frequencies to rank the words with.
     */
    QStringList complete(const QString &prefix, int limit, const QString &locale) const;

private:
    /**
     * One opened dictionary: the hunspell handle and the file it came from.
     * The handle is what libhunspell works with, the file is read for the list
     * of words.
     */
    struct Handle;

    std::shared_ptr<Handle> handleFor(const QString &locale) const;
    std::shared_ptr<const QStringList> wordsFor(const QString &locale) const;

    mutable QHash<QString, std::shared_ptr<Handle>> m_handles;
    mutable QHash<QString, std::shared_ptr<const QStringList>> m_words;
};
