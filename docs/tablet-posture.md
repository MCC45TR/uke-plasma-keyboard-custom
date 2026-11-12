# Tablet posture integration

13 November 2026. The client contract below is provisional and agreed with the
owning kernel/DT and Control workstream. No numeric provider is currently
delivered. Native client qualification is independent of provider/frame and
physical acceptance. Posture candidates stay outside the default update repository
until that dependency has been qualified and delivered.

The existing Control package owns the system-bus service
`org.senemos.Uke.Motion1`, object `/org/senemos/Uke/Motion1`, interface
`org.senemos.Uke.Motion1`:

| Member | D-Bus signature | Meaning |
|---|---|---|
| Claim | out `bdt` | Known, inclination degrees, actual sample CLOCK_BOOTTIME microseconds |
| Release | no arguments | Release this unique caller's lease |
| PostureChanged | `bdt` signal | Same values, targeted only to current claimers |

Inclination is0..180 degrees between the display's outward normal and upward:
zero is face-up horizontal,90 vertical,180 face-down. An absolute dot product
would incorrectly admit face-down operation. Display rotation does not alter
this physical angle. The provider must use an explicitly admitted mount/frame;
another IIO endpoint's topology or an orientation enum is insufficient.

Claim/Release are idempotent per unique bus connection. Releasing or losing the
caller releases its resources, including a pending acquisition. Last release
leaves no sensor work. The provider must preserve other consumers' configuration
and lifecycle, validate actual sample timestamps/generations and emit unknown on
starvation, suspend, frame or generation loss. It guarantees freshness throughout
the lease even when no angle boundary changes. It may suppress unchanged values
but must report known changes and crossings of both40 and45 degrees. It must
never attach a fresh arrival timestamp to a replayed cached sample. Unknown may
carry timestamp zero when no valid sample exists. Rates, ownership and stale
deadlines still require provider qualification; this document does not admit a
hardware frame or promise a currently working API.

That freshness guarantee includes a broker that freezes while retaining its bus
name. An edge-only client cannot detect that failure by itself. Provider
qualification must demonstrate bounded unknown/owner loss through its Control
lifecycle/watchdog or an agreed lease mechanism, together with stable-angle
buffer starvation. This requirement remains open with the provider.

The unprivileged keyboard creates a dedicated connection per lease, subscribes
before claiming and pins the unique provider owner. It rejects stale/future
deliveries (maximum age two seconds), nonfinite/out-of-range angles and reordered
observations. A signal supersedes a delayed initial snapshot. Release closes
the connection as a crash-safe fallback; callbacks from its generation cannot
enable a later lease. Service loss and logind sleep immediately invalidate the
cached posture. Resume requires a new claim and observation. There is no client
sensor timer, raw-device access or background polling. This connection lifecycle
uses the original [Qt D-Bus API](https://doc.qt.io/qt-6/qdbusconnection.html).

Qt deliberately returns an empty `QDBusMessage::service()` for return/error
messages ([original6.11.2 implementation](https://code.qt.io/cgit/qt/qtbase.git/tree/src/dbus/qdbusmessage.cpp?h=v6.11.2#n583)).
The pending method call is therefore bound to its pinned unique destination,
connection, serial and owner generation, with exact reply type/signature checks.
The production system bus must enforce requested-reply policy and restrict the
provider's name ownership. Signals expose their sender and keep the explicit
unique-owner check. The private test bus reproduces original Fedora's
requested-return/error rules and verifies that an unrelated peer cannot forge
either response before the real provider replies. Its broad fixture name/call
permissions are not a production policy or an admission of the Control service.

Only otherwise-eligible static mode requests the lease: enabled internal touch
panel, extended external monitor, open cover, unlocked session, authenticated
backend and no USB typing keyboard/receiver. Pogo remains excluded. Automatic
static mode or explicit manual entry requests posture; Normal, dynamic mode,
loss of a prerequisite and process exit release it. The settings-only process
never claims. Normal dismissal persists until another display/cover/USB/lock
eligibility cycle or explicit entry; stopping its sensor lease does not rearm
automatic entry by itself.

The button and tray permit a manual request before posture is known. While
waiting for posture or compositor entry they offer Cancel; cancellation releases
the lease and held input, and a delayed reply cannot reopen the surface. A
refused manual compositor entry releases its sensor demand. A refused sensor
claim is retried only by an explicit action or a provider ownership change,
without idle retries. The settings process mirrors pending state over the
existing keyboard control interface.

Policy starts unknown. A first valid angle up to45 admits static mode. Above45
revokes active/pending entry, releases held input and owned refresh/window/pointer
state, and waits strictly below40 before automatic reentry. Unknown revokes
entry without discarding this hysteresis. The application does not enable,
disable, reposition or rotate a display. Dynamic mode uses manual actions only,
even if an older configuration contains dynamicAutoShow=true.

The standard SensorProxy tilt enums use20/70-degree boundaries, so they cannot
establish this45/40 requirement. Current native gravity lacks an admitted mount
matrix and its cached replay timestamp path remains under owner review. Kernel
and DT own frame admission, timestamps and hardware lifetime; Control owns the
leased integration; the keyboard owns desktop eligibility and thresholds. No
new bridge RPM, userspace SSC protocol implementation or broad device grant is
introduced by the client.
