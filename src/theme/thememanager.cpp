/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "thememanager.h"

#include <KLocalizedString>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSValue>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QQmlEngine>
#include <QSet>
#include <QStandardPaths>
#include <QVariant>

namespace
{
// Colours of the "palette" section: every writable colour property of
// ThemePalette.qml. Keep this in sync with the palette.
const QSet<QString> &paletteKeys()
{
    static const QSet<QString> keys = {
        QStringLiteral("primaryColor"),
        QStringLiteral("primaryLightColor"),
        QStringLiteral("primaryDarkColor"),
        QStringLiteral("textOnPrimaryColor"),
        QStringLiteral("secondaryColor"),
        QStringLiteral("secondaryLightColor"),
        QStringLiteral("secondaryDarkColor"),
        QStringLiteral("textOnSecondaryColor"),
        QStringLiteral("keyboardBackgroundColor"),
        QStringLiteral("normalKeyBackgroundColor"),
        QStringLiteral("normalKeyPressedBackgroundColor"),
        QStringLiteral("highlightedKeyBackgroundColor"),
        QStringLiteral("latchedKeyBackgroundColor"),
        QStringLiteral("capsLockKeyAccentColor"),
        QStringLiteral("modeKeyAccentColor"),
        QStringLiteral("keyTextColor"),
        QStringLiteral("keySmallTextColor"),
        QStringLiteral("popupBackgroundColor"),
        QStringLiteral("popupBorderColor"),
        QStringLiteral("popupTextColor"),
        QStringLiteral("popupTextSelectedColor"),
        QStringLiteral("popupHighlightBorderColor"),
        QStringLiteral("popupHighlightColor"),
        QStringLiteral("selectionListTextColor"),
        QStringLiteral("selectionListSeparatorColor"),
        QStringLiteral("selectionListBackgroundColor"),
        QStringLiteral("navigationHighlightColor"),
        QStringLiteral("navigationHighlightBorderColor"),
    };
    return keys;
}

const QSet<QString> &geometryKeys()
{
    static const QSet<QString> keys = {
        QStringLiteral("keyBackgroundMargin"),
        QStringLiteral("keyContentMargin"),
        QStringLiteral("keyIconScale"),
        QStringLiteral("buttonRadius"),
        QStringLiteral("popupRadius"),
    };
    return keys;
}

const QSet<QString> &backgroundKeys()
{
    static const QSet<QString> keys = {
        QStringLiteral("type"),
        QStringLiteral("start"),
        QStringLiteral("end"),
        QStringLiteral("angle"),
    };
    return keys;
}

const QSet<QString> &keyStyleKeys()
{
    static const QSet<QString> keys = {
        QStringLiteral("outlineWidth"),
        QStringLiteral("outlineColor"),
        QStringLiteral("shadowStrength"),
        QStringLiteral("labelCase"),
    };
    return keys;
}

const QSet<QString> &categoryKeys()
{
    static const QSet<QString> keys = {
        QStringLiteral("suggestions"),
        QStringLiteral("modifier"),
        QStringLiteral("function"),
        QStringLiteral("accent"),
        QStringLiteral("digit"),
        QStringLiteral("normal"),
    };
    return keys;
}

const QSet<QString> &stateKeys()
{
    static const QSet<QString> keys = {
        QStringLiteral("normal"),
        QStringLiteral("pressed"),
        QStringLiteral("highlighted"),
        QStringLiteral("latched"),
        QStringLiteral("active"),
        QStringLiteral("text"),
    };
    return keys;
}

const QSet<QString> &outlineKeys()
{
    static const QSet<QString> keys = {
        QStringLiteral("width"),
        QStringLiteral("color"),
    };
    return keys;
}

// The colour and number properties read back for an export.
const char *const s_effectiveColors[] = {
    "primaryColor",
    "primaryLightColor",
    "primaryDarkColor",
    "textOnPrimaryColor",
    "secondaryColor",
    "secondaryLightColor",
    "secondaryDarkColor",
    "textOnSecondaryColor",
    "keyboardBackgroundColor",
    "normalKeyBackgroundColor",
    "normalKeyPressedBackgroundColor",
    "highlightedKeyBackgroundColor",
    "latchedKeyBackgroundColor",
    "capsLockKeyAccentColor",
    "modeKeyAccentColor",
    "keyTextColor",
    "keySmallTextColor",
    "popupBackgroundColor",
    "popupBorderColor",
    "popupTextColor",
    "popupTextSelectedColor",
    "popupHighlightBorderColor",
    "popupHighlightColor",
    "selectionListTextColor",
    "selectionListSeparatorColor",
    "selectionListBackgroundColor",
    "navigationHighlightColor",
    "navigationHighlightBorderColor",
};

const char *const s_effectiveGeometry[] = {
    "keyBackgroundMargin",
    "keyContentMargin",
    "keyIconScale",
    "buttonRadius",
    "popupRadius",
};

QString describe(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::String:
        return QStringLiteral("\"%1\"").arg(value.toString());
    case QJsonValue::Double: {
        const double number = value.toDouble();
        if (qFuzzyCompare(number, qRound(number))) {
            return QString::number(qRound(number));
        }
        return QString::number(number);
    }
    case QJsonValue::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Object:
        return QStringLiteral("an object");
    case QJsonValue::Array:
        return QStringLiteral("an array");
    default:
        return QStringLiteral("null");
    }
}

bool isColor(const QJsonValue &value)
{
    return value.isString() && QColor(value.toString()).isValid();
}

QString colorToJson(const QColor &color)
{
    return color.alpha() < 255 ? color.name(QColor::HexArgb) : color.name(QColor::HexRgb);
}

QString parseErrorString(const QJsonParseError &error)
{
    return QStringLiteral("Not a valid JSON file: %1").arg(error.errorString());
}
} // namespace

ThemeManager *ThemeManager::instance()
{
    static ThemeManager s_instance;
    return &s_instance;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

void ThemeManager::setQmlEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

QString ThemeManager::themesDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-keyboard/themes");
}

bool ThemeManager::isBuiltinId(const QString &id)
{
    static const QSet<QString> ids = {
        QStringLiteral("system"),
        QStringLiteral("light"),
        QStringLiteral("dark"),
        QStringLiteral("ios-light"),
        QStringLiteral("ios-dark"),
        QStringLiteral("material-light"),
        QStringLiteral("material-dark"),
        QStringLiteral("catppuccin-mocha"),
        QStringLiteral("google-light"),
        QStringLiteral("google-dark"),
    };
    return ids.contains(id);
}

QString ThemeManager::slugify(const QString &name)
{
    QString slug;
    bool pendingSeparator = false;
    for (const QChar character : name) {
        if (character.isLetterOrNumber()) {
            if (pendingSeparator && !slug.isEmpty()) {
                slug.append(QLatin1Char('-'));
            }
            pendingSeparator = false;
            slug.append(character.toLower());
        } else {
            pendingSeparator = true;
        }
    }
    return slug;
}

QVariantList ThemeManager::builtinThemes()
{
    const auto entry = [](const char *id, const QString &name) {
        return QVariantMap{
            {QStringLiteral("id"), QString::fromLatin1(id)},
            {QStringLiteral("name"), name},
            {QStringLiteral("source"), QStringLiteral("builtin")},
            {QStringLiteral("base"), QString::fromLatin1(id)},
        };
    };
    return {
        entry("system", i18n("System")),
        entry("light", i18n("Light")),
        entry("dark", i18n("Dark")),
        entry("ios-light", i18n("iOS (light)")),
        entry("ios-dark", i18n("iOS (dark)")),
        entry("material-light", i18n("Material (light)")),
        entry("material-dark", i18n("Material (dark)")),
        entry("catppuccin-mocha", i18n("Catppuccin Mocha")),
        entry("google-light", i18n("Google Light")),
        entry("google-dark", i18n("Google Dark")),
    };
}

QVariantList ThemeManager::availableThemes() const
{
    QVariantList themes = builtinThemes();

    QDir directory(themesDirectory());
    const QStringList files = directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        const QString id = QFileInfo(file).completeBaseName();
        QString name = id;
        QString base = QStringLiteral("system");

        QFile themeFile(directory.filePath(file));
        if (themeFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument document = QJsonDocument::fromJson(themeFile.readAll());
            if (document.isObject()) {
                const QJsonObject object = document.object();
                if (object.value(QStringLiteral("name")).isString() && !object.value(QStringLiteral("name")).toString().isEmpty()) {
                    name = object.value(QStringLiteral("name")).toString();
                }
                if (object.value(QStringLiteral("base")).isString() && !object.value(QStringLiteral("base")).toString().isEmpty()) {
                    base = object.value(QStringLiteral("base")).toString();
                }
            }
        }

        themes.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("source"), QStringLiteral("user")},
            {QStringLiteral("base"), base},
        });
    }

    return themes;
}

QString ThemeManager::readTextFile(const QUrl &url) const
{
    if (!url.isLocalFile()) {
        return {};
    }
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QVariantMap ThemeManager::themeDescription(const QString &id) const
{
    if (isBuiltinId(id)) {
        return {};
    }
    QFile file(themesDirectory() + QLatin1Char('/') + id + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }
    return document.object().toVariantMap();
}

bool ThemeManager::parseTheme(const QJsonObject &root, const QString &fallbackName, ThemeData &out, QString &error)
{
    static const QSet<QString> topLevelKeys = {
        QStringLiteral("name"),
        QStringLiteral("base"),
        QStringLiteral("palette"),
        QStringLiteral("geometry"),
        QStringLiteral("background"),
        QStringLiteral("keyStyle"),
        QStringLiteral("keyColors"),
    };

    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        if (!topLevelKeys.contains(it.key())) {
            error = QStringLiteral("Unknown top-level key: \"%1\"").arg(it.key());
            return false;
        }
    }

    // Name: the explicit argument wins, then the JSON name, then the file name.
    QString name = fallbackName.trimmed();
    if (root.contains(QStringLiteral("name"))) {
        const QJsonValue value = root.value(QStringLiteral("name"));
        if (!value.isString()) {
            error = QStringLiteral("name: expected a string, got %1").arg(describe(value));
            return false;
        }
        if (!value.toString().trimmed().isEmpty()) {
            name = value.toString().trimmed();
        }
    }
    if (name.isEmpty()) {
        error = QStringLiteral("name: expected a non-empty string");
        return false;
    }
    out.name = name;

    // Base: optional, must be one of the built-in ids.
    out.base = QStringLiteral("system");
    if (root.contains(QStringLiteral("base"))) {
        const QJsonValue value = root.value(QStringLiteral("base"));
        if (!value.isString()) {
            error = QStringLiteral("base: expected a string, got %1").arg(describe(value));
            return false;
        }
        if (!isBuiltinId(value.toString())) {
            error = QStringLiteral("base: unknown theme \"%1\"").arg(value.toString());
            return false;
        }
        out.base = value.toString();
    }

    // palette: colours only.
    if (root.contains(QStringLiteral("palette"))) {
        const QJsonValue value = root.value(QStringLiteral("palette"));
        if (!value.isObject()) {
            error = QStringLiteral("palette: expected an object, got %1").arg(describe(value));
            return false;
        }
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!paletteKeys().contains(it.key())) {
                error = QStringLiteral("palette: unknown key \"%1\"").arg(it.key());
                return false;
            }
            if (!it.value().isString()) {
                error = QStringLiteral("palette.%1: expected a colour string, got %2").arg(it.key(), describe(it.value()));
                return false;
            }
            if (!isColor(it.value())) {
                error = QStringLiteral("palette.%1: not a colour: \"%2\"").arg(it.key(), it.value().toString());
                return false;
            }
        }
        out.palette = object;
    }

    // geometry: numbers only (or the number of pixels/margins).
    if (root.contains(QStringLiteral("geometry"))) {
        const QJsonValue value = root.value(QStringLiteral("geometry"));
        if (!value.isObject()) {
            error = QStringLiteral("geometry: expected an object, got %1").arg(describe(value));
            return false;
        }
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!geometryKeys().contains(it.key())) {
                error = QStringLiteral("geometry: unknown key \"%1\"").arg(it.key());
                return false;
            }
            if (!it.value().isDouble()) {
                error = QStringLiteral("geometry.%1: expected a number, got %2").arg(it.key(), describe(it.value()));
                return false;
            }
        }
        out.geometry = object;
    }

    // background: type/start/end/angle.
    if (root.contains(QStringLiteral("background"))) {
        const QJsonValue value = root.value(QStringLiteral("background"));
        if (!value.isObject()) {
            error = QStringLiteral("background: expected an object, got %1").arg(describe(value));
            return false;
        }
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!backgroundKeys().contains(it.key())) {
                error = QStringLiteral("background: unknown key \"%1\"").arg(it.key());
                return false;
            }
            const QJsonValue field = it.value();
            if (it.key() == QLatin1String("type")) {
                if (!field.isString() || (field.toString() != QLatin1String("color") && field.toString() != QLatin1String("gradient"))) {
                    error = QStringLiteral("background.type: expected \"color\" or \"gradient\", got %1").arg(describe(field));
                    return false;
                }
            } else if (it.key() == QLatin1String("start") || it.key() == QLatin1String("end")) {
                if (!field.isString()) {
                    error = QStringLiteral("background.%1: expected a colour string, got %2").arg(it.key(), describe(field));
                    return false;
                }
                if (!isColor(field)) {
                    error = QStringLiteral("background.%1: not a colour: \"%2\"").arg(it.key(), field.toString());
                    return false;
                }
            } else if (it.key() == QLatin1String("angle")) {
                if (!field.isDouble()) {
                    error = QStringLiteral("background.angle: expected a number, got %1").arg(describe(field));
                    return false;
                }
                const int angle = field.toInt();
                if (angle != 0 && angle != 90 && angle != 180 && angle != 270) {
                    error = QStringLiteral("background.angle: expected 0, 90, 180 or 270, got %1").arg(angle);
                    return false;
                }
            }
        }
        out.background = object;
    }

    // keyStyle: outline width/colour, shadow strength, label case.
    if (root.contains(QStringLiteral("keyStyle"))) {
        const QJsonValue value = root.value(QStringLiteral("keyStyle"));
        if (!value.isObject()) {
            error = QStringLiteral("keyStyle: expected an object, got %1").arg(describe(value));
            return false;
        }
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!keyStyleKeys().contains(it.key())) {
                error = QStringLiteral("keyStyle: unknown key \"%1\"").arg(it.key());
                return false;
            }
            const QJsonValue field = it.value();
            if (it.key() == QLatin1String("outlineWidth") || it.key() == QLatin1String("shadowStrength")) {
                if (!field.isDouble()) {
                    error = QStringLiteral("keyStyle.%1: expected a number, got %2").arg(it.key(), describe(field));
                    return false;
                }
            } else if (it.key() == QLatin1String("outlineColor")) {
                if (!field.isString()) {
                    error = QStringLiteral("keyStyle.outlineColor: expected a colour string, got %1").arg(describe(field));
                    return false;
                }
                if (!isColor(field)) {
                    error = QStringLiteral("keyStyle.outlineColor: not a colour: \"%1\"").arg(field.toString());
                    return false;
                }
            } else if (it.key() == QLatin1String("labelCase")) {
                if (!field.isString() || (field.toString() != QLatin1String("normal") && field.toString() != QLatin1String("upper"))) {
                    error = QStringLiteral("keyStyle.labelCase: expected \"normal\" or \"upper\", got %1").arg(describe(field));
                    return false;
                }
            }
        }
        out.keyStyle = object;
    }

    // keyColors: category -> state -> value.
    if (root.contains(QStringLiteral("keyColors"))) {
        const QJsonValue value = root.value(QStringLiteral("keyColors"));
        if (!value.isObject()) {
            error = QStringLiteral("keyColors: expected an object, got %1").arg(describe(value));
            return false;
        }
        const QJsonObject object = value.toObject();
        for (auto category = object.constBegin(); category != object.constEnd(); ++category) {
            if (!categoryKeys().contains(category.key())) {
                error = QStringLiteral("keyColors: unknown category \"%1\"").arg(category.key());
                return false;
            }
            if (!category.value().isObject()) {
                error = QStringLiteral("keyColors.%1: expected an object, got %2").arg(category.key(), describe(category.value()));
                return false;
            }
            const QJsonObject states = category.value().toObject();
            for (auto state = states.constBegin(); state != states.constEnd(); ++state) {
                const QString path = QStringLiteral("keyColors.%1.%2").arg(category.key(), state.key());
                const QJsonValue field = state.value();
                if (stateKeys().contains(state.key())) {
                    if (!field.isString()) {
                        error = QStringLiteral("%1: expected a colour string, got %2").arg(path, describe(field));
                        return false;
                    }
                    if (!isColor(field)) {
                        error = QStringLiteral("%1: not a colour: \"%2\"").arg(path, field.toString());
                        return false;
                    }
                } else if (state.key() == QLatin1String("outlineWidth") || state.key() == QLatin1String("shadow")) {
                    if (!field.isDouble()) {
                        error = QStringLiteral("%1: expected a number, got %2").arg(path, describe(field));
                        return false;
                    }
                } else if (state.key() == QLatin1String("outlineColor")) {
                    if (!field.isString()) {
                        error = QStringLiteral("%1: expected a colour string, got %2").arg(path, describe(field));
                        return false;
                    }
                    if (!isColor(field)) {
                        error = QStringLiteral("%1: not a colour: \"%2\"").arg(path, field.toString());
                        return false;
                    }
                } else if (state.key() == QLatin1String("outline")) {
                    if (!field.isObject()) {
                        error = QStringLiteral("%1: expected an object, got %2").arg(path, describe(field));
                        return false;
                    }
                    const QJsonObject outline = field.toObject();
                    for (auto outlineField = outline.constBegin(); outlineField != outline.constEnd(); ++outlineField) {
                        if (!outlineKeys().contains(outlineField.key())) {
                            error = QStringLiteral("%1: unknown key \"%2\"").arg(path, outlineField.key());
                            return false;
                        }
                        const QJsonValue outlineValue = outlineField.value();
                        if (outlineField.key() == QLatin1String("width")) {
                            if (!outlineValue.isDouble()) {
                                error = QStringLiteral("%1.width: expected a number, got %2").arg(path, describe(outlineValue));
                                return false;
                            }
                        } else if (outlineField.key() == QLatin1String("color")) {
                            if (!outlineValue.isString()) {
                                error = QStringLiteral("%1.color: expected a colour string, got %2").arg(path, describe(outlineValue));
                                return false;
                            }
                            if (!isColor(outlineValue)) {
                                error = QStringLiteral("%1.color: not a colour: \"%2\"").arg(path, outlineValue.toString());
                                return false;
                            }
                        }
                    }
                } else {
                    error = QStringLiteral("%1: unknown key").arg(path);
                    return false;
                }
            }
        }
        out.keyColors = object;
    }

    return true;
}

QJsonObject ThemeManager::themeToJson(const ThemeData &theme)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), theme.name);
    object.insert(QStringLiteral("base"), theme.base);
    object.insert(QStringLiteral("palette"), theme.palette);
    object.insert(QStringLiteral("geometry"), theme.geometry);
    object.insert(QStringLiteral("background"), theme.background);
    object.insert(QStringLiteral("keyStyle"), theme.keyStyle);
    object.insert(QStringLiteral("keyColors"), theme.keyColors);
    return object;
}

QString ThemeManager::installTheme(const QUrl &source, const QString &name)
{
    if (!source.isLocalFile()) {
        return QStringLiteral("Only local files can be imported.");
    }

    QFile sourceFile(source.toLocalFile());
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        return QStringLiteral("Cannot read the theme file: %1").arg(sourceFile.errorString());
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(sourceFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return parseErrorString(parseError);
    }
    if (!document.isObject()) {
        return QStringLiteral("The theme file must contain a JSON object.");
    }

    ThemeData theme;
    QString error;
    const QString fallbackName = name.trimmed().isEmpty() ? QFileInfo(source.toLocalFile()).completeBaseName() : name.trimmed();
    if (!parseTheme(document.object(), fallbackName, theme, error)) {
        return error;
    }

    if (!name.trimmed().isEmpty()) {
        theme.name = name.trimmed();
    }
    if (theme.name.isEmpty()) {
        return QStringLiteral("The theme name must not be empty.");
    }

    const QString id = slugify(theme.name);
    if (id.isEmpty()) {
        return QStringLiteral("The theme name does not yield a usable file name.");
    }
    if (isBuiltinId(id)) {
        return QStringLiteral("A theme named \"%1\" already exists.").arg(theme.name);
    }

    const QString directory = themesDirectory();
    if (!QDir().mkpath(directory)) {
        return QStringLiteral("Cannot create the theme directory: %1").arg(directory);
    }

    const QString path = directory + QLatin1Char('/') + id + QStringLiteral(".json");
    if (QFile::exists(path)) {
        return QStringLiteral("A theme named \"%1\" already exists.").arg(theme.name);
    }

    QFile target(path);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral("Cannot write the theme file: %1").arg(target.errorString());
    }
    target.write(QJsonDocument(themeToJson(theme)).toJson(QJsonDocument::Indented));
    target.close();

    Q_EMIT themesChanged();
    return {};
}

QString ThemeManager::removeUserTheme(const QString &id)
{
    if (isBuiltinId(id)) {
        return QStringLiteral("Built-in themes cannot be removed.");
    }

    const QString path = themesDirectory() + QLatin1Char('/') + id + QStringLiteral(".json");
    if (!QFile::exists(path)) {
        return QStringLiteral("Unknown user theme: %1").arg(id);
    }
    if (!QFile::remove(path)) {
        return QStringLiteral("Cannot remove the theme file: %1").arg(path);
    }

    Q_EMIT themesChanged();
    return {};
}

QObject *ThemeManager::themeSingleton() const
{
    if (!m_engine) {
        return nullptr;
    }
    const QJSValue theme = m_engine->singletonInstance<QJSValue>("org.uke.plasma.keyboard.lib", "Theme");
    if (theme.isUndefined() || theme.isNull()) {
        return nullptr;
    }
    return theme.toQObject();
}

bool ThemeManager::currentThemeId(QString *id) const
{
    QObject *theme = themeSingleton();
    if (!theme) {
        return false;
    }
    const QVariant value = theme->property("themeId");
    if (!value.isValid()) {
        return false;
    }
    *id = value.toString();
    return true;
}

bool ThemeManager::readEffectiveTheme(ThemeData &out, QString &error) const
{
    QObject *theme = themeSingleton();
    if (!theme) {
        error = QStringLiteral("The QML theme layer is not available.");
        return false;
    }

    QObject *palette = theme->property("current").value<QObject *>();
    if (!palette) {
        error = QStringLiteral("The current palette is not available.");
        return false;
    }

    const auto colorOf = [palette](const char *property) {
        return palette->property(property).value<QColor>();
    };
    const auto numberOf = [palette](const char *property) {
        return palette->property(property).toDouble();
    };
    const auto stringOf = [palette](const char *property) {
        return palette->property(property).toString();
    };

    for (const char *property : s_effectiveColors) {
        out.palette.insert(QString::fromLatin1(property), colorToJson(colorOf(property)));
    }

    for (const char *property : s_effectiveGeometry) {
        out.geometry.insert(QString::fromLatin1(property), numberOf(property));
    }

    out.background.insert(QStringLiteral("type"), stringOf("backgroundType"));
    out.background.insert(QStringLiteral("start"), colorToJson(colorOf("backgroundStart")));
    out.background.insert(QStringLiteral("end"), colorToJson(colorOf("backgroundEnd")));
    out.background.insert(QStringLiteral("angle"), numberOf("backgroundAngle"));

    out.keyStyle.insert(QStringLiteral("outlineWidth"), numberOf("keyOutlineWidth"));
    out.keyStyle.insert(QStringLiteral("outlineColor"), colorToJson(colorOf("keyOutlineColor")));
    out.keyStyle.insert(QStringLiteral("shadowStrength"), numberOf("keyShadowStrength"));
    out.keyStyle.insert(QStringLiteral("labelCase"), stringOf("keyLabelCase"));

    const QVariant keyColors = palette->property("keyColors");
    QVariantMap keyColorsMap;
    if (keyColors.canConvert<QJSValue>()) {
        keyColorsMap = keyColors.value<QJSValue>().toVariant().toMap();
    } else {
        keyColorsMap = keyColors.toMap();
    }
    out.keyColors = QJsonObject::fromVariantMap(keyColorsMap);

    return true;
}

QString ThemeManager::exportTheme(const QString &id, const QUrl &target)
{
    if (!target.isLocalFile()) {
        return QStringLiteral("The export location must be a local file.");
    }

    QString name;
    QString base;
    QString source;
    const QVariantList themes = availableThemes();
    for (const QVariant &entry : themes) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
            name = map.value(QStringLiteral("name")).toString();
            base = map.value(QStringLiteral("base")).toString();
            source = map.value(QStringLiteral("source")).toString();
            break;
        }
    }
    if (source.isEmpty()) {
        return QStringLiteral("Unknown theme: %1").arg(id);
    }

    ThemeData theme;
    theme.name = name;
    theme.base = base;

    QString current;
    const bool isCurrent = currentThemeId(&current) && current == id;
    if (isCurrent) {
        QString error;
        if (!readEffectiveTheme(theme, error)) {
            return error;
        }
    } else if (source == QLatin1String("user")) {
        QFile file(themesDirectory() + QLatin1Char('/') + id + QStringLiteral(".json"));
        if (!file.open(QIODevice::ReadOnly)) {
            return QStringLiteral("Cannot read the theme file: %1").arg(file.errorString());
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            return parseErrorString(parseError);
        }
        if (!document.isObject()) {
            return QStringLiteral("The theme file must contain a JSON object.");
        }
        ThemeData stored;
        QString error;
        if (!parseTheme(document.object(), name, stored, error)) {
            return error;
        }
        stored.name = name;
        stored.base = base;
        theme = stored;
    } else {
        // A built-in that is not applied right now: its overrides live in QML
        // and cannot be read here without instantiating it. The base alone
        // reproduces it on import, which is what matters.
    }

    QFile file(target.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral("Cannot write the theme file: %1").arg(file.errorString());
    }
    file.write(QJsonDocument(themeToJson(theme)).toJson(QJsonDocument::Indented));
    file.close();

    return {};
}
