# Acceptance status

11 November 2026. Work in progress; no physical acceptance is claimed.

| Area | Source | Build/package | Physical |
|---|---|---|---|
| Fork and independent identity | Implemented | COPR pending | Not applicable |
| Static surface and KWin routing | Initial implementation | Pending | Pending |
| Physical size, key groups and settings | Initial implementation | Pending | Pending |
| Touchpad gestures, buttons, typing guard | Initial implementation | Pending | Pending |
| Language/variant selection, clipboard, emoji | Initial implementation | Pending | Pending |
| Fixed refresh profiles and restoration | Initial implementation | Pending | Pending |
| Native 30–60 VRR | Existing kernel interface reviewed; gated | Not qualified | Pending |
| Dynamic laptop mode | Later phase | Not implemented | Not tested |

Tests must separately cover Fedora 44/45/Rawhide aarch64, original-package file
coexistence, install/reinstall/upgrade/removal/rollback, native plugin ABI failure,
lock, closed cover, disabled panel, mirroring, hot unplug, process disappearance,
pointer exclusion, window placement and focus. Check 1×/2×/2.65× and rotations
with a physical ruler; TR Q/F, EN, RTL and CJK; Qt/GTK/browser/terminal/XWayland;
multitouch, gestures, modifier latches and release cleanup; SELinux enforcing,
password/unknown contexts, idle CPU/wakeups/PSS and sustained memory use.

The tablet currently has its cover closed and internal panel disabled. That is
an ineligible static-mode case, not permission to enable the panel or to claim
successful touch/keyboard acceptance. Original kernel and boot selection remain
outside this package's delivery scope.

## COPR 11309616 — source worker failure

Source `20a2d674feed5eca42c6b63659dcd08ac0202eeb`. The source worker checked out
the exact revision and invoked the reviewed Makefile, then failed because its
minimal chroot did not contain `git`. No native compilation or binary RPM was
produced. The recipe now installs its explicit source tools with native DNF5.
The package-definition API also requires its actual `package_name` form field;
that producer was created as 3281444 after HTTP 400 responses, without replaying
an ambiguous operation. GitHub Actions is disabled: the token lacks workflow
scope and no SSH identity is available. Native builds remain COPR-only.

## COPR 11309623 — configuration failure

The SRPM source worker succeeded with verified dependency archives. All three
aarch64 buildroots reached CMake and found the missing explicit `epoxy` build
dependency required by the original KWin export. No binary RPM was produced.
Epoxy, DRM and Vulkan development requirements are now explicit. This corrects
the package recipe without changing a distribution provider.

The next source adds native QML startup validation, dictionary presence gates,
single-active-language caches, exact XKB variant preservation and compositor
notifications for long-touch detection. Healthy-idle clipboard, gamepad and
binary-update polling were removed. Crash recovery blocks on a pidfd and file
notifications. These remain source changes until their COPR tests pass.

## COPR 11309653 — first native compile

The original Fedora KWin 6.8.2 development closure configured in all three
chroots. Native dictionary generation, speech dependencies and Qt resource
compilation ran on COPR. The plugin then failed on a wrong Qt D-Bus call:
`QDBusMessage::setDestination` does not exist. It is replaced with the documented
`createTargetedSignal` API, keeping touch notifications unicast to the selected
input method. No binary RPM or device delivery was accepted.

The next source also gives temporary refresh ownership and a private recovery
journal to the compositor, adds active-session checks and a narrow control
adaptor. Parent input methods are not exposed over D-Bus. Native tests include
caller rejection, input-method exposure denial and QML settings startup.

## COPR 11309690 — application compile errors

The KWin plugin compiled on the original Fedora development stack. Application
compilation then rejected a const D-Bus connection passed to registerObject and
a mixed QString/QStringBuilder initializer list. Both are corrected without
removing tests or weakening caller checks. No RPM or installation was accepted.
The next recipe supplies the staged QML import path to the startup test; held
modifier transitions and event-driven gamepad highlighting are also corrected.

## COPR 11309840 — compatibility facade include

Source8ae54cb reached native compilation on all three Fedora aarch64 workers.
The compatibility shortcut facade lacked QDBusPendingCall's complete type.
Its explicit include is restored; obsolete device-file/timer-queue declarations
are removed. No RPM or installation was produced.

The next source synchronizes lock LEDs from the compositor, preserves Caps Lock
through virtual release cleanup, keeps key labels readable under Ctrl, permits
intentional modifier/touchpad chords, and restarts one typing-guard timer rather
than scheduling one callback per key. Dictionary presence uses the predictor's
own file lookup without loading dictionaries on the GUI thread. Turkish strings
are supplied for the new UI. A second QML smoke scene instantiates the actual
keyboard and touchpad alongside the existing settings-startup case. These are
pending native tests, not physical acceptance.

## COPR 11309866 — clipboard test dependency closure

The application and native integration passed their earlier compilation errors.
The retained clipboard test then exposed its missing QtGui include/link closure
following the new clipboard notification connection; its target now links QtGui
and KF6 ConfigCore. Tests are retained. No completed RPM was published.

The next source resolves locale metadata from the installed XKB registry rather
than confusing country-layout IDs with ISO language IDs. Native regression
cases cover Brazil, Slovenia, Serbia, multilingual Belgium, Turkish Q/F, variant
overrides and malformed registries. Independent virtual-language mode now owns
its private layout list and selection without following or rewriting Plasma's
physical layout. The QML smoke scene checks actual key geometry at1×/2.65× and
portrait fitting instead of testing an unused fitting helper. The toolbar wraps
in a narrow viewport. Optional native-PC click/haptic feedback remains disabled
by default and is loaded only when enabled. All new native results are pending.

## COPR 11309883 — native build complete, surface startup failure

All three original Fedora aarch64 environments compiled and staged the app and
plugin. Each passed21 of22 retained tests, including XKB metadata, clipboard,
lock/content policy, predictions, dictionaries and speech/swipe engines. Settings
startup passed. The keyboard/touchpad smoke scene exited early without output;
the package correctly failed its check gate. No installable RPM was accepted.
The next test reports process exit status and requests a native crash trace on
COPR only, with debugger auto-loading and network symbol lookup disabled.

Source review also corrected the main QML resource URI, replaced unsupported
QDBusContext use in an adaptor with the documented trailing message parameter,
and added positive/negative caller-routing checks. A KWin version guard runs
before constructing the native integration. Static touchscreen delivery to the
keyboard's own surface bypasses global touchscreen gestures, so the virtual pad
alone supplies three/four-finger actions. Lock exits/cancels native input and
uses the inherited input-method path. Physical gesture/focus/lock acceptance is
still pending. The on-tablet ruler and original theme/font/outline settings are
connected to the native PC surface; none is a physical measurement receipt.
