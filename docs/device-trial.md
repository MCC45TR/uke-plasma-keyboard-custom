# First tablet trial

Use a successful, fully checked COPR build and signed RPMs through DNF. The
current build/installation status is in acceptance.md. Do not build on the
tablet, copy binaries into the system or download speech models for this trial.

1. Keep the original plasma-keyboard installed. After DNF installation, select
   Uke Plasma Keyboard Custom manually in System Settings → Virtual Keyboard.
   Confirm ordinary text input before testing laptop mode.
   In normal mode, hide the keyboard and click a synthetic text field with a
   mouse: it must stay hidden. Touch the field: it should open. Repeat after a
   local hide and while the field sends updates. Check Qt, GTK, browser, terminal
   and XWayland clients separately. The explicit Meta+Shift+K action still opens
   or hides it. Automatic static mode has its separate persistent policy;
   choose Normal keyboard to suppress it during this normal-mode trial.
2. Open the cover and use Plasma's display settings to enable the touchscreen
   and an extended external monitor. Keep mirroring off. The keyboard does not
   enable or reposition displays. Laptop mode becomes available only when its
   existing requirements are met.
   For static mode, disconnect physical USB typing keyboards and USB receivers;
   the native pogo connection is excluded from this USB condition. First verify
   that a connected USB keyboard prevents static entry with an explanation.
3. In a non-private external text field, check Turkish Q/F and English. Check
   an installed RTL layout and CJK engine separately. Only input layouts should
   synchronize; the desktop interface language should stay the same. Turn off
   synchronization to test the separate virtual-language preference.
4. With automatic static entry enabled (the new default), the mode should open
   once all requirements pass. It can also be opened with the bottom button.
   The tablet should show the
   top toolbar, keyboard and lower touchpad persistently. The pointer and normal
   application windows should stay on external screens, with text focus in the
   external application. Check F/modifier/arrow/navigation/system row controls.
   Verify that the surface covers the tablet edge to edge without the former
   thin outer line. It must stay on the tablet when focus moves externally.
5. Check tap, two-finger right click/scroll/pinch, double-tap drag and optional
   left/middle/right buttons. Check Ctrl/Shift plus deliberate clicks while the
   short typing guard is enabled. Three/four-finger gestures should follow the
   configured Plasma bindings once, without duplicate touchscreen gestures.
   Compare slow, very small pointer movements with fast long strokes. Optional
   acceleration should preserve precision while reducing the finger travel
   needed to cross the external screen. Turning it off should give a flat
   response; keep the existing speed setting unless deliberately adjusting it.
6. Enable the 50 mm ruler in keyboard settings and measure it on the tablet.
   Compare 1×/2×/2.65× and portrait/landscape through user-selected Plasma display
   settings. Adjust physical panel width/height if reported dimensions are wrong.
   Keys and toolbar should fit the available viewport.
7. Check Space cursor dragging, Backspace word deletion, long-press alternatives,
   optional feedback, theme/font settings, emoji search and clipboard pins.
   Use synthetic text. Password and unknown-content fields must hide auxiliary
   content; lock must exit static mode and release held native input. Verify
   ordinary lock-screen typing through the inherited input-method path.
8. Exit, disconnect the monitor, close/reopen the cover, lock/unlock and restart
   the keyboard separately. Static mode must exit rather than enable a screen.
   Verify held keys/buttons release and temporary refresh settings restore when
   still owned. Preserve intervening user changes.
   Repeat with a USB keyboard attached while static mode is active: static mode
   must exit and release held input. Removing the last USB keyboard permits
   automatic entry again when enabled. An explicit Normal keyboard action keeps
   it closed until the next eligibility cycle. Check pogo
   separately without treating the native software classification as a physical
   accessory receipt.
9. Start with the keep refresh profile. Test advertised fixed 30/60 modes only
   after the basic trial. Adaptive 30–60 remains gated on the kernel/display/touch
   coordinator's physical qualification; do not change boot parameters here.
10. After interaction has settled, measure healthy idle with
    `bash tools/measure-idle.sh KEYBOARD_PID 30` in both visible and hidden states.
    Record CPU, PSS/RSS and scheduling counters. Context-switch counts describe
    scheduling; hardware wakeups require separate tracing. Record SELinux
    enforcing/AVC results and physical observations separately from build tests.

For rollback, leave laptop mode and confirm temporary refresh settings have
restored first. A disabled or missing panel cannot be enabled for recovery;
wait for user-selected display availability before completing that check.
Stop and disable the optional Uke lock-switcher first if you
enabled it (`systemctl --user disable --now uke-plasma-keyboard-lock-switcher.service`).
Select the original Plasma keyboard, then remove the Uke
RPM through DNF if desired. Preserve keyboard preferences and the original
provider. Future image defaults require first-session and lock-screen acceptance.

## Static pixel shifting

With pixel shifting enabled and static mode eligible, leave the surface idle
for slightly over a minute. Content may move within one physical pixel on each
axis; the opaque full-panel background must remain fixed. Repeat at fractional
scale. Keys, touchpad hit regions and the input-method rectangle must remain
aligned. While holding a touch, key or button, no step should occur; a pending
step waits until interaction ends. Exit/disable must reset the offset and stop
the timer. Normal/dynamic mode and hidden/ineligible static mode must not shift.
This checks the bounded motion policy, not long-term protection against burn-in.

## Dynamic surface

Dynamic mode remains eligible with a USB keyboard. Software entry/exit on the
tablet has been checked with a temporary open-cover simulation; the interaction
checks below remain physical acceptance work.

Select Dynamic in keyboard settings. The movable surface opens and closes only
through explicit actions, including when a legacy automatic-opening preference
was enabled. Hide the normal keyboard before checking mouse focus: it must stay
hidden. Touch focus may open the normal keyboard; the movable surface still
requires its button or tray action. Open/close it explicitly, drag the handle
and resize through settings. Focus changes must leave an opened surface visible.
Touch outside its visible rectangle must reach desktop content. Check keyboard,
pad, 3/4-finger gestures and modifier chords without duplicate touchscreen
shortcuts. The pointer must remain able to enter the tablet and ordinary windows
must remain where the user placed them. Refresh mode must remain unchanged.
Check fast focus changes, manual hide, disconnect, cover close and lock during
pending entry.
Repeat with a complex IME to verify that the language panel is visible above the
pad and does not cover its buttons. These are pending physical checks.

## Posture follow-up

Use this section after the numeric provider and fitted sensor frame have passed
their separate qualification. A successful keyboard build or private-bus test
does not establish that readiness. Keep the original Fedora keyboard available.

1. With the cover open, internal touchscreen enabled, extended external monitor
   active and USB typing keyboards disconnected, lay the tablet face up.
   Automatic static mode should open and remain visible across text-focus
   changes. The physical inclination must be within45 degrees of horizontal.
2. Raise the tablet clearly beyond45 degrees. The static surface must close and
   release held keys/buttons and owned pointer/window/refresh state. Display
   enablement, layout, scale and rotation-lock preferences must stay unchanged.
   Face-down operation must remain ineligible.
3. After closing, movement within the40–45-degree band must not reopen the
   surface. Lower it below40: it should reopen automatically if the other
   conditions still pass. Repeat in portrait and landscape. Rough position
   checks do not establish physical angle calibration at the exact boundaries.
4. Choose Normal keyboard. Focus changes must not reopen static mode. Its sensor
   lease stops; explicit entry or a display/cover/USB/lock eligibility cycle
   permits another request. A pose change alone does not cancel this deliberate
   dismissal while sensing is stopped.
5. Turn automatic static entry off and request laptop mode while the tablet is
   upright. The action should offer Cancel while waiting for posture. Cancel,
   then lay it flat: it must remain closed. A new explicit request may open it
   after a fresh valid observation. Reenable automatic entry afterward if that
   is the intended preference.
6. Repeat USB hotplug, external-display disconnect, cover close and lock/unlock
   while flat. Entry must require fresh posture again after prerequisites
   return. Record optical/input behavior separately from software status; do
   not substitute orientation strings or an injected pose for this trial.
