%global debug_package %{nil}
%global __provides_exclude_from ^%{_libdir}/%{name}/.*$
%global __requires_exclude ^lib(ggml[^()]*|transcribe[^()]*)[.]so.*$
Name: uke-plasma-keyboard-custom
Version: 0.1.0
Release: 19%{?dist}
Summary: Tablet virtual keyboard and laptop control surface for Plasma Wayland
License: GPL-2.0-or-later AND GPL-3.0-only AND LGPL-2.1-only AND LGPL-3.0-only AND BSD-2-Clause AND BSD-3-Clause AND MIT AND CC0-1.0 AND LicenseRef-CC-BY-2.0-FR AND Unicode-3.0
URL: https://github.com/MCC45TR/uke-plasma-keyboard-custom
Source0: %{name}-%{version}.tar.xz
Source1: https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v1.9.3.tar.gz#/whisper.cpp-1.9.3.tar.gz
Source2: https://github.com/handy-computer/transcribe.cpp/archive/5bb2deb2a4afb1fd50534ecb51cfcb521ef94944.tar.gz#/transcribe.cpp-5bb2deb2a4afb1fd50534ecb51cfcb521ef94944.tar.gz
BuildRequires: gcc-c++ cmake ninja-build extra-cmake-modules gettext git-core bash nodejs
BuildRequires: cmake(Qt6Core) cmake(Qt6Gui) cmake(Qt6Qml) cmake(Qt6Quick) cmake(Qt6DBus)
BuildRequires: cmake(Qt6Multimedia) cmake(Qt6Network) cmake(Qt6Concurrent) cmake(Qt6Test)
BuildRequires: cmake(Qt6VirtualKeyboard) cmake(Qt6WaylandClient) cmake(Qt6WaylandCompositor)
BuildRequires: cmake(Qt6CorePrivate) cmake(Qt6GuiPrivate) cmake(Qt6WaylandClientPrivate)
BuildRequires: cmake(KF6CoreAddons) cmake(KF6I18n) cmake(KF6KCMUtils) cmake(KF6Config)
BuildRequires: cmake(KF6Crash) cmake(KF6GlobalAccel) cmake(KF6Screen)
BuildRequires: cmake(KF6StatusNotifierItem) cmake(Qt6Widgets)
BuildRequires: cmake(KWin) cmake(PlasmaQuick) cmake(LayerShellQt)
BuildRequires: pkgconfig(wayland-client) pkgconfig(wayland-server) pkgconfig(xkbcommon)
BuildRequires: pkgconfig(epoxy) pkgconfig(libdrm) pkgconfig(vulkan)
BuildRequires: hunspell hunspell-tr hunspell-en-US xkeyboard-config
BuildRequires: wayland-protocols-devel dbus-daemon jq systemd-rpm-macros
BuildRequires: gdb-headless
BuildRequires: desktop-file-utils appstream
BuildRequires: kf6-kirigami kf6-kirigami-addons kf6-kitemmodels
Requires: plasma-keyboard kwin >= 6.8
Requires: kf6-kirigami kf6-kirigami-addons kf6-kitemmodels
Requires: qt6-qtvirtualkeyboard qt6-qtquickcontrols2
Requires: xkeyboard-config
Recommends: hunspell
Suggests: hunspell-tr hunspell-en-US
%{?systemd_requires}

%description
A separately named fork of plasma-keyboard-custom. Includes configurable PC
key regions, physical key sizing and a persistent keyboard/touchpad surface
for an enabled tablet display with an extended external monitor. The original
Plasma keyboard remains installed. Select this keyboard in System Settings.
Static mode requires the matching KWin integration. Hardware acceptance and
refresh qualification are recorded separately from successful builds.

%prep
%autosetup -n %{name}-%{version}
tar xf %{SOURCE1}
tar xf %{SOURCE2}

%build
%cmake -G Ninja \
 -DBUILD_TESTING=ON \
 -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
 -DKDE_INSTALL_LIBEXECDIR=%{_libexecdir} \
 -DWHISPER_CPP_SOURCE_DIR=$PWD/whisper.cpp-1.9.3 \
 -DTRANSCRIBE_CPP_SOURCE_DIR=$PWD/transcribe.cpp-5bb2deb2a4afb1fd50534ecb51cfcb521ef94944 \
 -DGGML_NATIVE=OFF -DGGML_OPENMP=OFF -DGGML_BLAS=OFF
%cmake_build

%install
%cmake_install
install -Dm644 packaging/org.uke.plasma.keyboard.settings.desktop %{buildroot}%{_datadir}/applications/org.uke.plasma.keyboard.settings.desktop
mkdir -p %{buildroot}%{_datadir}/%{name}/emoji
install -m644 data/emoji/* %{buildroot}%{_datadir}/%{name}/emoji/
# Build-only CMake metadata and developer caches are not runtime payloads.
find %{buildroot} -type f \( -name '*.qmltypes' -o -name '*.qmlc' \) -delete

%check
bash tests/source.sh
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop
appstreamcli validate --no-net %{buildroot}%{_datadir}/metainfo/org.uke.plasma.keyboard.metainfo.xml
export QML_IMPORT_PATH=%{buildroot}%{_libdir}/qt6/qml XDG_DATA_DIRS=%{buildroot}%{_datadir}:/usr/local/share:/usr/share
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QML_DISABLE_DISK_CACHE=1 PLASMA_KEYBOARD_TEST_MODE=1
export UKE_REQUIRE_TRANSCRIBE=1 PLASMA_KEYBOARD_TRANSCRIBE_LIBRARY=%{buildroot}%{_libdir}/%{name}/libtranscribe.so
# The protocol fixture uses software rendering and a private Wayland socket.
dbus-run-session -- ctest --test-dir %{_vpath_builddir} --output-on-failure --no-tests=error
! find %{buildroot} -type f -name '*.py' | read
! find %{buildroot} -type f -name '*plasma-keyboard-touchscreen.rules' | read

%preun
%systemd_user_preun uke-plasma-keyboard-lock-switcher.service

%postun
%systemd_user_postun_with_restart uke-plasma-keyboard-lock-switcher.service

%files
%license LICENSE LICENSES data/emoji/UNICODE-LICENSE.txt
%doc README.md docs
%{_bindir}/%{name}
%{_libdir}/qt6/qml/QtQuick/VirtualKeyboard/Styles/UkeBreeze/
%{_libdir}/qt6/qml/org/uke/plasma/keyboard/lib/
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_uke_plasma_keyboard_custom.so
%{_libdir}/qt6/plugins/kwin/plugins/uke_keyboard_backend.so
%{_libdir}/%{name}/
%{_libexecdir}/uke-plasma-keyboard-lock-switcher
%{_userunitdir}/uke-plasma-keyboard-lock-switcher.service
%{_datadir}/applications/org.uke.plasma.keyboard*.desktop
%{_datadir}/applications/kcm_uke_plasma_keyboard_custom.desktop
%{_datadir}/config.kcfg/ukeplasmakeyboardcustomsettings.kcfg
%{_datadir}/metainfo/org.uke.plasma.keyboard.metainfo.xml
%{_datadir}/plasma/plasmoids/org.uke.plasma.keyboard.toggle/
%{_datadir}/%{name}/
%{_datadir}/locale/*/LC_MESSAGES/kcm_uke_plasma_keyboard_custom.mo
%{_datadir}/locale/*/LC_MESSAGES/%{name}.mo

%changelog
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-19
- Add a movable laptop panel with manual tray and optional focus activation.
- Support mobile and PC cursor/word-deletion gestures with optional key previews.
- Keep local search edits scoped and place toolbar panels within the viewport.
- Guard asynchronous cover and entry changes and preserve system ELF dependencies.
- Require the complete native protocol, QML and library-loading checks in COPR.

* Wed Nov 11 2026 Uke Linux contributors - 0.1.0-7
- Introduce the separately named keyboard and native laptop control surface.
- Preserve original desktop providers and qualify native tests in COPR.
