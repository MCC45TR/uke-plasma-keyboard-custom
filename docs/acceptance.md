# Acceptance status

12 November 2026. Work in progress. The user confirms tablet-only placement,
persistent keyboard/touchpad visibility and basic pointer movement, clicks and
typing at35. Release39 implements frame-independent small motion, optional
acceleration, a borderless Kirigami surface and optional idle pixel shifting.
Its physical feel and new appearance await user feedback.
The delivered checkpoint is 0.1.0-39 / COPR11317837, with 40 passing CTests on
Fedora 44/45/Rawhide aarch64 and an actual DNF upgrade from35. Reinstallation and
automatic provider renewal are separately verified at the earlier28 checkpoint.
The chronological sections below retain earlier failures and superseded states.

Current blocker found after33 delivery: a real static entry with the pointer
on the tablet crashed the compositor through repeated pointer-motion recursion.
The wrapper restarted KWin; the selected keyboard provider was restored. The
earlier simulated-open trials did not qualify this pointer path. Candidate34
defers/coalesces pointer corrections outside KWin's recursive input queue,
uses stable output-interior targets and stops static mode if confinement fails.
It also implements the user's clarified automatic-entry default and applies
active laptop state when its window registers late. All37 tests, signatures,
DNF delivery and automatic entry pass at34 without another compositor restart.
The actual surface still lands on DP-1, confirmed by the user and bounded native
window geometry, despite the backend's DSI-1 selection. The visible window's peer
PID is the compositor PID because KWin created the inherited socketpair.
Release35 uses the exact selected Wayland connection for surface ownership,
window exclusion and touch routing. A native test creates two real original-KWin
connections with identical peer PIDs and verifies distinct ownership. All38 tests
pass on all three targets. The signed Rawhide RPM has SHA256
`19094ceb64d4be3fe2641510b1c16d7772b3a15734174f1dc6089a2ac98aa0e6`;
two signatures, file coexistence and unchanged requirements/scriptlets pass.
The DNF upgrade needed one explicit provider-only reload after automatic renewal
did not finish; the same compositor and boot, selected input method and five
tracked preference files are preserved. Native owned-surface diagnostics and
bounded window geometry now place the full1280×854 logical surface on DSI-1.
The user confirms the keyboard, touchpad and persistence, then basic pointer,
click and typing operation. This does not qualify smoothness, advanced gestures,
held-input cleanup, hotplug, lock or refresh restoration. The isolated original
KWin35 fixture also passes owned-surface identity and unrelated-caller rejection.

| Area | Source | Build/package | Physical |
|---|---|---|---|
| Fork and independent identity | Implemented | Signed RPM, coexistence, upgrade/reinstall pass | Not applicable |
| Static surface and KWin routing | Implemented | Native connection identity, controller and authentication pass at35 | User confirms tablet-only placement, persistence and basic typing/pointer/click; extended routing open |
| USB keyboard exclusion | Physical USB typing keyboards/receivers block static mode; pogo excluded | Native classification/hotplug tests and actual open-cover software refusal pass | Physical hotplug/pogo pending |
| Physical size, key groups and settings | Initial implementation | Native scale/portrait key geometry passes | Pending |
| Touchpad gestures, buttons, typing guard | Implemented |39 passes actual QML quarter-pixel/no-frame, acceleration/precision/rate/reversal and input-order regressions | Basic movement/click at35;39 feel and extended gestures pending |
| Static pixel shifting | Optional, active only in static provider; deferred during interaction |40-test matrix and actual runtime advance/exit reset pass | Long-term panel protection and optical acceptance unmeasured |
| Language/variant selection, clipboard, emoji | Initial implementation | Registry, privacy and selected-layout tests pass | Pending |
| Fixed refresh profiles and restoration | Scoped native requests and durable recovery | 33 passes36 CTests including recovery/failure cases; physical modes/restoration open | Pending |
| Native 30–60 VRR | Operational profile incomplete; adaptive requests refused | Kernel/display/touch qualification and admission remain required | Pending |
| Dynamic laptop mode | Initial movable/tray/optional focus implementation | Native tests and actual USB-connected entry/exit with simulated open cover pass; physical interactions open | Not tested |

Tests must separately cover Fedora 44/45/Rawhide aarch64, original-package file
coexistence, install/reinstall/upgrade/removal/rollback, native plugin ABI failure,
lock, closed cover, disabled panel, mirroring, hot unplug, process disappearance,
pointer exclusion, window placement and focus. Check 1×/2×/2.65× and rotations
with a physical ruler; TR Q/F, EN, RTL and CJK; Qt/GTK/browser/terminal/XWayland;
multitouch, gestures, modifier latches and release cleanup; SELinux enforcing,
password/unknown contexts, idle CPU/wakeups/PSS and sustained memory use.

The earlier closed-cover/disabled-panel checkpoints below are historical.
During the35 physical trial both displays are enabled and the cover is open;
the application does not change those user-selected states. Earlier authorized
software simulations restored both states afterward. Kernel and boot selection remain outside this
package's delivery scope.

12 November inventory integration: the application-owned migration entry
records the explicitly requested fork, separate COPR producer and remaining
original-package return path. It is supplied for the shared migration policy;
the owning policy integrated it in commit04f24818, inspected separately.
With a private copy of that policy and a fresh complete28-producer main COPR
inventory plus installed RPM snapshot, the original jq audit reports complete
source coverage and no unknown packages/producers. The installed32 keyboard
maps to this component with no main-project producer. Source mapping alone
supplies no functional evidence; all four audit acceptance flags remain false.
No runtime package, panel state or acceptance gate changes with this record.

## Release33 — physical USB keyboard policy

12 November, source `3edc599` / COPR11316039: static entry is refused for physical
USB typing keyboards and receivers. Device attachment exits static mode through the existing input,
window and refresh cleanup. Normal/dynamic modes and non-USB pogo transport
are unaffected. All36 CTests pass on Fedora44/45/Rawhide aarch64, including the
new classification and controller hotplug, pending-entry and stale-reply cases.
The reviewed published spec is byte-equal, SHA256
`72249f28dc51f2685dcc37210f27a5540e56f1871e28ef569b533d3341195027`.
No native compilation runs on the tablet.

The signed Rawhide RPM, SHA256
`223a444f05ba7c0abcf9a1a16e3c5848e3f78d7ef90e28dc7a2b5ad35d28c51d`,
passes two signatures, header/payload digests and file coexistence checks.
Requires and scriptlets equal the accepted26 baseline. Actual DNF upgrades
only32→33; the selected provider automatically renews in the same compositor
and boot. Native build33/readiness and USB detection are observed; tracked
preferences and input-method selection are unchanged.

With33 installed, the bounded open-cover simulation and enabled DSI-1 retain
the extended DP-1. Physical USB keyboards are detected and the tray activation
request stays inactive with an explicit USB refusal. The original closed switch,
disabled panel and recorded display fields are restored afterward. The isolated
original-KWin trial also passes native33 attachment, unrelated-caller rejection
and normal-keyboard visibility. These are software checks; physical USB hotplug,
pogo attachment, typing/touch and optical/VRR acceptance remain open.

An additional trial lasting 4.72 seconds selected dynamic presentation
temporarily through the original KConfig tool. With USB keyboards still present,
the simulated-open cover and enabled internal panel, the tray request entered
dynamic mode and then returned to normal. Recorded output enable/mode/position/
scale/rotation fields stayed unchanged during dynamic mode. The exact original
configuration bytes (or original file absence), closed switch and disabled panel
were restored. All five tracked preference hashes, input-method selection,
application, compositor and boot remained unchanged. This verifies software
entry/exit only; drag, focus, touch, gestures and actual cover behavior remain
physical acceptance gates.

The user additionally authorized a bounded temporary open-cover simulation and
panel wake for software testing, followed by restoration. This does not add a
production bypass or qualify physical cover/touch behavior. Real home testing
and native30–60 VRR qualification remain open.

Before33 delivery, the installed32 baseline passed an actual bounded software
trial on the tablet: a temporary SW_LID open event, an enabled DSI-1 and native
static entry. Normal mode, the original closed switch state and disabled panel
were then restored. Recorded output enable/mode/position/scale/rotation fields,
the five tracked preference files, selected input method, boot and compositor
were unchanged afterward. The helper dropped privilege immediately after
opening the exact switch and retained only that descriptor for restoration.
This demonstrates software entry with a simulated cover, not physical touch,
held-input, optical or real-cover acceptance. No kernel/boot change was made.

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

## COPR 11310768 — first complete binary package matrix

Fedora 44, Fedora 45 and Rawhide aarch64 each pass all 29 CTests and RPM
assembly. Worker build times are 603, 563 and 581 seconds respectively. This
establishes native test and packaging acceptance, not device or DNF acceptance.
No package has been installed on the tablet.

The pre-install privacy audit found existing overlay debug messages containing
selected or pending text. The next source replaces them with event-only messages
and adds a native synthetic-input test with debug logging explicitly enabled.
It also installs the retained upstream README referenced by the package docs.
The full native suite must pass again before delivery.

## COPR 11310862 — qualified package matrix and signed Rawhide RPM

Source `f7c39c1a134607e91c043026a2e493c8ddc4fce5`, version 0.1.0-20:
Fedora 44, Fedora 45 and Rawhide aarch64 each pass all 29 CTests and RPM assembly.
The overlay regression runs with debug logging enabled and verifies that
synthetic selected/pending text does not appear in messages. Actual source-worker
revision and published recipe match the submitted inputs. Recipe SHA256:
`040bd3b45a513c408a24f38fe0f68528861135256c4a1b4ac399a0c29a4a9128`.

The Rawhide binary's header and legacy OpenPGP signatures and payload/header
digests verify against COPR key
`359e586fa7bff815105a41b9ee9b13208dfa961c` in an isolated RPM keyring.
Binary SHA256:
`9ce5f8dc53a5964eb87fdac85c6cc18f16309016b99b6a1291502c4dfefd2aee`.
The 17,589,798-byte download contains 487 paths and occupies 33,441,286 bytes
installed. No existing non-directory path overlaps, original QML-library
provider names, Python payload or input-grant udev rules were found. Scriptlets
only handle optional user-service removal and upgrade; there is no fresh-install
activation or input-method setter.

DNF resolves the exact signed RPM against the installed RPM database with only
this one addition. This read-only solve uses a separate empty DNF reason-state
directory and `--assumeno`; it is not an applied transaction. A stale metadata
cache initially could not find release20; checking the exact verified RPM avoids
confusing that observation with a missing package or dependency. The native
desktop authorization attempt found no authentication agent. Local terminal
authentication, actual DNF installation and preference-preservation checks are
still pending. No source RPM or model was stored on the tablet, and no native
compilation ran there.

First input-method selection remains manual. SELinux runtime, visible/hidden
idle measurements, static/dynamic touch and keyboard behavior, refresh
restoration, lock/disconnect/cover transitions and native30–60 VRR remain open.
Passing the package matrix does not establish physical acceptance.

## First device installation and native plugin load

Version0.1.0-20 was installed through the explicitly selected Rawhide aarch64
COPR repository. DNF added only this package and verified/imported the expected
COPR signing key. The original plasma-keyboard6.8.2 and KWin6.8.2 remain
installed. RPM verification reports no differences; all recorded keyboard,
layout and lock preferences are unchanged. The optional lock-switcher remains
disabled/inactive and SELinux remains Enforcing.

The actual KWin process successfully loaded the native plugin. Its capabilities
reported no enabled internal output, one external output and an inactive
surface, matching the closed-panel session. An unrelated client Attach request
was refused. Unloading restored the original loaded-plugin list without changing
the selected input method or outputs. This proves live ABI loading and caller
rejection, not keyboard/pad interaction.

The runtime audit also found that plugin metadata requested automatic loading
at every KWin startup. The next package changes that flag tofalse, retaining
the keyboard's existing explicit LoadPlugin request. Physical/manual input-method
and native VRR qualification remain open.

## Installed UI checks and touchpad size correction

The actual0.1.0-20 RPM also passes DNF reinstallation and a removal solve using
the real system reason-state; removal would affect only this fork. Removal was
not applied. Both settings and laptop scenes open from the installed payload
with isolated preferences and software rendering. The existing scale/portrait,
Turkish Q/F, English and Arabic geometry assertions pass on the tablet. This
executes the prebuilt application and QML; no native build runs on the device.
A bounded recent audit query found no keyboard-process AVC records. Physical
interaction and wider SELinux acceptance remain separate.

Rendering the actual deck exposed a layout defect: its nested layout expanded
the touchpad to the full area regardless of the requested percentage and physical
height. The next source centers the pad in its own expanding area, applying the
chosen percentage/full width and millimetre height within available bounds.
The existing QML test now checks60%,40%, full width,45/35mm heights and horizontal
and vertical centering, including portrait. These source-QML runtime checks pass
with the installed binary and the distribution's original KDE control style.
The first preview attempt used a C++ window item without a QML engine; capturing
the deck itself resolves that fixture error. Preview renders are synthetic,
not photographs or physical acceptance evidence.

## Private original-KWin session exposed an attachment defect

A bounded virtual-output session using the installed original Fedora KWin and
keyboard starts successfully on a private bus with isolated runtime/config/data
directories. A synthetic KDialog field activates the normal text-input context.
The first five-second hidden sample records zero CPU ticks, PSS92,310KiB and
RSS160,564KiB; this is an isolated software sample, not production parity or a
hardware-wakeup measurement. All fixture processes terminate in a resource-bound
transient user scope. Missing accessibility/PipeWire services are fixture limits;
the real session and its settings are not changed.

Waiting explicitly for native attachment then exposes a real failure: the
backend reads /proc/PID/status with an atEnd guard. Qt's documented
[procfs semantics](https://doc.qt.io/qt-6/qfile.html#platform-specific-issues)
make that guard report EOF before the generated data is read. The next source
reads bounded lines until no bytes remain and still requires the exact parent,
UID, root-owned installed executable, selected desktop entry and unique sender.
A new native test checks the real process/parent relationship and rejects wrong
or invalid identities. Those native checks and a repeated original-KWin trial
are required before accepting attachment; the earlier unrelated-client denial
and successful plugin load alone did not establish it.

## COPR11311573 and live module replacement

The procfs fix passes all30 CTests and RPM assembly on Fedora44/45/Rawhide
aarch64 in COPR11311573. Before delivery, the actual KWin process is observed
holding a deleted older module mapping after the package reinstallation.
Removing the plugin object does not unmap that code with Qt's default loader.
The next recipe therefore gives the module a package-specific filename and
loads it after removing only an existing owned integration. A native private-bus
test checks that unload completes before load and targets only this plugin ID.
The first versioned-module candidate assumed an explicit metadata ID remained
authoritative; inspection of the original KCoreAddons6.31 implementation rejects
that assumption. The candidate is not installed on the device. The next source
uses filename-derived IDs, removes the deprecated JSON Id and tests actual
staged discovery through KPluginMetaData. It also unloads all owned older IDs
before loading the matching build, leaving unrelated plugins alone. No
KWin/session restart is used to work around the problem.

The real session's selected input method was independently observed changing
to Uke at04:05UTC. No setter was executed by these installation checks, and the
new selection is preserved. Earlier byte-for-byte preference checks apply to
the installation/reinstallation checkpoint; the later changed kwinrc hash is
recorded separately rather than overwritten or called unchanged.

The client binds native requests to the verified compositor parent's unique
bus connection and invalidates pending replies on ownership changes. Native
private-bus checks reject an unrelated backend and move its public alias to a
different connection to verify that requests do not follow that alias. These
new discovery/ownership checks still require the full COPR gate and installed
runtime qualification; source checks alone do not establish acceptance.

## COPR11311655 — discovery fixture rejects a build-tree shadow

All application, process-parent, controller reload, unrelated-server rejection
and unique-connection pinning checks pass. The complete result is30/31 CTests:
the discovery test resolves the correct module ID but KCoreAddons prioritizes
the executable's build-tree plugin directory over the added staged search path.
Its exact-path assertion correctly rejects that shadow and no RPM is accepted.
The next fixture passes the absolute staged namespace to the same metadata API;
it keeps both the ID and exact installed-path assertions. No gate is removed or
relaxed. Full native/package and installed-session checks remain required.

## COPR11311710 /0.1.0-26 — installed native integration works

Fedora 44, 45 and Rawhide aarch64 each pass all 31 CTests and produce RPMs.
The published source worker pins commit 10dfc2539ced61ac08a3475429f18c1536b37f11;
its published spec matches the reviewed recipe. The Rawhide binary's package
signatures, payload digests and file ownership are verified before DNF upgrades
only this package to 0.1.0-26. RPM verification is clean. Original KWin and the
original plasma-keyboard remain installed.

The existing compositor process loads the matching versioned module without
restarting the desktop. The selected Uke process reports native readiness and
the backend reports build 0.1.0-26.fc46. The open-cover requirement correctly
keeps laptop mode unavailable while the internal panel is disabled. The
resolved input-method choice survives the upgrade; kwinrc is rewritten by the
self-restart mechanism, while the other tracked preferences keep their hashes
or absence. This is semantic selection preservation, not byte-identical kwinrc.

A repeated private original-KWin virtual-output fixture keeps its lock service
enabled, with automatic locking disabled only in that private configuration.
It reports known-unlocked state, native readiness, rejects an unrelated Attach
request, and shows the normal keyboard after activation of a synthetic text
field. The resource-bounded fixture exits successfully. This closes the
earlier fixture's unknown-lock visibility limit, not physical laptop-mode
acceptance. Static/dynamic touch interaction and adaptive refresh remain open.

The actual selected hidden process is also sampled for 30.05 seconds: zero CPU
ticks, PSS 101,871 KiB, RSS 193,200 KiB, 18 threads and two context switches.
This is a bounded observation of that process in that session, not a hardware
wakeup measurement or an equivalent-environment comparison with the stock app.
The prototype screenshot is captured from installed QML with synthetic input.

## COPR11311802 /0.1.0-27 — preference race fixed, reload limit found

The restart helper now leaves an unrelated provider alone and restores its
temporary blank choice only if no intervening nonempty selection was made.
A native test runs the real helper with private configuration for the unrelated,
normal restore and intervening-choice cases. All 32 tests and RPM assembly pass
on Fedora 44, 45 and Rawhide aarch64. The actual source-worker pin and published
spec match the reviewed commit f8e0fd943a8badbab996a5fd5d2d240a9adb7256. Both
Rawhide package signatures and payload digests pass; no foreign file owners,
Python files or input grants are added. Dependencies and scriptlets match 26.
DNF upgrades only the keyboard and RPM verification passes.

The device has independently entered a new boot session before that delivery;
the current compositor predates the DNF transaction. Do not extend the earlier
PID1132 no-restart observation across that separate boot. The running old
executable remains deleted after this upgrade: versions 26 and 27 each have
size 18,444,032 bytes and timestamp 1794441600, despite distinct file digests.
The old watcher compares only size and timestamp and therefore misses this
replacement. Calling the installed preference-safe restart helper once loads
27 and restores native readiness in the current compositor, PID1165. The
resolved provider selection survives. This is an explicit provider reload, not
proof that the old automatic watcher handled the upgrade.

## COPR11315309 /0.1.0-28 — installed replacement regression passes

The event-driven watcher additionally compares the regular file's device/inode
identity. No idle polling is added. A native filesystem regression atomically
replaces a file with different contents but equal size and timestamp while the
old inode remains open, matching the observed RPM transition. It also rejects
missing and nonregular paths. All 33 CTests and RPM assembly pass independently
on Fedora 44, 45 and Rawhide aarch64. The source worker pins commit
56b262b60551880f11204556d40abc63db1f2a60 and the published spec is byte-equal
to the reviewed recipe (SHA256 8d2015ffabf995105fb5e340166a5940ab22d5e21ff07029b57ccaca4ebdf05b).
The Rawhide KWin SDK is 6.8.2, matching the actual original compositor.

The signed Rawhide RPM's SHA256 is
ecf4f9c1940bd26069540fce67236fbb2beda4ef51ece37499eb399f3d7eecd5.
Both package signatures and header/payload digests pass; 487 paths add no
foreign ownership, Python files or input grants. Dependencies and scriptlets
match the earlier installed recipe. DNF upgrades only the keyboard to 28.
The old 27 watcher also misses this equal-size/equal-time transition, so the
installed helper reloads that provider once; build28 and native readiness then
appear in the same original compositor, PID1165.

An actual DNF reinstallation now replaces the inode while keeping the binary's
size 18,444,032 bytes and timestamp 1794441600. The new watcher automatically
renews the main process from PID16702 to PID17003, without a manual helper or
compositor restart. Native build28/readiness return, RPM verification is clean,
the resolved input-method selection survives, and all five tracked preference
hash/absence states match the immediate pre-reinstallation checkpoint. These
file-hash observations apply to this checkpoint, not every earlier upgrade.
The optional lock-switcher remains disabled/inactive. A removal solve with
--assumeno removes only the Uke RPM; no removal is applied and original
plasma-keyboard remains installed.

The final stable private original-KWin fixture reports known-unlocked state,
native build28/readiness, unrelated Attach denial and normal keyboard visibility
after synthetic text activation. It exits successfully in a 768 MiB/60 second
transient scope. Its five-second hidden sample records zero CPU ticks,
PSS84,784 KiB, RSS156,912 KiB, eight threads and zero context switches. The first
overlapping fixture sample was discarded when the reinstallation restarted its
keyboard; a stable post-transaction run supplies this evidence. Missing private
accessibility/PipeWire services remain fixture limits.

The actual selected hidden process records zero CPU ticks over 30.03 seconds,
PSS108,758 KiB, RSS204,488 KiB, 16 threads and one context switch. This bounded
sample does not establish hardware-wakeup or stock-provider parity. SELinux
remains Enforcing. The bounded ausearch query did not return a recognized
result, so no audit-log absence claim is made.

The rejected, never-installed 24 build (11311632) is withdrawn after the
qualified replacement is installed. Its source commit and reviewed recipe
remain recorded. The API returns 404 and refreshed primary metadata on all
three targets excludes 24 while retaining 28. No extra rebuild or metadata
regeneration request is needed.

Static/dynamic physical gestures, ordinary lock-screen typing, refresh ownership
restoration and native adaptive 30–60 Hz cadence/touch qualification remain open.
The cover is closed and the internal panel stays disabled; laptop mode correctly
refuses entry. No panel, display layout, kernel or boot selection is changed.

## 0.1.0-29 — fixed-refresh ownership correction pending

A source audit finds that the old asynchronous KScreen path supplies a complete
copied output graph. Original libkscreen applies differences in all supplied
fields, so a concurrent user layout change could be overwritten. The old failed
30→60 transition also records the failed target before apply and leaves it as
owned after an error; exit can then discard the recovery baseline. These are
source findings, not observed physical display failures.

Candidate29 constructs a synchronous original-KWin request for the selected,
already enabled internal output, with only currentMode and vrrPolicy set.
Persistent desiredMode, geometry, power, scaling, rotation, color and other
output preferences are not populated. Mode and VRR-policy ownership are tracked
independently so an edit to one field preserves the other's baseline. The
recovery logic retains the last successfully applied ownership on failure, compares rates in millihertz and
waits for an enabled panel without enabling one. No idle timer is introduced.

Two native CTests are added: private-filesystem recovery/failure cases and
actual original-KWin request-field checks. They cover failed switching followed
by crash recovery, nearby user-selected rates, policy changes, disconnect and
mode withdrawal, untrusted recovery files, journal write failures and reentrant
exit. The expected complete matrix is35 CTests per Fedora aarch64 target;
source guards pass but native/package/device results remain pending. Installed28
and its qualified receipts remain the current delivery checkpoint. Adaptive
30–60 is still refused; no kernel/display/touch acceptance is implied.

## COPR11315526 /29 — test SDK target scope failure

All three Fedora aarch64 workers stop at CMake generation. The new native request
test cannot see KWin::kwin because the SDK was found only in the sibling plugin
directory. The source worker pins0528060b7a017806c4b62bbbd6756997ddca55b6 and its
published recipe matches (SHA256 f2a2e7b6aace1c179dce033ab3c3ea61ad0fa3c8686eef830f673d5ecc11dddc).
No new C++ compile,35-test result or RPM is accepted. Candidate30 explicitly
finds the same original KWin6.8 SDK in the test directory; no test or version
gate is removed. Installed28 remains unchanged.

## COPR11315543 /30 — strict Qt conversion failure

CMake resolves the native SDK and C++ compilation begins. All three aarch64
workers reject implicit ASCII keys in the recovery implementation when it is
compiled for the new strict Qt test target. The reviewed source6959d08 and
published speccc734621c2a23196382704cc407797a0cc289103c20feafce9670af6e031e4f4
match. No35-test/RPM/device result is accepted. Candidate31 uses explicit
QStringLiteral keys and keeps the conversion restrictions and all tests.
Installed28 remains the last qualified delivery.

## COPR11315576 /31 — native request and recovery matrix passes

Fedora44/45/Rawhide aarch64 each pass all35 CTests and RPM assembly. The actual
source pin is fdc8532 and the published spec matches SHA256
4e14d032466cad3304f5d2f311f65b10e421f55c8434387b074d5a486dc2d464.
The new recovery and original-KWin request-field tests pass. The host's original
KWin exports the new request functions. No31 RPM is installed on the tablet.

The exact KWin6.8.2 source also confirms that vrrPolicy is stored persistently,
while the old recovery journal lived only in RuntimeLocation. Candidate32 keeps
private user state across a new runtime directory and mirrors version1 runtime
state for in-session downgrade compatibility. File data and containing directory
are synchronized before a new mode is applied. Trusted legacy state migrates
before waiting for a disabled panel; recovery still never enables an output.
Additional native cases replace the runtime directory, recover an interrupted
policy after boot-mode restoration, migrate legacy state while disconnected and
reject a failed shadow write. These additions and signed delivery still require
the complete35-test matrix. Hardware/power-cycle behavior is not established by
these software fixtures. Installed28 remains the current device checkpoint.

## COPR11315666 /32 — signed delivery and private normal session pass

All three Fedora aarch64 targets pass 35/35 CTests, including cross-runtime state
recovery, legacy migration while the panel is unavailable and failed shadow
writes. Source 8a48b46f67f6b75c40adbc540bc9496eba900a0f and published recipe SHA256
487aea4b4239d6d82c8dde1a47c8478dfd3c5cc64e36859493ed8c07809609fd match.
The Rawhide KWin SDK remains 6.8.2, matching the installed original compositor.

The actual Rawhide RPM is 17,613,205 bytes, SHA256
e18b14586d14cf965dc2f106e4f8adf8ad3ab07c26dc0339785808b1f4d85e7b.
Both package signatures and payload/header digests pass. Its 487 paths have no
foreign owners, project Python or input grants; dependency/scriptlet output
matches the earlier reviewed recipe. DNF upgrades only the keyboard from 28 to 32.
The watcher automatically renews the main process 17003→48307 in the same boot
and KWin 1165 process. Native readiness/build 32 and clean RPM verification pass.
The selected provider and all five tracked preference hash/absence states match
this checkpoint. No physical refresh profile is applied and neither real
recovery record exists. The optional lock-switcher stays disabled/inactive;
SELinux stays Enforcing. A removal solve lists only this RPM; it is not applied.

A post-transaction original-KWin software fixture isolates runtime, state,
config, data, cache and D-Bus. It reports known-unlocked state, native
readiness/build 32, unrelated Attach denial and normal visibility in a synthetic
text field. The 768 MiB/60s scope exits successfully. Its 5.04 s hidden sample is
zero CPU, PSS 86,406 KiB/RSS 158,800 KiB, eight threads/zero context switches.
Missing private PipeWire/accessibility services remain fixture limits.

The real selected hidden 32 process is sampled for 30.03 s: zero CPU ticks,
PSS 106,783 KiB/RSS 202,388 KiB, 18 threads/one context switch. These are bounded
process measurements, not hardware-wakeup or stock-provider parity evidence.

No 32 reinstallation or physical rollback is claimed from these results; earlier
28 reinstallation and the new native compatibility fixtures have distinct
scopes. Static/dynamic touch/typing/focus/window/pointer behavior, physical
refresh restoration and native adaptive 30–60 remain open. No panel, layout,
refresh profile, kernel or boot setting was changed for acceptance.

## COPR11316679 /36 — touch test localization type error

Source `bbc329946535993bfc297f80cf85d25dd4049b45` and the published spec match
(SHA256 `ebc3f8ba356f412c9ce647e2e180c7052b7db3b5f4675b64adfb9fd439183db5`).
All three native workers stop compiling the new touch regression: localization
setup expects a QQmlEngine pointer, rather than QQuickView. No39-test result or
RPM is accepted, and the installed35 surface remains. Candidate37 passes the
view's engine and preserves the actual Touchpad.qml small-motion test, private
D-Bus ordering/no-reply test and all earlier gates.

## COPR11316725 /37 — isolated touch test imports unrelated resources

Sourceb292956 and published specf721d6be match. All three workers compile and
pass38/39 tests, including the full application QML smoke test and ordered
no-reply input regression. The isolated Touchpad.qml test cannot instantiate
because its recording bridge import also loads the staged application theme,
whose LocaleResolver resource belongs to the main executable. No37 RPM or
device delivery is accepted. Candidate38 protects the test-only procedural
bridge module and clears that test's staged QML import path; the full application
smoke test retains its normal resource closure. The actual touchpad logic and
every earlier test remain. Each quarter-pixel delta is now checked individually.

During a later45s installed35 trial, timing-only metadata records1827 raw touch
updates and1072 pointer calls; within-stroke median/p95 gaps are8.58/16.28ms and
11.24/24.48ms respectively. These are event intervals, not end-to-end latency
or proof that every raw event originated inside the pad. No coordinates or
entered text are recorded. The user then reports usable motion and requests
natural acceleration to cross the monitor sooner, plus pixel shifting.38 adds
a velocity-based physical-motion curve, optional coarse-timed static content
shifting and native policy/interaction regressions. All40 native tests, signed
RPM delivery and physical acceleration/pixel-shift acceptance remain pending.

## COPR11317739 /38 — recording module isolation still incomplete

Source6cfaa70 and published spec7bfe7865 match. All three targets pass39/40
tests, including acceleration/precision/rate/reversal/flat-profile checks,
bounded/deferred/reset pixel policy, controller timer/touch/button deferral,
full application QML and no-reply input ordering. The isolated touch test still
loads the application theme and cannot start; protecting the original module
name and clearing the import environment was insufficient. No38 RPM or device
delivery is accepted.39 gives the recording facade a unique URI, reads the
actual Touchpad source and replaces exactly its bridge import, leaving every
input handler unchanged. A separate temporary source context avoids the app
module/cache association. All40 gates remain required.

## COPR11317837 /39 —40 tests, signed DNF and actual runtime pass

Source `f3305066032d78ebcdcb8a3eb2f24a2357adc27c` and published spec match
(SHA256 `29df6493c0a8f4a1881f772f51c25953255c14426b37bd0687248ed4dc5e7fc5`).
All three Fedora aarch64 targets pass40/40, including the actual Touchpad QML
quarter-pixel/no-frame regression, acceleration and pixel policies, controller
interaction/timer guards, complete application QML and earlier security gates.
The signed Rawhide RPM has SHA256
`7089e0368eecf66181d38376bbb9779038905f39eebffe7f9d00561608549987`,
17,631,762 bytes. Two signatures and digests pass;487 paths coexist with original
Fedora providers, and requirements/scriptlets equal the accepted26 baseline.

Actual DNF upgrades only35→39. The selected provider renews automatically in
the same compositor and boot; five tracked preference files, input-method
selection and output enable/mode/position/scale/rotation fields are unchanged.
The installed RPM header matches the reviewed signed file and RPM verification
is clean. Native39 is active on DSI-1, with a1280×854 logical surface. Acceleration
and pixel shifting are enabled; the first(-1,-1) pixel offset is observed. The
isolated original-KWin39 fixture passes native attachment, owned-surface geometry,
unrelated-caller refusal and normal visibility; its scope exits successfully.
Private portal/accessibility/PipeWire limitations and orderly compositor shutdown
messages do not qualify or invalidate physical input.

The system later reports a closed cover, an absent internal output and
physical USB keyboards connected. Static mode exits, the shift disables and its
offset resets to(0,0), following the requested gates. No panel wake, layout
change or USB bypass is applied. User feedback on39 acceleration and the new
appearance remains pending. A45s timing-only sample has39 raw updates/33 pointer
calls with roughly33ms intervals; the activity differs from the earlier trial
and supplies no performance comparison. Advanced gestures, held-input/lock,
physical hotplug/pogo, refresh restoration, native30–60 and future-image gates
remain open. Long-term panel protection has not been measured.

## Release39 — actual same-version reinstall

An actual signed-repository DNF39→39 reinstall replaces the executable inode
without changing its NEVRA, size or reproducible timestamp. The provider renews
automatically; the same compositor/boot and all five tracked preferences plus
input-method selection are preserved. The installed header still matches signed39
and RPM verification is clean. Cover/display/USB gates remain ineligible and are
not bypassed. This qualifies current reinstall renewal, not physical input.

Candidate40 also addresses toolbar settings: its rows follow the selected order,
move actions have localized tool-specific accessible names/tooltips, and keyboard
focus is restored after moving a row. A new full-settings QML smoke case exercises
the actual action, rendered order and focus. Native/package qualification is
pending; this does not establish end-to-end screen-reader acceptance.

## COPR11317984 /40 — toolbar order and focus pass

Source53818e3 and published spec6fd18a88 match;40/40 tests and RPM assembly
pass on all three Fedora aarch64 targets. The additional full-settings scenario
activates the actual named move action, verifies the rendered order against the
preference and checks restored keyboard focus.40 is not installed by this work;
the next cumulative41 also includes the independently found rotation correction.

Candidate41 normalizes pinch angle differences across atan2's positive/negative
180-degree boundary. A small crossing must not begin a pinch, and an already
active gesture must send a small incremental angle rather than nearly a full
turn. The actual Touchpad component receives two contacts through Qt's native
window-system path to test both cases. The scale remains relative to the gesture
origin, following the original libinput gesture contract. Native41 and physical
multitouch acceptance remain pending.

## Candidate42 — automatic normal opening requires touch

The user reports mouse clicks opening the keyboard and requires touch-only
automatic activation. The existing false mouse-focus default was insufficient:
the input-context and surrounding-text paths could show the independent
layer-shell window themselves.42 asks the authenticated native backend for the
actual last touch handler before mapping it, using an asynchronous reply pinned
to the compositor parent. Obsolete replies are invalidated on context loss,
hide and lock boundaries. Missing integration refuses automatic opening.
The old mouse preference remains stored but cannot enable mouse activation.

The actual-application mock-compositor scenario now refuses non-touch context
and text updates, accepts a current touch activation, and refuses a delayed
reply after context loss. Existing static/dynamic, explicit shortcut and
compositor-owned lock policies are retained. Source/packaging guards pass;
native42, signed RPM, DNF and physical mouse/touch acceptance are pending.
The host currently selects the original Plasma Keyboard; that preference is
preserved. This fork cannot change the original provider's application logic.

Candidate43 also prevents `setKwinVisible(true)` from activating a still hidden
layer window. The QML active-window handler could otherwise show it ahead of
the asynchronous decision. The real-application regression now supplies that
compositor visibility edge while refusing touch authorization and requires the
layer surface to stay unmapped.42 is not delivered while this known route
remains; the cumulative43 native and delivery gates remain pending.

## COPR11318055 /42 — protocol fixture fails after reactivation

All three targets compile and pass39/40 CTests. The new actual-application
scenario fails because its manually sent second Wayland activation is not
flushed to the client while the test waits for its native touch query. Later
input cases then run against the hidden context.42 has no accepted RPM/device
delivery. The fixture now explicitly flushes both activation and deactivation;
the touch, stale-reply and existing input assertions remain required.

## Candidate44 — original6.8.3 source plus ordered patches

The user chose the separate Uke RPM with an original Plasma Keyboard baseline
and portable patches. The original6.8.3 archive has SHA256
`ec54e6b2d163daf5288cab20ccc7043823dc0e2a3fa431eb2499d7945fa820bb` and
matches all478 tracked files at KDE revisionce74711d. The RPM uses this archive
as Source0 and applies five ordered patches. Unchanged attributed data is a
separate asset source; integration metadata/documentation is another source.

Source checks verify patch order/digests, all production paths/content and five
asset digests. Applying the series to the pristine official archive reproduces
the reviewed expanded production tree. The interface, optional tools, laptop
and touch policies remain; the inherited SteamOS dependency floors follow the
selected original release instead. Original-only metadata/translations remain
in source and are not installed into Fedora-owned namespaces.

This is source/package preparation only. Native44, signatures/dependencies,
DNF renewal, actual mouse/touch and the wider physical/VRR/image gates remain
pending. Source43 was not submitted as a separate native build; its hidden-focus
fix and the protocol-fixture correction are cumulative in44.
