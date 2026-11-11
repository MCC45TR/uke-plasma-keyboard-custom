# SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
# SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

add_definitions(-DQT_NO_KEYWORDS)

kcmutils_add_qml_kcm(kcm_uke_plasma_keyboard_custom)

kconfig_add_kcfg_files(kcm_uke_plasma_keyboard_custom GENERATE_MOC ../src/plasmakeyboardsettings.kcfgc)

target_sources(kcm_uke_plasma_keyboard_custom PRIVATE
    plasmakeyboardkcm.cpp
    plasmakeyboardkcm.h
    sttmodeldownloader.cpp
    sttmodeldownloader.h
    ../src/stt/sttmodelcatalog.cpp
    ../src/stt/sttmodelcatalog.h
    ../src/stt/stt.qrc
    ../src/theme/thememanager.cpp
    ../src/theme/thememanager.h
)

target_include_directories(kcm_uke_plasma_keyboard_custom PRIVATE
    ../src/stt
    ../src/theme
)

target_link_libraries(kcm_uke_plasma_keyboard_custom PRIVATE
    Qt::Core
    Qt::CorePrivate
    Qt::Gui
    Qt::Multimedia
    Qt::Network
    Qt::Qml
    KF6::CoreAddons
    KF6::I18n
    KF6::KCMUtilsQuick
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::GlobalAccel
)
