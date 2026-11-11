# SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
# SPDX-License-Identifier: GPL-2.0-or-later

# The style is installed under a unique name so that it can coexist with the
# Breeze style shipped by the official plasma-keyboard package and with the
# PlasmaBreeze style of our own package. Qt Virtual Keyboard resolves the style
# by scanning the QML import paths and would otherwise prefer the
# system-installed one, which would make the local build show the system style.
ecm_add_qml_module(breezestyle URI QtQuick.VirtualKeyboard.Styles.UkeBreeze GENERATE_PLUGIN_SOURCE DEPENDENCIES QtQuick)

ecm_target_qml_sources(breezestyle SOURCES
    breeze/style.qml
)

target_link_libraries(breezestyle
    PUBLIC
        Qt::Core
    PRIVATE
        Qt::Qml
        Qt::Quick
)

ecm_finalize_qml_module(breezestyle)
