// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QLocale>
#include <QVariantList>
#include <QVariantMap>
class QIODevice;
namespace UkeKeyboard {
// XKB layout identifiers denote hardware regions, not ISO language codes.
// Keep registry metadata and variant overrides as the language source.
class LayoutRegistry {
public:
    explicit LayoutRegistry(QIODevice *device);
    static const LayoutRegistry &system();
    QVariantList layouts() const;
    QVariantList variants(const QString &layout) const;
    QString locale(const QString &layout,const QString &variant={},QLocale preferred=QLocale::system()) const;
    QString defaultLayout(QLocale preferred=QLocale::system()) const;
private:
    QVariantMap entry(const QString &layout,const QString &variant) const;
    QVariantList m_layouts;
};
}
