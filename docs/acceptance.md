# Acceptance status

12 November 2026. Work in progress; no physical acceptance is claimed.

| Area | Source | Build/package | Physical |
|---|---|---|---|
| Fork and independent identity | Implemented | Native tests pass; RPM file fixes pending | Not applicable |
| Static surface and KWin routing | Initial implementation | Compile/controller tests pass; compositor integration pending | Pending |
| Physical size, key groups and settings | Initial implementation | Native scale/portrait key geometry passes | Pending |
| Touchpad gestures, buttons, typing guard | Initial implementation | QML construction passes; gestures pending | Pending |
| Language/variant selection, clipboard, emoji | Initial implementation | Registry, privacy and selected-layout tests pass | Pending |
| Fixed refresh profiles and restoration | Initial implementation | Compiles; mode/restoration qualification pending | Pending |
| Native 30–60 VRR | Existing kernel interface reviewed; gated | Not qualified | Pending |
| Dynamic laptop mode | Initial movable/tray/optional focus implementation | Controller/QML tests pass; compositor integration pending | Not tested |

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

## COPR 11310094 — real key geometry passes; protocol fixture gaps

The native scene test now passes: both QML windows stay alive, actual key-item
geometry preserves its physical target across simulated scale and fits portrait,
the toolbar stays within bounds, and the controller reports TR Q/F, English and
Arabic glyphs. Rawhide passed25 of26 CTests. These are isolated native GUI checks,
not measurements on the physical tablet. No RPM passed the complete package gate.

The newly enabled protocol fixture exposed missing fixture capabilities: it
provided neither a known unlocked ScreenSaver state nor content-type metadata,
so the production privacy guard correctly stayed closed. Its surface getter
also referred to the invisible input-panel stub instead of the visible layer
surface. The fixture now advertises a minimal layer-shell server, reports safe
synthetic text metadata on a private bus/socket, and targets actual rendered
key geometry instead of estimating row coordinates. It explicitly configures
independent Italian virtual input alongside its US physical compose keymap.
All existing assertions remain. The normal/native protocol suite is still pending.

Discovery metadata now names the Uke fork and its actual executable/maintainer
rather than identifying it as the stock KDE application. Desktop-file/AppStream
validation passes locally as text validation, with the checks added to COPR too.

## COPR 11310196 — retained protocol tests expose three failures

All three Fedora aarch64 workers compiled the app/plugin and passed26 of27
CTest entries. The remaining protocol entry passed10 of13 internal cases:
compose, dead keys, raw keys, repeat balancing and overlay navigation passed;
touching Q, the assumed three-Right navigation position, and Italian alternate
ordering failed. No RPM passed the full gate and nothing was installed.

Italian alternate data was installed under the fork's directory while the
loader still searched the original directory. The loader now matches its
actual payload. Keyboard-event routing now follows the visible PC component,
rather than treating every backend-ready session as a PC keyboard. The touch
probe ignores hidden/disabled key items, waits for settled geometry and records
bounded synthetic navigation state only on the private test path. Navigation
still must reach and commit X without forwarding its arrow/Return keys; its
initial highlight is no longer assumed to occupy one fixed position.

Dynamic laptop mode now has an initial source implementation: an eligible
session can select a movable, resizable keyboard/touchpad panel. It opens
manually from the native KDE tray by default; text-focus auto-show is optional.
The backend forwards only contacts on the keyboard's own surface and releases
input on exit/lock/disconnect. Dynamic mode never starts static pointer/window
exclusion or refresh ownership. Complex IME surfaces are raised into the deck's
allocated keyboard region instead of being hidden behind its background.
Native compilation, dynamic transitions, touch, focus and physical use are
pending. The original Fedora StatusNotifierItem/Qt Widgets providers are used.

## COPR 11310276 — duplicate QML property rejected

The three workers rejected a duplicate `z` declaration in the main QML
component. No complete test run, RPM or installation was accepted. The duplicate
is removed; the existing foreground order already keeps the language input
panel above the deck. All tests remain required.

A read-only query of the running24 kernel/session reports the disabled DSI-1
panel advertising 3200×2136 at30 and60 Hz, alongside48/50/90/120/144 Hz. UPower
reports a present, closed lid. These are advertised modes and an ineligible
closed-panel state, not actual refresh, VRR, touch or visual qualification.

## COPR 11310318 — touch and Italian alternatives pass

All three aarch64 native builds completed. Rawhide and Fedora44 passed26 of27
CTest entries; the retained protocol entry passed12 of13 cases with no skips.
Touching the rendered Q and Italian grave-first alternates now pass. The remaining
navigation check read `activeKey`, which is transient, instead of Qt's actual
navigation-highlight item. The probe now uses the same highlighted-key accessor
as the existing gamepad UI; the X-commit and no-arrow/Return-leak assertions stay.
Fedora45 reached terminal failure too; its result-log read temporarily returned
HTTP503 and needs a retry. No RPM or installation was accepted.

The next source also rejects stale initial lid replies after a close event,
handles lost/invalidated UPower state conservatively, and updates dimensions on
screen geometry/physical-size events. New native controller tests use private
session/system buses to exercise reply ordering, unknown cover metadata, cancelled
entries and dynamic mode's lack of refresh ownership. A staged speech-library
load check needs no model. RPM filtering now hides only private SONAME needs;
normal system-library requirements remain generated. These additions are pending
COPR qualification and are not physical-test claims.

## Original-provider idle baseline

12 November, before installing the fork: the original Fedora plasma-keyboard
6.8.2-1.fc46 process stayed alive across a bounded30.02-second read-only sample.
No process CPU ticks accrued; PSS was81,093 KiB, RSS157,344 KiB, with17 threads.
The lid remained closed and the internal panel disabled. These process counters
are a comparison baseline, not a hardware-wakeup or Uke-keyboard result.

The Fedora45 result log for11310318 was subsequently read successfully: it too
passed26 of27 CTest entries and12 of13 protocol cases, with only the navigation
probe failure. No test was removed and no RPM was delivered from that candidate.

## COPR 11310359 — controller test compiler flags

All three workers failed while compiling the new controller test. That target
had not inherited the production application's UTF-8 string-conversion compiler
options. It now uses the same options; its runtime assertions and private-bus
isolation remain required. No complete test run, RPM or installation was accepted.

The next source closes a normal-mode feature gap: mobile Qt keyboard layouts now
also support Space cursor dragging and left-Backspace word deletion. They cancel
the original pending key before emitting editing commands. Word deletion saves
and restores the compositor's complete modifier state, including held Shift and
Caps Lock. Both mobile and PC character previews follow the same opt-in setting.
New native protocol cases check cursor motion without a space, balanced deletion
keys, and preservation of synthetic Shift/Caps state. Source checks pass; native
and physical acceptance of these additions remain pending.

## COPR 11310429 — all application/protocol tests pass; fixture bus fix

All three Fedora aarch64 workers passed27 of28 CTest entries. The complete
Wayland fixture now passes, including mobile Space dragging without an inserted
space, balanced word-deletion keys, and restoration of held Shift/Caps state.
QML geometry/startup, dictionaries and the staged speech-library load pass too.
The remaining controller test exposed Qt's refusal to defer a same-connection
local D-Bus call. Its mock backend now owns a separate connection to the private
bus, matching the application's real cross-process transport. Entry-cancellation
cases also assert that an actual reply is pending before cancellation. No RPM
passed the full gate or reached the device.

The next source keeps cursor/word gestures, alternate characters and glide input
inside an open emoji search editor. Local editing uses Qt text boundaries and
never falls back to the external input context when a local target is invalid.
External predictions and learning are suppressed while that editor is active.
Sensitive/unknown contexts reject glide commits, and cancelled touches discard
their trace. Toolbar panels have explicit close controls and stay open while the
user types. Normal mode places panels above/below the keyboard as space permits;
laptop mode uses the touchpad area so the keys remain accessible. Native QML
checks add popup viewport bounds; a new native text test covers Unicode cursor
boundaries, word deletion, selections and invalid local targets. Qualification
of these additions and physical use remains pending.

## COPR 11310499 — all native tests pass; RPM file manifest rejects packaging

Fedora 44, Fedora 45 and Rawhide aarch64 each pass all 29 CTests. The controller
now exercises real deferred replies over separate private-bus connections.
Unicode/local-editor tests and native popup bounds pass too. RPM assembly then
rejects three missing paths: the helper used KDE's lib64/libexec default, the
service retained its original basename, and the source archive excluded docs.
No binary RPM was accepted or installed.

The next recipe explicitly selects Fedora's libexec directory, gives the service
its Uke filename and includes Markdown documentation in the source archive.
Legacy screenshots remain excluded. The QML module libraries also receive Uke
basenames, avoiding the stock package's libkeyboardlib/libbreezestyle provider
names. The private speech build inherits the recipe's GGML_NATIVE setting for a
consistent CPU baseline. The full native suite and RPM checks remain required.

## COPR 11310535 — complete tests; one unpackaged launcher

All three Fedora aarch64 workers again pass all 29 CTests with the renamed QML
libraries, corrected service/libexec paths and complete source documentation.
RPM assembly now rejects exactly one installed file: the generated KCM desktop
launcher was absent from the file list. The next recipe owns that launcher and
includes it in the existing desktop-file validation. The application code is
unchanged. No binary RPM or device installation was accepted.

The optional user-service lifecycle also uses the original distribution's
systemd removal/upgrade macros. Fresh installation does not preset or enable it;
final removal stops it, and upgrade marks an active service for restart. Actual
DNF transaction and preference-preservation checks remain pending. Rollback
instructions explicitly disable this optional policy before selecting stock.
