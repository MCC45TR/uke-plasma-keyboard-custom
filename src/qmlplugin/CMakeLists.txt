# SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
# SPDX-License-Identifier: BSD-2-Clause

ecm_add_qml_module(ukekeyboardlib URI org.uke.plasma.keyboard.lib GENERATE_PLUGIN_SOURCE DEPENDENCIES QtQuick)

set_source_files_properties(
    BreezeConstants.qml
    Theme.qml
    PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

ecm_target_qml_sources(ukekeyboardlib SOURCES
    BreezeKeyPanel.qml
    BreezeConstants.qml
    BreezePopup.qml
    FloatingKey.qml
    HideKey.qml
    LanguageKey.qml
    MicrophoneKey.qml
    ModifierKey.qml
    Theme.qml
    ThemePalette.qml
    ThemeSystemPalette.qml
    ThemeLightPalette.qml
    ThemeDarkPalette.qml
    ThemeIosLightPalette.qml
    ThemeIosDarkPalette.qml
    ThemeMaterialLightPalette.qml
    ThemeMaterialDarkPalette.qml
    ThemeCatppuccinMochaPalette.qml
    ThemeGoogleLightPalette.qml
    ThemeGoogleDarkPalette.qml
)

target_link_libraries(ukekeyboardlib
    PUBLIC
        Qt::Core
    PRIVATE
        Qt::Qml
        Qt::Gui
        Qt::Quick
)

ecm_finalize_qml_module(ukekeyboardlib)
