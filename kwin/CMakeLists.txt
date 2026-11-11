# SPDX-License-Identifier: BSD-2-Clause
find_package(KWin REQUIRED COMPONENTS kwin)
kcoreaddons_add_plugin(uke_keyboard_backend INSTALL_NAMESPACE "kwin/plugins")
target_sources(uke_keyboard_backend PRIVATE backend.cpp)
target_include_directories(uke_keyboard_backend PRIVATE ../src/laptop)
target_link_libraries(uke_keyboard_backend PRIVATE KWin::kwin Qt::DBus KF6::ConfigCore)
if(KWin_VERSION VERSION_GREATER_EQUAL 6.8)
    target_compile_definitions(uke_keyboard_backend PRIVATE UKE_KWIN_68=1)
endif()
