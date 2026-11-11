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

## COPR 11309924 — translation-marker compile failure

All three builds rejected a deprecated translation marker that is disabled by
the project's KF6 compatibility settings. Tool labels now call i18nd directly.
The keyboard-surface diagnostic did not run, so its earlier failure remains
open. No RPM or installation was accepted. Diagnostic output now also records
QML loader warnings, root-object count and last-window closure for the isolated
test fixture; no entered or clipboard text is involved.

The next source preserves the native predictor's actual XKB language while
resolving the installed Qt IME's historical locale names. Source JS checks pass
175 inherited Shift/Caps/release cases plus locale-alias/deduplication cases;
these are not native Qt or physical tests. The remote scene will additionally
check production TR Q/F, English and Arabic key labels with a private kxkbrc.
Asynchronous window configure acknowledgements update restoration ownership
only when the requested geometry is actually reached.

## COPR 11309938 — non-exported KWin helper

Native compilation completed, but the integration link rejected TouchPoint's
non-exported surface-position setter. The plugin now cancels its own active
touch sequence on keyboard surface geometry changes, releasing held input,
instead of calling a hidden symbol. The QML startup failure remains unresolved
until the next complete native test run. No RPM or device installation passed.

Live read-only KWin introspection also established the case-sensitive
`org.kde.KWin.Plugins.LoadPlugin` method and `org.kde.screensaver.AboutToLock`
signal; both call sites are corrected. Native attachment additionally requires
the installed executable to be a direct KWin child. The retained clipboard test
adds protected-field/lock/setting/pin-size cases with private temporary config.
A bounded read-only idle measurement tool and physical trial procedure are
prepared, not yet run against the installed keyboard.

Normal mode defaults to the retained phone/tablet layout; the new PC layout is
the laptop default, with an independent option to use it in normal mode. This
follows the request to preserve the existing mode alongside the new surface.
The optional preference question remains answerable without a new code change.

## COPR 11310065 — scene constructs, measurement probe incomplete

All three environments compiled and linked the application/plugin. Rawhide
passed24 of25 tests, including the expanded clipboard pin/privacy and caller
routing checks. Settings and the keyboard/pad fixture each constructed a root
object and stayed running. The scene test failed because its completion marker
was not captured; this does not establish successful geometry or visual use.
No RPM passed the package gate or reached the device.

The next probe uses the production key items' geometry directly and a native
Qt event-loop driver, forcing QML diagnostics to stderr. It still checks scale,
portrait fit, toolbar bounds and actual TR Q/F/English/Arabic key labels. Child
processes are stopped before assertions. The previous protocol-fixture exclusion
was based on an unverified EGL assumption; its source already disables hardware
integration and requests software rendering. The next recipe runs that retained
private-Wayland fixture too, with a bounded timeout and isolated XDG data/config.

One-shot modifiers are consumed after deliberate clicks, and multi-word deletion
holds its temporary Ctrl for the entire operation. Their physical touch/chord
acceptance remains open. No distribution package was forked or rebuilt here.
