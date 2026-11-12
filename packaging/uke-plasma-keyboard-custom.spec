%global debug_package %{nil}
%global __provides_exclude_from ^%{_libdir}/%{name}/.*$
%global __requires_exclude ^lib(ggml[^()]*|transcribe[^()]*)[.]so.*$
Name: uke-plasma-keyboard-custom
Version: 0.1.0
Release: 43%{?dist}
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
 -DUKE_PLUGIN_BUILD_ID=%{version}-%{release} \
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
export UKE_KEYBOARD_PLUGIN_ROOT=%{buildroot}%{_libdir}/qt6/plugins
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
%doc README.md README.upstream.md docs
%{_bindir}/%{name}
%{_libdir}/qt6/qml/QtQuick/VirtualKeyboard/Styles/UkeBreeze/
%{_libdir}/qt6/qml/org/uke/plasma/keyboard/lib/
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_uke_plasma_keyboard_custom.so
%{_libdir}/qt6/plugins/kwin/plugins/uke_keyboard_backend_*.so
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
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-43
- Keep hidden layer windows inactive until touch authorization completes
- Exercise compositor visibility edges in the actual application regression

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-42
- Require compositor-verified touch input for automatic normal keyboard opening
- Reject obsolete activation replies and test mouse focus and text updates

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-41
- Normalize pinch rotation across the positive/negative angle boundary
- Exercise two-contact boundary crossing through the actual touchpad component

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-40
- Show toolbar settings in the selected order with named move actions

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-39
- Give the isolated touch recording bridge a unique module and source context

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-38
- Add velocity-based touchpad acceleration with precise fractional movement
- Shift static content by one physical pixel only between interactions
- Isolate the recording QML bridge in the native small-motion regression

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-37
- Pass the QML engine to localization setup in the touch delivery regression

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-36
- Deliver touch updates independently of rendered frames and omit input replies
- Remove the fullscreen border and refine the Kirigami toolbar and touchpad

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-35
- Identify owned surfaces by KWin's selected Wayland input-method connection.
- Keep the keyboard on the tablet and route its touch independently of inherited peer PIDs.
- Test actual same-PID socketpair clients and report bounded surface geometry.
- Restore the topology explanation after USB detection completes.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-34
- Open persistent static mode automatically when all requirements are met.
- Keep explicit manual dismissal until the next eligibility cycle.
- Defer pointer correction outside KWin's recursive input queue and use stable interior coordinates.
- Apply an already active laptop state when its surface is registered.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-33
- Refuse static laptop mode while a physical USB keyboard is connected.
- Leave static mode on USB hotplug; preserve normal, dynamic and pogo behavior.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-32
- Keep refresh recovery in private user state across compositor and system restarts.
- Migrate and mirror version1 runtime records for supported session rollback.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-31
- Use explicit Qt string literals in the refresh recovery implementation.
- Keep strict Qt conversion checks enabled in the native recovery test.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-30
- Resolve the original KWin SDK in the native request test's CMake scope.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-29
- Scope temporary refresh requests to the native panel mode and VRR policy.
- Preserve persistent modes, concurrent display preferences and failed-switch recovery.
- Test ownership, journal failures, reentrant exit and native request field scope.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-28
- Detect replaced executables even with equal RPM timestamps and file sizes.
- Test atomic replacement while the old inode remains open.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-27
- Preserve an intervening input-method selection during keyboard self-restart.
- Leave unrelated providers untouched and test real private-config helper runs.
* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-26
- Keep the discovery gate on the staged RPM instead of build-tree precedence.

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-25
- Resolve versioned native modules using KDE's filename-derived identifiers.
- Bind native calls to the authenticated compositor's unique bus connection.
- Check staged module discovery through the original KDE metadata API.

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-24
- Give each native module build its own filename for live package updates.
- Reload only the owned integration before attaching a restarted keyboard.

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-23
- Read procfs parent identity correctly when authorizing the selected keyboard.
- Verify real process relationships without relaxing native input restrictions.

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-22
- Honor centered touchpad widths and physical heights with a full-width option.
- Exercise touchpad resizing and centering in the existing native QML checks.

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-21
- Load the KWin integration on keyboard demand instead of every desktop login.

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-20
- Keep entered and selected text out of overlay debug messages.
- Exercise text-free debug output with synthetic input in the native suite.
- Include the retained upstream README in installed documentation.

* Thu Nov 12 2026 Uke Linux contributors - 0.1.0-19
- Add a movable laptop panel with manual tray and optional focus activation.
- Support mobile and PC cursor/word-deletion gestures with optional key previews.
- Keep local search edits scoped and place toolbar panels within the viewport.
- Guard asynchronous cover and entry changes and preserve system ELF dependencies.
- Require the complete native protocol, QML and library-loading checks in COPR.

* Wed Nov 11 2026 Uke Linux contributors - 0.1.0-7
- Introduce the separately named keyboard and native laptop control surface.
- Preserve original desktop providers and qualify native tests in COPR.
