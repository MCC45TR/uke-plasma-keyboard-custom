# Tablet posture integration

13 November 2026. The client contract below is provisional and agreed with the
owning kernel/DT and Control workstream. No numeric provider is currently
delivered. Native client qualification is independent of provider/frame and
physical acceptance. Candidate46 must stay outside the default update repository
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

The unprivileged keyboard creates a dedicated connection per lease, subscribes
before claiming and pins the unique provider owner. It rejects stale/future
deliveries (maximum age two seconds), nonfinite/out-of-range angles and reordered
observations. A signal supersedes a delayed initial snapshot. Release closes
the connection as a crash-safe fallback; callbacks from its generation cannot
enable a later lease. Service loss and logind sleep immediately invalidate the
cached posture. Resume requires a new claim and observation. There is no client
sensor timer, raw-device access or background polling. This connection lifecycle
uses the original [Qt D-Bus API](https://doc.qt.io/qt-6/qdbusconnection.html).

Only otherwise-eligible static mode requests the lease: enabled internal touch
panel, extended external monitor, open cover, unlocked session, authenticated
backend and no USB typing keyboard/receiver. Pogo remains excluded. Automatic
static mode or explicit manual entry requests posture; Normal, dynamic mode,
loss of a prerequisite and process exit release it. The settings-only process
never claims. Normal dismissal persists until another display/cover/USB/lock
eligibility cycle or explicit entry; stopping its sensor lease does not rearm
automatic entry by itself.

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
