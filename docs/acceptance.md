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
