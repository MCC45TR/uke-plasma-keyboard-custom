# Architecture and provenance

11 November 2026. Baseline: mops1k/plasma-keyboard-custom
`c64e02591c1db3d973fc3567c62ba47cb2433a0c`. This specific keyboard fork was
explicitly requested by the user. The delta is independent RPM/application
identity, physical sizing, configurable PC keys, laptop surface isolation,
native input routing and event-driven clipboard integration. Other desktop
packages are not forked. Upstream-compatible fixes can be proposed upstream;
feature retirement requires equivalent original-package support.

The [migration inventory entry](../packaging/native-migration-component.json)
records this application exception, its exact remaining delta and upstream
return path. Its producer is in the separate keyboard COPR project, so the
main unstable project's producer list is empty for this component. Mapping an
installed source establishes inventory coverage only; it grants no retirement
or physical acceptance. KWin, Qt and Plasma Keyboard remain original packages.

## Session boundaries

Candidate35 identifies surfaces through `SurfaceInterface::client()` and the
original WaylandServer's `inputMethodConnection()`. Compositor-created socketpairs
report the compositor's PID, as documented in original6.8.2
[`ClientConnection`](https://github.com/KDE/kwin/blob/f130ab33c8e6f3b9d728aade87836086ae9a8ed4/src/wayland/clientconnection.h).
Comparing that PID with the launched keyboard child misclassified its own
window, moved it to the external monitor and missed its touch sequences. The
native connection identity is used only after the existing root-owned selected
executable, direct-parent and unique D-Bus caller checks. These checks are not
relaxed. Read-only capability fields report owned-surface count and largest
surface output/dimensions without window titles or entered content.

Candidate34 enables automatic static entry when every existing condition passes.
Each eligibility cycle has one automatic attempt. An explicit Normal keyboard
action suppresses reopening until a new cycle; native revocation cancels pending
entry, and text-focus loss does not exit static mode. The persistent surface is
bound to the selected internal Qt screen, including late window registration.

A real33 static entry exposed repeated native pointer-motion recursion. Review
of original6.8.2 `pointer_input.cpp` at the pinned revision below confirms that
PositionUpdateBlocker drains nested warp requests recursively. Pointer correction
now uses a coalesced Qt queued call, outside the filter's native stack. Output
membership follows KWin's floored-point convention; existing external positions
remain exact, and new targets lie inside the native output bounds. A failed
native correction ends static mode instead of retrying. The original compositor
package is unchanged. Geometry, deferred/coalesced work, unload cancellation and
automatic-entry/revocation cases await native37-test qualification.

Static laptop mode additionally requires no physical USB typing keyboard.
The original KWin6.8.2 InputDevice busType/sysPath and alphaNumericKeyboard
properties identify keyboards and USB receivers without treating a mouse's
media keys as a typing keyboard. Virtual devices and non-USB transports such
as the native pogo path are excluded; names and vendor allowlists do not grant
exceptions. InputRedirection deviceAdded/deviceRemoved events recompute the
state without polling. The backend independently rejects BeginStatic and
ends an active static session on attachment, using the existing release and
refresh/window restoration path. Normal and dynamic modes are unaffected.
The client queries the authenticated backend and ignores stale capability
replies after a newer device event; unknown detection blocks static entry.

Reviewed original headers:
[`core/inputdevice.h`](https://github.com/KDE/kwin/blob/f130ab33c8e6f3b9d728aade87836086ae9a8ed4/src/core/inputdevice.h),
[`backends/libinput/device.h`](https://github.com/KDE/kwin/blob/f130ab33c8e6f3b9d728aade87836086ae9a8ed4/src/backends/libinput/device.h)
and `input.cpp`, at the same original6.8.2 revision used below. GPL attribution
remains in the unchanged reference sources; this uses their existing APIs.

The keyboard owns its QML surfaces, language model and per-user preferences.
The KWin extension owns virtual input and pointer/window isolation. Its virtual
InputDevice follows the original KDE buttonrebinds pattern (David Redondo,
GPL), and its factory follows KWin's native plugin example. These interfaces
were reviewed at KDE/kwin `cefd679e046f44e637a4785077a4cb1a00124524` (Plasma/6.8).
The binary is compiled against each distribution's original KWin development
package. The plugin checks the selected input method, root-owned executable,
calling UID and unique D-Bus sender before attaching. It releases keys, buttons,
gestures and isolation on client disappearance or lock. Input is refused while
locked. No root daemon, general uinput grant or direct DRM ownership is added.

KScreen discovers output capabilities. Fixed refresh is applied through
a synchronous, partial original-KWin request containing only the internal panel's
current mode and VRR policy. It leaves the persistent desired mode, power,
position, scale, rotation and unrelated output settings unset. Fixed refresh
selection is scoped to static mode. The four settings are keep, 30, 60 and
native 30–60 VRR.
The adaptive option remains unavailable until the actual kernel range and
physical touch/cadence have been qualified; a capability flag alone is not proof.
No timer-based modesetting is used to imitate VRR. Kernel changes, if required,
join the existing cumulative unstable coordinator queue.

The refresh review uses KWin6.8.2 commit
`f130ab33c8e6f3b9d728aade87836086ae9a8ed4`,
[`core/outputconfiguration.h`](https://github.com/KDE/kwin/blob/f130ab33c8e6f3b9d728aade87836086ae9a8ed4/src/core/outputconfiguration.h),
`core/backendoutput.h`, `workspace.cpp` and `wayland/outputmanagement_v2.cpp`
(original GPL/LGPL headers retained in unchanged reference sources). Its
temporary currentMode is distinct from the user's desiredMode; the native
configuration uses Source::System. Libkscreen6.8.2 commit
`f9ca271de44ebb0e36e15e50f5ca1386d7d26b5c`,
[`waylandoutputdevice.cpp`](https://github.com/KDE/libkscreen/blob/f9ca271de44ebb0e36e15e50f5ca1386d7d26b5c/backends/kwayland/waylandoutputdevice.cpp)
and `waylandconfig.cpp` (LGPL-2.1-or-later), compares every supplied output field
with its current state. This makes a copied asynchronous graph unsuitable for
the keyboard's narrow temporary ownership. No distribution library is modified.
Release33 passes the complete36-test matrix on three Fedora aarch64 targets;
source/native checks alone do not prove physical mode or optical behavior.
KWin's same-revision `core/backendoutput.cpp` and `outputconfigurationstore.cpp`
also show that VRR policy enters persistent output state. The keyboard therefore
keeps a0600 recovery record under GenericStateLocation in a0700 keyboard-owned
directory, with file/directory synchronization. It migrates trusted older runtime
records and maintains the version1 runtime shadow for in-session downgrades.
This adds no service or runtime package. Cross-runtime recovery cases pass in
the native suite; physical restoration and actual power-cycle behavior remain
unqualified.

## Data and dependencies

- Existing layout/IME, prediction and swipe sources retain file-level licenses.
  Prediction and model data are loaded on demand. The native dictionary converter
  ports upstream's PKD1-to-AOSP conversion; the former interpreter is removed.
- whisper.cpp v1.9.3 archive SHA256
  `1650f884effba487025143bd8facd2f9fb40a83b3737a732803c67a8d659d9c0`.
  It supplies Whisper and Parakeet, MIT. Fedora's current whisper-cpp exists,
  but the retained Parakeet API needs this source revision; compatibility with
  the original shared provider is a follow-up return path.
- transcribe.cpp `5bb2deb2a4afb1fd50534ecb51cfcb521ef94944`, MIT, archive SHA256
  `c6491711a6eb3c44217118f20cdf3c9ce877d175e407c7a253a52e5c0c6a824d`.
  The optional GigaAM engine's private library is isolated from system ggml.
  No native speech models are bundled or downloaded automatically.
- Emoji: Unicode 17.0.0 `emoji/emoji-test.txt`, SHA256
  `1d8a944f88d7952f7ef7c5167fef3c67995bcae24543949710231b03a201acda`.
  Turkish search annotations: unicode-org/cldr
  `c7792db1b1f556563a94068402db2b4084a366eb`, `common/annotations/tr.xml`,
  SHA256 `903d227cefd905301157fe2f3fac20db905b173ce254586d727d5efee4570909`.
  Unicode-3.0 license is included. These are data, not executable KDE forks.

The original dictionaries remain optional Fedora providers. Clipboard history
follows Klipper and is refreshed on notifications, not periodic queries. Explicit
pins are stored in per-user keyboard configuration; sensitive fields cannot
read or change pins. Entered text is never logged.

Layout language/territory metadata comes from the original installed
`xkeyboard-config` registry, with variant overrides. XKB country codes are never
assumed to be ISO language codes. Qt's original
[QLocale language and territory APIs](https://doc.qt.io/qt-6/qlocale.html#codeToLanguage)
resolve the registry identifiers. Physical XKB keymaps retain their exact layout
and variant independently of the available Qt IME locale. When synchronization
is off, the virtual layout list and selection live only in keyboard preferences.

The control adaptor uses Qt's documented
[trailing QDBusMessage parameter](https://doc.qt.io/qt-6/qdbusdeclaringslots.html)
to authenticate callers; QDBusContext is not mixed with QDBusAbstractAdaptor.
The KWin factory checks the running original KWin version before constructing
private-API objects. It refuses a mismatch instead of pinning or downgrading the
desktop. Native touch forwarding is adapted from KWin's GPL ForwardInputFilter
at the same reviewed revision (`src/input.cpp`, `src/wayland/seat.h`), and is
restricted to the selected keyboard's surface during static mode. Lock state is
checked before forwarding; global touchscreen gestures do not also process
keyboard/pad contacts. Native pad gestures use KWin's usual gesture pipeline.

The native Wayland protocol fixture includes the unchanged MIT-style
`wlr-layer-shell-unstable-v1.xml` from KDE/layer-shell-qt revision
`034a61bc3d1dce51906803eeaa8752abff4c297c`, fileSHA256
`a6fbdac025cfce7a4d7420ed8cd667786a53c5a3a01af6f974a6eb49392cf799`.
Its original Drew DeVault copyright/license text is retained. This is a build-only
protocol fixture, not a fork or runtime rebuild of LayerShellQt. It supplies
known synthetic session/content metadata without bypassing production guards.

Dynamic mode uses the same authenticated virtual device and selected output.
Only static mode calls the pointer/window exclusion and refresh guard. Both
modes forward touches only for the keyboard's own surface, before global touch
gestures, and cancel input when that surface moves or disappears. The transparent
part of a dynamic surface has an empty input region. Its handle moves the QML
panel without changing desktop geometry. A bounded entry-generation check stops
late asynchronous replies from reopening a surface after lock/leave/topology
changes. Tray control uses the distribution's original
[KStatusNotifierItem](https://api.kde.org/kstatusnotifieritem.html), with no polling.

Mobile editing gestures use the inherited input-method-v1 context and original
Qt keyboard components. Qt's virtualKeyPress defers the actual click until
release/repeat; cancelling it before the drag therefore avoids an extra space
or delete. A temporary client-scoped Ctrl sequence restores the exact serialized
XKB depressed/latched/locked/layout state afterwards. No raw input grant or Qt
fork is required. This behavior was checked against Qt Virtual Keyboard's6.12
`src/components/Keyboard.qml`, `SpaceKey.qml` and
`src/virtualkeyboard/qvirtualkeyboardinputengine.cpp` on12 November2026.
The reviewed Qt Virtual Keyboard6.12 branch revision was
`ee95ac1db05f6b1a5ef8b309f81e47bbe1a497f8`; its original Qt/GPL license headers
remain with the reference source. Only the keyboard's own integration changes.

The QML helper/style targets use ukekeyboardlib and ukebreezestyle as their
library names as well as independent import URIs. Stock Fedora retains its
original libkeyboardlib/libbreezestyle providers. The optional lock-switcher
service has a Uke basename and uses Fedora's libexec directory; it is installed
disabled. Source archives include the acceptance and architecture documents,
while legacy reference screenshots stay in Git only. The private transcribe
build inherits GGML_NATIVE from the outer recipe; COPR requests OFF rather than
optimizing that dependency for a particular builder CPU.

The integration's metadata disables automatic loading at desktop startup.
The keyboard requests it through LoadPlugin when needed; an explicit existing
user plugin preference still takes precedence. KWin's PluginManager at the
reviewed revision's [PluginManager](https://github.com/KDE/kwin/blob/cefd679e046f44e637a4785077a4cb1a00124524/src/pluginmanager.cpp)
checks EnabledByDefault only during startup and permits an
explicit LoadPlugin request independently. This keeps the extra virtual device
and input filter absent when the user continues using the stock keyboard.

Native module filenames and their internal plugin IDs include the RPM build
identity. The D-Bus interface and user-visible integration name stay stable.
Qt's default [PreventUnloadHint](https://doc.qt.io/qt-6/qpluginloader.html#loadHints-prop)
retains library code in the compositor after an object is removed. On keyboard
startup, only loaded modules in the Uke integration's namespace are unloaded
before the installed build is loaded. Its new filename avoids reusing a previous
package's mapped code. KCoreAddons derives shared-module IDs from filenames;
the deprecated explicit JSON Id is removed. Staged discovery is checked through
the original KPluginMetaData API rather than a mock filename assumption.
The exact running KWin version is still checked before constructing the backend.
No compositor restart, desktop-policy setter or root session service is added.

The client also verifies that the backend's bus connection belongs to its
compositor parent and sends native requests only to that unique connection.
Changing the public service name cannot redirect already authenticated input.
Late attachment/lock replies are discarded after an ownership generation change.
