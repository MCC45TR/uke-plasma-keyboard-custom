/*
    SPDX-FileCopyrightText: 2026 uke-plasma-keyboard-custom contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

/**
 * Scrollable page holding a column of SettingsRow items, used as a tab page in
 * the settings KCM.
 */
QQC2.ScrollView {
    id: page

    default property alias content: contentColumn.data

    contentWidth: availableWidth
    clip: true
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.largeSpacing

    ColumnLayout {
        id: contentColumn

        // Fill about 80% of the page and stay centred: the settings used to be
        // a narrow strip in the middle with all controls bunched together.
        width: Math.min(page.availableWidth, Math.max(Kirigami.Units.gridUnit * 24, page.availableWidth * 0.8))
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Kirigami.Units.largeSpacing
    }
}
