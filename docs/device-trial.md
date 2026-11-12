# First tablet trial

Use a successful, fully checked COPR build and signed RPMs through DNF. The
current build/installation status is in acceptance.md. Do not build on the
tablet, copy binaries into the system or download speech models for this trial.

1. Keep the original plasma-keyboard installed. After DNF installation, select
   Uke Plasma Keyboard Custom manually in System Settings → Virtual Keyboard.
   Confirm ordinary text input before testing laptop mode.
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
5. Check tap, two-finger right click/scroll/pinch, double-tap drag and optional
   left/middle/right buttons. Check Ctrl/Shift plus deliberate clicks while the
   short typing guard is enabled. Three/four-finger gestures should follow the
   configured Plasma bindings once, without duplicate touchscreen gestures.
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

## Dynamic surface

Dynamic mode remains eligible with a USB keyboard. Software entry/exit on the
tablet has been checked with a temporary open-cover simulation; the interaction
checks below remain physical acceptance work.

Select Dynamic in keyboard settings. With automatic opening disabled, focusing a
text field must not open either the normal keyboard or the deck. Open/close the
deck from its tray item and shortcut; drag the handle and resize through settings.
Touch outside its visible rectangle must reach desktop content. Check keyboard,
pad, 3/4-finger gestures and modifier chords without duplicate touchscreen
shortcuts. The pointer must remain able to enter the tablet and ordinary windows
must remain where the user placed them. Refresh mode must remain unchanged.
Enable focus auto-show separately and check field activation/deactivation, manual
hide, fast focus changes, disconnect, cover close and lock during pending entry.
Repeat with a complex IME to verify that the language panel is visible above the
pad and does not cover its buttons. These are pending physical checks.
